#!/usr/bin/env bash

DISK_IMG_SIZE=2k
DISK_IMG_NAME=ext4.img

dd if=/dev/zero of=$DISK_IMG_NAME bs=4k count=$DISK_IMG_SIZE
mkfs.ext4 $DISK_IMG_NAME
tune2fs -c0 -i0 $DISK_IMG_NAME
mkdir -p mnt/
sudo mount $DISK_IMG_NAME mnt/
sudo sh -c "echo \"Hello World!\" > mnt/hello.txt"
sudo umount mnt/
