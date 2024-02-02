#!/usr/bin/bash

# Run this script to install virtio-gpu module into
# the guest Linux system

mkdir -p /lib/modules/$(uname -r)
cp ./modules/*.ko /lib/modules/$(uname -r)

depmod -a
modprobe virtio-gpu
