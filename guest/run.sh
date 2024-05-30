#!/usr/bin/bash

# Run this script to install virtio-gpu module into
# the guest Linux system

mkdir -p /lib/modules/
cp -r ./lib/modules/* /lib/modules/

# Install DirectFB and examples
cp -r directfb/usr/local /usr/
# FIXME: This requires `source XXX.sh` command
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

modprobe virtio-gpu
