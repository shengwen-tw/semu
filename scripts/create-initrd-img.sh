#!/usr/bin/bash

IMG=ext4.img
KERNEL_VER=6.1.92
SRC=linux/out/lib/modules/$KERNEL_VER
DEST=rootfs/lib/modules/$KERNEL_VER

FILES='kernel/drivers/gpu/drm/drm.ko
       kernel/drivers/gpu/drm/drm_kms_helper.ko
       kernel/drivers/gpu/drm/drm_panel_orientation_quirks.ko
       kernel/drivers/gpu/drm/drm_shmem_helper.ko
       kernel/drivers/gpu/drm/virtio/virtio-gpu.ko
       kernel/drivers/i2c/algos/i2c-algo-bit.ko
       kernel/drivers/i2c/i2c-core.ko
       kernel/drivers/virtio/virtio_dma_buf.ko
       kernel/drivers/video/fbdev/core/cfbcopyarea.ko
       kernel/drivers/video/fbdev/core/cfbfillrect.ko
       kernel/drivers/video/fbdev/core/cfbimgblt.ko
       kernel/drivers/video/fbdev/core/fb_sys_fops.ko
       kernel/drivers/video/fbdev/core/syscopyarea.ko
       kernel/drivers/video/fbdev/core/sysfillrect.ko
       kernel/drivers/video/fbdev/core/sysimgblt.ko
       modules.dep'

mkdir -p $DEST/kernel/drivers/gpu/drm/
mkdir -p $DEST/kernel/drivers/gpu/drm/virtio/
mkdir -p $DEST/kernel/drivers/i2c/algos/
mkdir -p $DEST/kernel/drivers/virtio/
mkdir -p $DEST/kernel/drivers/video/fbdev/core/

for file in $FILES; do
    cp -f $SRC/$file $DEST/$file
done

cp -r directfb rootfs
cp guest/run.sh rootfs

# kernel objects of virtio-gpu and root files requires ~35MiB of space
dd if=/dev/zero of=${IMG} bs=4k count=30000
mkfs.ext4 -F ${IMG} -d rootfs

rm -rf rootfs

# show image size
du -h ${IMG}
