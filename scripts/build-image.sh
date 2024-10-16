#!/usr/bin/env bash

function ASSERT
{
    $*
    RES=$?
    if [ $RES -ne 0 ]; then
        echo 'Assert failed: "' $* '"'
        exit $RES
    fi
}

PASS_COLOR='\e[32;01m'
NO_COLOR='\e[0m'
function OK
{
    printf " [ ${PASS_COLOR} OK ${NO_COLOR} ]\n"
}

PARALLEL="-j$(nproc)"

function do_buildroot
{
    ASSERT git clone https://github.com/buildroot/buildroot -b 2024.05.2 --depth=1
    cp -f configs/buildroot.config buildroot/.config
    cp -f configs/busybox.config buildroot/busybox.config
    # Otherwise, the error below raises:
    #   You seem to have the current working directory in your
    #   LD_LIBRARY_PATH environment variable. This doesn't work.
    unset LD_LIBRARY_PATH
    pushd buildroot
    ASSERT make olddefconfig
    ASSERT make $PARALLEL
    popd
    cp -f buildroot/output/images/rootfs.cpio ./
}

function do_linux
{
    ASSERT git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git -b linux-6.1.y --depth=1
    mkdir -p linux/out
    cp -f configs/linux.config linux/.config
    export PATH="$PWD/buildroot/output/host/bin:$PATH"
    export CROSS_COMPILE=riscv32-buildroot-linux-gnu-
    export ARCH=riscv
    export INSTALL_MOD_PATH="out"
    pushd linux
    ASSERT make olddefconfig
    ASSERT make $PARALLEL
    ASSERT make modules_install
    cp -f arch/riscv/boot/Image ../Image
    popd
}

function do_directfb
{
    export PATH=$PATH:$PWD/buildroot/output/host/bin
    export BUILDROOT_OUT=$PWD/buildroot/output/
    mkdir -p directfb

    # Build DirectFB2
    ASSERT git clone https://github.com/directfb2/DirectFB2.git
    pushd DirectFB2
    ASSERT wget https://gist.githubusercontent.com/shengwen-tw/098da8c41ba7fbb9162ddaa4ff62b29e/raw/6bcd3adef7d4932fa4f56a18aee9ebc1e1bd0665/riscv-cross-file
    ASSERT meson -Ddrmkms=true --cross-file riscv-cross-file build/riscv
    ASSERT meson compile -C build/riscv
    DESTDIR=$BUILDROOT_OUT/host/riscv32-buildroot-linux-gnu/sysroot meson install -C build/riscv
    DESTDIR=../../../directfb meson install -C build/riscv
    popd

    # Build DirectFB2 examples
    ASSERT git clone https://github.com/directfb2/DirectFB-examples.git
    pushd DirectFB-examples/
    ASSERT wget https://gist.githubusercontent.com/shengwen-tw/098da8c41ba7fbb9162ddaa4ff62b29e/raw/6bcd3adef7d4932fa4f56a18aee9ebc1e1bd0665/riscv-cross-file
    ASSERT meson --cross-file riscv-cross-file build/riscv
    ASSERT meson compile -C build/riscv
    DESTDIR=../../../directfb meson install -C build/riscv
    popd
}

function do_directfb_gl
{
    export BUILDROOT_OUT=$PWD/buildroot/output/

    ASSERT git clone https://github.com/directfb2/DirectFBGL-EGL.git
    pushd DirectFBGL-EGL
    ASSERT wget https://gist.githubusercontent.com/shengwen-tw/098da8c41ba7fbb9162ddaa4ff62b29e/raw/6bcd3adef7d4932fa4f56a18aee9ebc1e1bd0665/riscv-cross-file
    ASSERT meson --cross-file riscv-cross-file build/riscv
    ASSERT meson compile -C build/riscv
    DESTDIR=$BUILDROOT_OUT/host/riscv32-buildroot-linux-gnu/sysroot meson install -C build/riscv
    popd
}

function do_gles2_driver
{
    ASSERT git clone https://github.com/directfb2/DirectFB2-gles2.git
    pushd DirectFB2-gles2
    ASSERT wget https://gist.githubusercontent.com/shengwen-tw/098da8c41ba7fbb9162ddaa4ff62b29e/raw/6bcd3adef7d4932fa4f56a18aee9ebc1e1bd0665/riscv-cross-file
    ASSERT meson --cross-file riscv-cross-file build/riscv
    ASSERT meson compile -C build/riscv
    popd
}

function do_yagears
{
    ASSERT git clone https://github.com/caramelli/yagears.git 
    pushd yagears
    ASSERT wget https://gist.githubusercontent.com/shengwen-tw/098da8c41ba7fbb9162ddaa4ff62b29e/raw/6bcd3adef7d4932fa4f56a18aee9ebc1e1bd0665/riscv-cross-file
    ASSERT meson --cross-file riscv-cross-file build/riscv
    ASSERT meson compile -C build/riscv
    popd    
}

do_buildroot && OK
do_linux && OK
do_directfb && OK

# Subject to change
do_directfb_gl && OK
do_gles2_driver & OK
do_yagears && OK
