#!/usr/bin/env bash

set -eufo pipefail

dir="$(realpath .)"
source="$(realpath "$1")"
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
export KBUILD_BUILD_VERSION=0
export KBUILD_BUILD_USER=nobody
export KBUILD_BUILD_HOST=localhost

xz -d < "$source" | tar -x --strip-components 1

make defconfig
sed -i 's/=m/=n/; /CONFIG_RD_/ s/=y/=n/' .config

cat >> .config << EOF
CONFIG_EXPERT=y
CONFIG_MODULES=n
CONFIG_NET=n
CONFIG_CRYPTO=n
CONFIG_DRM=n
CONFIG_SOUND=n
CONFIG_VIRTUALIZATION=n
CONFIG_XZ_DEC=n
CONFIG_CMDLINE_BOOL=y
CONFIG_CMDLINE="console=ttyS0 loglevel=7 panic=-1"
EOF

make olddefconfig

make -j "$(nproc)" bzImage
cp arch/x86/boot/bzImage "$dir/kernel"
make headers_install INSTALL_HDR_PATH="$dir/kernel_headers"
