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

#define VBLK_FEATURES_0 0
#define VBLK_FEATURES_1 1 /* VIRTIO_F_VERSION_1 */
#define VBLK_QUEUE_NUM_MAX 1024

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
        *value = 0x1AF4;
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
        *value = 0;
        return true;
    case 24: /* InterruptStatus (R) */
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
        *value = 0;
        return true;
    default:
        printf("catched %d\n\r", addr);
        return false;
    }
}

static bool virtio_blk_reg_write(virtio_blk_state_t *vblk,
                                 uint32_t addr,
                                 uint32_t value)
{
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
        vblk->QueueSel = value;
        return true;
    case 14: /* QueueNum (W) */

        return true;
    case 17: /* QueueReady (RW) */
        return true;
    case 32: /* QueueDescLow (W) */

        return true;
    case 33: /* QueueDescHigh (W) */

        return true;
    case 36: /* QueueAvailLow (W) */

        return true;
    case 37: /* QueueAvailHigh (W) */

        return true;
    case 40: /* QueueUsedLow (W) */

        return true;
    case 41: /* QueueUsedHigh (W) */

        return true;
    case 20: /* QueueNotify (W) */
        return true;
    case 25: /* InterruptACK (W) */
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
