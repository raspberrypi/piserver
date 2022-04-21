#!/bin/bash
#
# Converts NOOBS style boot.tar.xz and root.tar.xz to image suitable for Piserver
#

set -e

if [ $# -ne 3 ]; then
    echo "$0: script to convert boot.tar.xz/root.tar.xz to image suitable for PiServer"
    echo
    echo "Usage: $0 boot.tar.xz root.tar.xz output.tar.xz"
    exit 1
fi

BOOT_TARBALL="$1"
ROOT_TARBALL="$2"
OUTPUT_TARBALL="$3"
ARCH=$(dpkg --print-architecture)
CHROOT_SCRIPT=$(dirname "$0")/convert_chrooted_cmds

if [ -d work ]; then
    echo "There already exists a temporary folder called 'work'"
    echo "Aborting."
    echo "If a previous session was aborted, delete the folder manually"
    exit 1
fi

if [ ! -f "$ROOT_TARBALL" ]; then
    echo "$ROOT_TARBALL does not exists"
    exit 1
fi

if [ ! -f "$BOOT_TARBALL" ]; then
    echo "$BOOT_TARBALL does not exists"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "Script needs to be run as root"
    exit 1
fi

if [[ "$ARCH" != arm* ]]; then
    command -v qemu-arm-static > /dev/null || { echo "qemu-arm-static command not found. Try: sudo apt-get install binfmt-support qemu-user-static"; exit 1; }
fi
command -v xz > /dev/null || { echo "xz command not found. Try: sudo apt install xz"; exit 1; }

mkdir work
echo "Extracting original files"
tar xJf "$ROOT_TARBALL" -C work
tar xJf "$BOOT_TARBALL" -C work/boot

if ! chroot work true; then
    echo "Could not chroot into target environment. Please make sure binfmt is set up properly"
    echo "Deleting work directory"
    rm -rf work
    exit 1
fi

install -m 0755 "$CHROOT_SCRIPT" work/chrooted_cmds
modprobe fuse

echo "Installing extra software"

mount -t proc none work/proc

chroot work /chrooted_cmds

umount work/proc

rm -f work/chrooted_cmds
rm -rf work/run/*

echo "Compressing result"
tar cf "$OUTPUT_TARBALL" -C work --use-compress-program "xz -T0" .
echo "Deleting work directory"
rm -rf work

echo
echo Complete!
echo "Output is in $OUTPUT_TARBALL"
echo
