#!/usr/bin/bash

IMG=ext4.img

mkdir -p rootfs
mkdir -p rootfs/root
mkdir -p rootfs/modules

MODULES='linux/drivers/i2c/i2c-core.ko 
         linux/drivers/i2c/algos/i2c-algo-bit.ko 
         linux/drivers/virtio/virtio_dma_buf.ko 
         linux/drivers/gpu/drm/drm_kms_helper.ko 
         linux/drivers/gpu/drm/virtio/virtio-gpu.ko 
         linux/drivers/gpu/drm/drm.ko 
         linux/drivers/gpu/drm/drm_shmem_helper.ko 
         linux/drivers/gpu/drm/drm_panel_orientation_quirks.ko'

for file in $MODULES; do
    cp $file rootfs/modules
done

cp -r buildroot/output/target/* rootfs/root
cp drm-framebuffer/drm-framebuffer rootfs
cp scripts/run.sh rootfs

# kernel objects of virtio-gpu and root files requires ~35MiB of space
dd if=/dev/zero of=${IMG} bs=4k count=9000
mkfs.ext4 -F ${IMG} -d rootfs

rm -rf rootfs

# show image size
du -h ${IMG}
