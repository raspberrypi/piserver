#!/bin/bash
#
# Converts NOOBS style boot.tar.xz and root.tar.xz to image suitable for Piserver
#

set -e

if [ $# -ne 3 ]; then
    echo $0: script to convert boot.tar.xz/root.tar.xz to image suitable for PiServer
    echo
    echo Usage: $0 boot.tar.xz root.tar.xz output.tar.xz
    exit 1
fi

BOOT_TARBALL=$1
ROOT_TARBALL=$2
OUTPUT_TARBALL=$3
ARCH=$(dpkg --print-architecture)
CHROOT_SCRIPT=`dirname $0`/convert_chrooted_cmds

if [ -d work ]; then
    echo There already exists a temporary folder called 'work'
    echo Aborting.
    echo If a previous session was aborted, delete the folder manually
    exit 1
fi

if [ ! -f "$ROOT_TARBALL" ]; then
    echo $ROOT_TARBALL does not exists
    exit 1
fi

if [ ! -f "$BOOT_TARBALL" ]; then
    echo $BOOT_TARBALL does not exists
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo Script needs to be run as root
    exit 1
fi

if [ "$ARCH" != "armhf" ]; then
    command -v qemu-arm-static > /dev/null || { echo "qemu-arm-static command not found. Try: sudo apt-get install binfmt-support qemu-user-static"; exit 1; }
fi
command -v pxz > /dev/null || { echo "pxz command not found. Try: sudo apt-get install pxz"; exit 1; }

mkdir work
echo Extracting original files
tar xJf $ROOT_TARBALL -C work 
tar xJf $BOOT_TARBALL -C work/boot 

install -m 0755 "$CHROOT_SCRIPT" work/chrooted_cmds
mv work/etc/ld.so.preload work/etc/ld.so.preload.disabled
modprobe fuse

echo Installing extra software

mount -t proc none work/proc

if [ "$ARCH" == "armhf" ]; then
    chroot work /bin/bash /chrooted_cmds
else
    install -m 0755 /usr/bin/qemu-arm-static work/usr/bin/qemu-arm-static
    chroot work qemu-arm-static /bin/bash /chrooted_cmds
fi

umount work/proc

rm -f work/chrooted_cmds
rm -f work/usr/bin/qemu-arm-static
mv work/etc/ld.so.preload.disabled work/etc/ld.so.preload
rm -rf work/run/*

echo Compressing result
tar cf $OUTPUT_TARBALL -C work --use-compress-program pxz .
echo Deleting work directory
rm -rf work

echo
echo Complete!
echo Output is in $OUTPUT_TARBALL
echo
