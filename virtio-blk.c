#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include "device.h"
#include "riscv.h"
#include "riscv_private.h"

#define VIRTIO_VENDOR_ID 0x12345678

#define VIRTIO_STATUS__DRIVER_OK 4
#define VIRTIO_STATUS__DEVICE_NEEDS_RESET 64

#define VIRTIO_INT__USED_RING 1
#define VIRTIO_INT__CONF_CHANGE 2

#define VIRTIO_DESC_F_NEXT 1
#define VIRTIO_DESC_F_WRITE 2

#define VBLK_FEATURES_0 0
#define VBLK_FEATURES_1 1 /* VIRTIO_F_VERSION_1 */
#define VBLK_QUEUE_NUM_MAX 1024
#define VBLK_QUEUE (vblk->queues[vblk->QueueSel])

#define VBLK_PREPROCESS_ADDR(addr)                        \
    ((addr) < RAM_SIZE && !((addr) &0b11) ? ((addr) >> 2) \
                                          : (virtio_blk_set_fail(vblk), 0))

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_GET_ID 8
#define VIRTIO_BLK_T_GET_LIFETIME 10
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13
#define VIRTIO_BLK_T_SECURE_ERASE 14

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t status;
} __attribute__((packed));

static void virtio_blk_set_fail(virtio_blk_state_t *vblk)
{
    vblk->Status |= VIRTIO_STATUS__DEVICE_NEEDS_RESET;
    if (vblk->Status & VIRTIO_STATUS__DRIVER_OK)
        vblk->InterruptStatus |= VIRTIO_INT__CONF_CHANGE;
}

static void virtio_blk_update_status(virtio_blk_state_t *vblk, uint32_t status)
{
    vblk->Status |= status;
    if (status)
        return;

    /* Reset */
    uint32_t *ram = vblk->ram;
    memset(vblk, 0, sizeof(*vblk));
    vblk->ram = ram;
}

static int virtio_blk_iter_desc(virtio_blk_state_t *vblk,
                                virtio_blk_queue_t *queue,
                                uint32_t desc_idx)
{
    int plen = sizeof(struct virtio_blk_req);

    int i = 0;

    while (1) {
        if (desc_idx >= queue->QueueNum) {
            virtio_blk_set_fail(vblk);
            return 0;
        }

        uint32_t *desc = &vblk->ram[queue->QueueDesc + desc_idx * 4];
        uint32_t desc_addr = desc[0];
        uint16_t desc_len = desc[2];
        uint16_t desc_flags = desc[3];

        plen += desc_len;

        if (i == 0) {
            printf("[VirtIO-Block Header]\n");

            struct virtio_blk_req *vblk_rq =
                (struct virtio_blk_req *) ((char *) vblk->ram + desc_addr);
            printf(
                "* header info => &virtio_blk_req: %lu, type: %d, sector: %ld, "
                "status: %d\n",
                (uint64_t) vblk_rq, vblk_rq->type, vblk_rq->sector,
                vblk_rq->status);

            i++;
        } else if (!(desc_flags & VIRTIO_DESC_F_NEXT)) {
            printf("[Virtio-Block Foot]\n");

            uint8_t *foot_info = (uint8_t *) vblk->ram + desc_addr;
            *foot_info = VIRTIO_BLK_S_OK;
            printf("* foot info => %d\n", *foot_info);
            printf("* discriptor info => &desc: %u, len: %d,  flags: %d\n",
                   desc_addr, desc_len, desc_flags);

            break;
        } else {
            printf("[VirtIO-Block Data]\n");
            memset((char *) vblk->ram + desc_addr, 0, sizeof(char) * desc_len);
        }

        desc_idx = desc[3] >> 16;

        printf("* discriptor info => &desc: %u, len: %d,  flags: %d\n",
               desc_addr, desc_len, desc_flags);

        printf("\n");
    }

    printf("plen: %d\n", plen);
    return plen;
}

static void virtio_queue_notify_handler(virtio_blk_state_t *vblk, int index)
{
    uint32_t *ram = vblk->ram;
    virtio_blk_queue_t *queue = &vblk->queues[index];
    if (vblk->Status & VIRTIO_STATUS__DEVICE_NEEDS_RESET)
        return;

    if (!((vblk->Status & VIRTIO_STATUS__DRIVER_OK) && queue->ready))
        return virtio_blk_set_fail(vblk);

    /* check for new buffers */
    uint16_t new_avail = ram[queue->QueueAvail] >> 16;
    if (new_avail - queue->last_avail > (uint16_t) queue->QueueNum)
        return (fprintf(stderr, "size check fail\n"),
                virtio_blk_set_fail(vblk));

    if (queue->last_avail == new_avail)
        return;

    /* process them */
    uint16_t new_used = ram[queue->QueueUsed] >> 16;
    while (queue->last_avail != new_avail) {
        uint16_t queue_idx = queue->last_avail % queue->QueueNum;
        uint16_t buffer_idx = ram[queue->QueueAvail + 1 + queue_idx / 2] >>
                              (16 * (queue_idx % 2));

        int plen = virtio_blk_iter_desc(vblk, queue, buffer_idx);

        /* consume from available queue, write to used queue */
        queue->last_avail++;
        ram[queue->QueueUsed + 1 + (new_used % queue->QueueNum) * 2] =
            buffer_idx;
        ram[queue->QueueUsed + 1 + (new_used % queue->QueueNum) * 2 + 1] = plen;
        new_used++;
    }

    vblk->ram[queue->QueueUsed] &= MASK(16);
    vblk->ram[queue->QueueUsed] |= ((uint32_t) new_used) << 16;

    /* send interrupt, unless VIRTQ_AVAIL_F_NO_INTERRUPT is set */
    if (!(ram[queue->QueueAvail] & 1))
        vblk->InterruptStatus |= VIRTIO_INT__USED_RING;
}

static bool virtio_blk_reg_read(virtio_blk_state_t *vblk,
                                uint32_t addr,
                                uint32_t *value)
{
    switch (addr) {
    case 0: /* MagicValue (R) */
        *value = 0x74726976;
        return true;
    case 1: /* Version (R) */
        *value = 2;
        return true;
    case 2: /* DeviceID (R) */
        *value = 2;
        return true;
    case 3: /* VendorID (R) */
        *value = VIRTIO_VENDOR_ID;
        return true;
    case 4: /* DeviceFeatures (R) */
        *value = vblk->DeviceFeaturesSel == 0
                     ? VBLK_FEATURES_0
                     : (vblk->DeviceFeaturesSel == 1 ? VBLK_FEATURES_1 : 0);
        return true;
    case 13: /* QueueNumMax (R) */
        *value = VBLK_QUEUE_NUM_MAX;
        return true;
    case 17: /* QueueReady (RW) */
        *value = VBLK_QUEUE.ready ? 1 : 0;
        return true;
    case 24: /* InterruptStatus (R) */
        *value = vblk->InterruptStatus;
        return true;
    case 28: /* Status (RW) */
        *value = vblk->Status;
        return true;
    case 63: /* ConfigGeneration (R) */
        *value = 0;
        return true;
    case 64:
        *value = 0;
        return true;
    case 65:
        *value = 2;
        return true;
    default:
        return false;
    }
}

static bool virtio_blk_reg_write(virtio_blk_state_t *vblk,
                                 uint32_t addr,
                                 uint32_t value)
{
    printf("------[VirtIO-Block Write] addr: %d, value: %d\n", addr, value);

    switch (addr) {
    case 5: /* DeviceFeaturesSel (W) */
        vblk->DeviceFeaturesSel = value;
        return true;
    case 8: /* DriverFeatures (W) */
        vblk->DriverFeaturesSel == 0 ? (vblk->DriverFeatures = value) : 0;
        return true;
    case 9: /* DriverFeaturesSel (W) */
        vblk->DriverFeaturesSel = value;
        return true;
    case 12: /* QueueSel (W) */
        if (value < ARRAY_SIZE(vblk->queues))
            vblk->QueueSel = value;
        else
            virtio_blk_set_fail(vblk);
        return true;
    case 14: /* QueueNum (W) */
        if (value > 0 && value <= VBLK_QUEUE_NUM_MAX)
            VBLK_QUEUE.QueueNum = value;
        else
            virtio_blk_set_fail(vblk);
        return true;
    case 17: /* QueueReady (RW) */
        VBLK_QUEUE.ready = value & 1;
        if (value & 1)
            VBLK_QUEUE.last_avail = vblk->ram[VBLK_QUEUE.QueueAvail] >> 16;
        return true;
    case 32: /* QueueDescLow (W) */
        VBLK_QUEUE.QueueDesc = VBLK_PREPROCESS_ADDR(value);
        return true;
    case 33: /* QueueDescHigh (W) */
        if (value)
            virtio_blk_set_fail(vblk);
        return true;
    case 36: /* QueueAvailLow (W) */
        VBLK_QUEUE.QueueAvail = VBLK_PREPROCESS_ADDR(value);
        return true;
    case 37: /* QueueAvailHigh (W) */
        if (value)
            virtio_blk_set_fail(vblk);
        return true;
    case 40: /* QueueUsedLow (W) */
        VBLK_QUEUE.QueueUsed = VBLK_PREPROCESS_ADDR(value);
        return true;
    case 41: /* QueueUsedHigh (W) */
        if (value)
            virtio_blk_set_fail(vblk);
        return true;
    case 20: /* QueueNotify (W) */
        virtio_queue_notify_handler(vblk, value);
        return true;
    case 25: /* InterruptACK (W) */
        vblk->InterruptStatus &= ~value;
        return true;
    case 28: /* Status (RW) */
        virtio_blk_update_status(vblk, value);
        return true;
    default:
        return false;
    }
}

void virtio_blk_read(vm_t *vm,
                     virtio_blk_state_t *vblk,
                     uint32_t addr,
                     uint8_t width,
                     uint32_t *value)
{
    switch (width) {
    case RV_MEM_LW:
        if (!virtio_blk_reg_read(vblk, addr >> 2, value))
            vm_set_exception(vm, RV_EXC_LOAD_FAULT, vm->exc_val);
        break;
    case RV_MEM_LBU:
    case RV_MEM_LB:
    case RV_MEM_LHU:
    case RV_MEM_LH:
        vm_set_exception(vm, RV_EXC_LOAD_MISALIGN, vm->exc_val);
        return;
    default:
        vm_set_exception(vm, RV_EXC_ILLEGAL_INSTR, 0);
        return;
    }
}

void virtio_blk_write(vm_t *vm,
                      virtio_blk_state_t *vblk,
                      uint32_t addr,
                      uint8_t width,
                      uint32_t value)
{
    switch (width) {
    case RV_MEM_SW:
        if (!virtio_blk_reg_write(vblk, addr >> 2, value))
            vm_set_exception(vm, RV_EXC_STORE_FAULT, vm->exc_val);
        break;
    case RV_MEM_SB:
    case RV_MEM_SH:
        vm_set_exception(vm, RV_EXC_STORE_MISALIGN, vm->exc_val);
        return;
    default:
        vm_set_exception(vm, RV_EXC_ILLEGAL_INSTR, 0);
        return;
    }
}

bool virtio_blk_init(virtio_blk_state_t *vblk)
{
    return true;
}
