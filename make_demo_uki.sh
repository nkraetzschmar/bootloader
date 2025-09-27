#!/usr/bin/env bash

set -eufo pipefail

kernel="$1"; shift
busybox="$1"; shift
pe_inject="$1"; shift
kernel_stub="$1"; shift
pe_loader="$1"; shift
uki="$1"; shift

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

mkdir "$tmpdir/rootfs"
cp -r "$busybox/." "$tmpdir/rootfs/."
mkdir "$tmpdir/rootfs/"{proc,sys,dev,run,tmp,etc}

rm "$tmpdir/rootfs/sbin/init"
cat > "$tmpdir/rootfs/sbin/init" << 'EOF'
#!/bin/sh

set -e

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs -o mode=0755 none /run
mount -t tmpfs none /tmp

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

ukify build --stub /usr/lib/systemd/boot/efi/linuxx64.efi.stub --linux "$kernel" --initrd "$tmpdir/rootfs.cpio" --cmdline "rdinit=/sbin/init" --os-release "VERSION=0.1" -o "$tmpdir/uki_base.efi"
./pe_inject.py "$tmpdir/uki_base.efi" "$kernel_stub" "$pe_loader" "$uki"
