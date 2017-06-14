#!/bin/bash

set +e
trap 'KILL -HUP ($jobs -p)' HUP

if [ $# -lt 1 ]; then
    echo $0: script to chroot to PiServer OS folder
    echo Usage: sudo $0 \<OS folder\> [command to execute]
    exit 1
fi

ARCH=$(dpkg --print-architecture)
OSFOLDER=$1
PROG=$2
[ -z "$PROG" ] && PROG="/bin/bash"

if [ "$ARCH" != "armhf" ]; then
    command -v qemu-arm-static > /dev/null || { echo "qemu-arm-static command not found. Try: sudo apt-get install binfmt-support qemu-user-static"; exit 1; }
fi

if [ "$EUID" -ne 0 ]; then
    echo Script needs to be run as root
    exit 1
fi

if [ -n "$XAUTHORITY" ]; then
    cp "$XAUTHORITY" $OSFOLDER/root/Xauthority
    export XAUTHORITY=/root/Xauthority
fi

mv $OSFOLDER/etc/ld.so.preload $OSFOLDER/etc/ld.so.preload.disabled
mount -t proc none $OSFOLDER/proc
mount -t devpts devpts $OSFOLDER/dev/pts

if [ "$ARCH" == "armhf" ]; then
    chroot $OSFOLDER $PROG
else
    install -m 0755 /usr/bin/qemu-arm-static $OSFOLDER/usr/bin/qemu-arm-static
    chroot $OSFOLDER $PROG
fi

umount $OSFOLDER/dev/pts
umount $OSFOLDER/proc

mv $OSFOLDER/etc/ld.so.preload.disabled $OSFOLDER/etc/ld.so.preload

if [ -n "$XAUTHORITY" ]; then
    rm -f "$OSFOLDER/root/Xauthority"
fi
