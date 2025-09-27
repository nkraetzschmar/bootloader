#!/usr/bin/env bash

qemu-system-x86_64 \
	-machine pc -cpu qemu64 -accel tcg -m 1024 \
	-nodefaults -no-reboot -nographic -serial mon:stdio \
	-drive file="$1",format=raw
