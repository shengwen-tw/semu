#pragma once

#include "riscv.h"
#include "virtio.h"

/* RAM */

#define RAM_SIZE (512 * 1024 * 1024)
#define DTB_SIZE (1 * 1024 * 1024)
#define INITRD_SIZE (65 * 1024 * 1024)

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

void ram_read(hart_t *core,
              uint32_t *mem,
              const uint32_t addr,
              const uint8_t width,
              uint32_t *value);

void ram_write(hart_t *core,
               uint32_t *mem,
               const uint32_t addr,
               const uint8_t width,
               const uint32_t value);

/* PLIC */

typedef struct {
    uint32_t masked;
    uint32_t ip;     /* support 32 interrupt sources only */
    uint32_t ie[32]; /* support 32 sources to 32 contexts only */
    /* state of input interrupt lines (level-triggered), set by environment */
    uint32_t active;
} plic_state_t;

void plic_update_interrupts(vm_t *vm, plic_state_t *plic);
void plic_read(hart_t *core,
               plic_state_t *plic,
               uint32_t addr,
               uint8_t width,
               uint32_t *value);
void plic_write(hart_t *core,
                plic_state_t *plic,
                uint32_t addr,
                uint8_t width,
                uint32_t value);
/* UART */

#define IRQ_UART 1
#define IRQ_UART_BIT (1 << IRQ_UART)

typedef struct {
    uint8_t dll, dlh;                  /**< divisor (ignored) */
    uint8_t lcr;                       /**< UART config */
    uint8_t ier;                       /**< interrupt config */
    uint8_t current_int, pending_ints; /**< interrupt status */
    /* other output signals, loopback mode (ignored) */
    uint8_t mcr;
    /* I/O handling */
    int in_fd, out_fd;
    bool in_ready;
} u8250_state_t;

void u8250_update_interrupts(u8250_state_t *uart);
void u8250_read(hart_t *core,
                u8250_state_t *uart,
                uint32_t addr,
                uint8_t width,
                uint32_t *value);
void u8250_write(hart_t *core,
                 u8250_state_t *uart,
                 uint32_t addr,
                 uint8_t width,
                 uint32_t value);
void u8250_check_ready(u8250_state_t *uart);
void capture_keyboard_input();

/* virtio-net */

#if SEMU_HAS(VIRTIONET)
#define IRQ_VNET 2
#define IRQ_VNET_BIT (1 << IRQ_VNET)

typedef struct {
    uint32_t QueueNum;
    uint32_t QueueDesc;
    uint32_t QueueAvail;
    uint32_t QueueUsed;
    uint16_t last_avail;
    bool ready;
    bool fd_ready;
} virtio_net_queue_t;

typedef struct {
    /* feature negotiation */
    uint32_t DeviceFeaturesSel;
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    /* queue config */
    uint32_t QueueSel;
    virtio_net_queue_t queues[2];
    /* status */
    uint32_t Status;
    uint32_t InterruptStatus;
    /* supplied by environment */
    int tap_fd;
    uint32_t *ram;
    /* implementation-specific */
    void *priv;
} virtio_net_state_t;

void virtio_net_read(hart_t *core,
                     virtio_net_state_t *vnet,
                     uint32_t addr,
                     uint8_t width,
                     uint32_t *value);
void virtio_net_write(hart_t *core,
                      virtio_net_state_t *vnet,
                      uint32_t addr,
                      uint8_t width,
                      uint32_t value);
void virtio_net_refresh_queue(virtio_net_state_t *vnet);

bool virtio_net_init(virtio_net_state_t *vnet);
#endif /* SEMU_HAS(VIRTIONET) */

/* VirtIO-Block */

#if SEMU_HAS(VIRTIOBLK)

#define IRQ_VBLK 3
#define IRQ_VBLK_BIT (1 << IRQ_VBLK)

typedef struct {
    uint32_t QueueNum;
    uint32_t QueueDesc;
    uint32_t QueueAvail;
    uint32_t QueueUsed;
    uint16_t last_avail;
    bool ready;
} virtio_blk_queue_t;

typedef struct {
    /* feature negotiation */
    uint32_t DeviceFeaturesSel;
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    /* queue config */
    uint32_t QueueSel;
    virtio_blk_queue_t queues[2];
    /* status */
    uint32_t Status;
    uint32_t InterruptStatus;
    /* supplied by environment */
    uint32_t *ram;
    uint32_t *disk;
    /* implementation-specific */
    void *priv;
} virtio_blk_state_t;

void virtio_blk_read(hart_t *vm,
                     virtio_blk_state_t *vblk,
                     uint32_t addr,
                     uint8_t width,
                     uint32_t *value);

void virtio_blk_write(hart_t *vm,
                      virtio_blk_state_t *vblk,
                      uint32_t addr,
                      uint8_t width,
                      uint32_t value);

uint32_t *virtio_blk_init(virtio_blk_state_t *vblk, char *disk_file);
#endif /* SEMU_HAS(VIRTIOBLK) */

/* VirtIO-GPU */

#if SEMU_HAS(VIRTIOGPU)

#define IRQ_VGPU 4
#define IRQ_VGPU_BIT (1 << IRQ_VGPU)

typedef struct {
    uint32_t QueueNum;
    uint32_t QueueDesc;
    uint32_t QueueAvail;
    uint32_t QueueUsed;
    uint16_t last_avail;
    bool ready;
} virtio_gpu_queue_t;

typedef struct {
    /* feature negotiation */
    uint32_t DeviceFeaturesSel;
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    /* queue config */
    uint32_t QueueSel;
    virtio_gpu_queue_t queues[2];
    /* status */
    uint32_t Status;
    uint32_t InterruptStatus;
    /* supplied by environment */
    uint32_t *ram;
    /* implementation-specific */
    void *priv;
} virtio_gpu_state_t;

void virtio_gpu_read(hart_t *vm,
                     virtio_gpu_state_t *vgpu,
                     uint32_t addr,
                     uint8_t width,
                     uint32_t *value);

void virtio_gpu_write(hart_t *vm,
                      virtio_gpu_state_t *vgpu,
                      uint32_t addr,
                      uint8_t width,
                      uint32_t value);

void semu_virgl_init(void);

void virtio_gpu_init(virtio_gpu_state_t *vgpu);
void virtio_gpu_add_scanout(virtio_gpu_state_t *vgpu,
                            uint32_t width,
                            uint32_t height);
#endif /* SEMU_HAS(VIRTIOGPU) */

/* VirtIO Input */

#if SEMU_HAS(VIRTIOINPUT)

#define IRQ_VINPUT_KEYBOARD 5
#define IRQ_VINPUT_KEYBOARD_BIT (1 << IRQ_VINPUT_KEYBOARD)

#define IRQ_VINPUT_MOUSE 6
#define IRQ_VINPUT_MOUSE_BIT (1 << IRQ_VINPUT_MOUSE)

typedef struct {
    uint32_t QueueNum;
    uint32_t QueueDesc;
    uint32_t QueueAvail;
    uint32_t QueueUsed;
    uint16_t last_avail;
    bool ready;
} virtio_input_queue_t;

typedef struct {
    /* feature negotiation */
    uint32_t DeviceFeaturesSel;
    uint32_t DriverFeatures;
    uint32_t DriverFeaturesSel;
    /* queue config */
    uint32_t QueueSel;
    virtio_input_queue_t queues[2];
    /* status */
    uint32_t Status;
    uint32_t InterruptStatus;
    /* supplied by environment */
    uint32_t *ram;
    /* implementation-specific */
    int id;  // FIXME
    void *priv;
} virtio_input_state_t;

void virtio_input_read(hart_t *vm,
                       virtio_input_state_t *vinput,
                       uint32_t addr,
                       uint8_t width,
                       uint32_t *value);

void virtio_input_write(hart_t *vm,
                        virtio_input_state_t *vinput,
                        uint32_t addr,
                        uint8_t width,
                        uint32_t value);

void virtio_input_init(virtio_input_state_t *vinput);

void virtio_input_update_key(uint32_t key, uint32_t state);

void virtio_input_update_mouse_button_state(uint32_t button, bool pressed);

void virtio_input_update_cursor(uint32_t x, uint32_t y);
#endif /* SEMU_HAS(VIRTIOINPUT) */

/* clint */
typedef struct {
    uint32_t msip[4096];
    uint64_t mtimecmp[4095];
    semu_timer_t mtime;
} clint_state_t;

void clint_update_interrupts(hart_t *vm, clint_state_t *clint);
void clint_read(hart_t *vm,
                clint_state_t *clint,
                uint32_t addr,
                uint8_t width,
                uint32_t *value);
void clint_write(hart_t *vm,
                 clint_state_t *clint,
                 uint32_t addr,
                 uint8_t width,
                 uint32_t value);

/* memory mapping */

typedef struct {
    bool stopped;
    uint32_t *ram;
    uint32_t *disk;
    plic_state_t plic;
    u8250_state_t uart;
#if SEMU_HAS(VIRTIONET)
    virtio_net_state_t vnet;
#endif
#if SEMU_HAS(VIRTIOBLK)
    virtio_blk_state_t vblk;
#endif
#if SEMU_HAS(VIRTIOGPU)
    virtio_gpu_state_t vgpu;
#endif
#if SEMU_HAS(VIRTIOINPUT)
    virtio_input_state_t vkeyboard;
    virtio_input_state_t vmouse;
#endif
    clint_state_t clint;
} emu_state_t;
