#!/bin/sh

set -e

if [ $# -ne 1 ]; then
    echo $0: script to make an operating system image suitable for PiServer
    echo
    echo Usage: $0 /directory/where/os/lives
    exit 1
fi

if [ ! -d "$1" ]; then
    echo $1 is not a directory
    exit 1
fi

ARCH=$(dpkg --print-architecture)
CHROOT_SCRIPT=`dirname $0`/convert_chrooted_cmds

if [ "$EUID" -ne 0 ]; then
    echo Script needs to be run as root
    exit 1
fi

if [ "$ARCH" != "armhf" ]; then
    command -v qemu-arm-static > /dev/null || { echo "qemu-arm-static command not found. Try: sudo apt-get install binfmt-support qemu-user-static"; exit 1; }
fi

install -m 0755 "$CHROOT_SCRIPT" $1/chrooted_cmds

if [ -f "$1/etc/ld.so.preload" ]; then
    mv "$1/etc/ld.so.preload" "$1/etc/ld.so.preload.disabled"
fi

modprobe fuse

echo Installing extra software

mount -t proc none "$1/proc"

if [ "$ARCH" == "armhf" ]; then
    chroot "$1" /bin/bash /chrooted_cmds
else
    install -m 0755 /usr/bin/qemu-arm-static "$1/usr/bin/qemu-arm-static"
    chroot "$1" qemu-arm-static /bin/bash /chrooted_cmds
fi

umount "$1/proc"

rm -f "$1/chrooted_cmds"
rm -f "$1/usr/bin/qemu-arm-static"

if [ -f "$1/etc/ld.so.preload.disabled" ]; then
    mv "$1/etc/ld.so.preload.disabled" "$1/etc/ld.so.preload"
fi

rm -rf "$1"/run/*
