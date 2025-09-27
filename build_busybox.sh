#!/usr/bin/env bash

set -eufo pipefail

dir="$(realpath .)"
source="$(realpath "$1")"
headers="$(realpath "$2")"
tmp_dir=

on_exit() {
	cd "$dir"
	[ -z "$tmp_dir" ] || rm -rf "$tmp_dir"
}

trap on_exit EXIT

tmp_dir="$(mktemp -d)"
cd "$tmp_dir"

export ARCH=x86
export CROSS_COMPILE=x86_64-linux-gnu-

bzip2 -d < "$source" | tar -x --strip-components 1

make defconfig

sed -i '/CONFIG_STATIC[= ]/d;/CONFIG_TC[= ]/d' .config

cat >> .config <<EOF
CONFIG_STATIC=y
CONFIG_TC=n
EOF

make oldconfig

make -j "$(nproc)" V=1 EXTRA_CFLAGS="-I${headers}/include"

make CONFIG_PREFIX="$dir/busybox" install
touch "$dir/busybox"
