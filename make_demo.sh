#!/usr/bin/env bash

set -eufo pipefail

mbr="$1"; shift
stage2="$1"; shift
kernel="$1"; shift
busybox="$1"; shift
disk="$1"; shift

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

ls -lah "$busybox/bin/busybox"

mkdir "$tmpdir/rootfs"
cp -r "$busybox/." "$tmpdir/rootfs/."
mkdir "$tmpdir/rootfs/"{proc,sys,dev,run,tmp,efi,etc}

rm "$tmpdir/rootfs/sbin/init"
cat > "$tmpdir/rootfs/sbin/init" << 'EOF'
#!/bin/sh

set -e

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs -o mode=0755 none /run
mount -t tmpfs none /tmp
mount -t vfat -o ro /dev/sda1 /efi

ln -s /proc/self/fd /dev/fd
ln -s /proc/self/fd/0 /dev/stdin
ln -s /proc/self/fd/1 /dev/stdout
ln -s /proc/self/fd/2 /dev/stderr

echo 4 > /proc/sys/kernel/printk

exec setsid -c /bin/sh -i
EOF

cat > "$tmpdir/rootfs/etc/passwd" << EOF
root:x:0:0::/:/bin/sh
EOF

cat > "$tmpdir/rootfs/etc/group" << EOF
root:x:0:
EOF

chown -R --no-dereference root:root "$tmpdir/rootfs/."
chmod -R --no-dereference a=rX,u=rwX "$tmpdir/rootfs/."
chmod a+x "$tmpdir/rootfs/bin/busybox"
chmod a+x "$tmpdir/rootfs/sbin/init"

truncate -s 1G "$tmpdir/rootfs.ext2"
mkfs.ext2 -L ROOT -d "$tmpdir/rootfs" "$tmpdir/rootfs.ext2"

truncate -s 0 "$disk"
truncate -s 2G "$disk"

start_esp=2048
size_esp=$(( 512 * 2048 ))
start_root=$(( 513 * 2048 ))
size_root=$(( 1024 * 2048 ))

sfdisk "$disk" << EOF
label: gpt
label-id: 01234567-abcd-0123-abcd-0123456789ab
start=$start_esp, size=$size_esp, type=uefi, uuid=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee, name=EFI
start=$start_root, size=$size_root, type=linux, uuid=00000000-1111-2222-3333-444444444444, name=ROOT
EOF

dd if="$mbr" of="$disk" bs=446 count=1 conv=notrunc 2> /dev/null
dd if="$stage2" of="$disk" bs=512 count=32 seek=34 conv=notrunc 2> /dev/null
dd if="$tmpdir/rootfs.ext2" of="$disk" bs=1M seek=513 conv=notrunc 2> /dev/null

mformat -i "$disk@@2048s" -T "$size_esp" -c 2 -F -v EFI ::
mmd -i "$disk@@2048s" ::/EFI
mmd -i "$disk@@2048s" ::/EFI/BOOT
mcopy -i "$disk@@2048s" /usr/lib/systemd/boot/efi/systemd-bootx64.efi ::/EFI/BOOT/BOOTX64.EFI

mcopy -i "$disk@@2048s" "$kernel" ::/kernel
mmd -i "$disk@@2048s" ::/loader
mmd -i "$disk@@2048s" ::/loader/entries

mcopy -i "$disk@@2048s" - ::/loader/entries/example-00.conf << EOF
title   Demo
version 0.1
linux   /kernel
options loglevel=6 root=/dev/sda2 ro
EOF

mdir -i "$disk@@2048s" -/ ::
