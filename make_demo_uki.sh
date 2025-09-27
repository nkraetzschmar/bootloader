#!/usr/bin/env bash

set -eufo pipefail

mbr="$1"; shift
stage2="$1"; shift
kernel="$1"; shift
busybox="$1"; shift
pe_inject="$1"; shift
kernel_stub="$1"; shift
pe_loader="$1"; shift
disk="$1"; shift

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

mkdir "$tmpdir/rootfs"
cp -r "$busybox/." "$tmpdir/rootfs/."
mkdir "$tmpdir/rootfs/"{proc,sys,dev,run,tmp,efi,etc}

rm "$tmpdir/rootfs/sbin/init"
cat > "$tmpdir/rootfs/sbin/init" << 'EOF'
#!/bin/sh

set -e

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs -o mode=0755 none /run
mount -t tmpfs none /tmp
mount -t vfat /dev/sda1 /efi

echo 4 > /proc/sys/kernel/printk

sh
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

(cd "$tmpdir/rootfs" && find . | cpio -o -H newc) > "$tmpdir/rootfs.cpio"

ukify build --stub /usr/lib/systemd/boot/efi/linuxx64.efi.stub --linux "$kernel" --initrd "$tmpdir/rootfs.cpio" --cmdline "rdinit=/sbin/init" --os-release $'PRETTY_NAME=Demo\nVERSION=0.1\n' -o "$tmpdir/uki_base.efi"
./pe_inject.py "$tmpdir/uki_base.efi" "$kernel_stub" "$pe_loader" "$tmpdir/uki.efi"

truncate -s 0 "$disk"
truncate -s 1G "$disk"

start_esp=2048
size_esp=$(( 512 * 2048 ))

sfdisk "$disk" << EOF
label: gpt
label-id: 01234567-abcd-0123-abcd-0123456789ab
start=$start_esp, size=$size_esp, type=uefi, uuid=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee, name=EFI
EOF

dd if="$mbr" of="$disk" bs=446 count=1 conv=notrunc 2> /dev/null
dd if="$stage2" of="$disk" bs=512 count=32 seek=34 conv=notrunc 2> /dev/null

mformat -i "$disk@@2048s" -T "$size_esp" -c 2 -F -v EFI ::
mmd -i "$disk@@2048s" ::/EFI
mmd -i "$disk@@2048s" ::/EFI/BOOT
mcopy -i "$disk@@2048s" /usr/lib/systemd/boot/efi/systemd-bootx64.efi ::/EFI/BOOT/BOOTX64.EFI

mmd -i "$disk@@2048s" ::/EFI/Linux
mcopy -i "$disk@@2048s" "$tmpdir/uki.efi" ::/EFI/Linux/uki.efi

mdir -i "$disk@@2048s" -/ ::
