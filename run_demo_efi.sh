#!/usr/bin/env bash

qemu-system-x86_64 \
	-machine q35 -cpu qemu64 -accel tcg -m 1024 \
	-nodefaults -no-reboot -nographic -serial mon:stdio \
	-drive if=pflash,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on \
	-drive file="$1",format=raw
