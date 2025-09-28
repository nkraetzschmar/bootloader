#!/usr/bin/env bash

set -euo pipefail

machine="pc"
serial="mon:stdio"
qemu_opts=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--efi)
			machine="q35"; qemu_opts+=(-drive if=pflash,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on); shift;;
		--legacy)
			machine="pc"; shift;;
		-n|--no-monitor)
			serial="stdio"; shift;;
		--kernel)
			qemu_opts+=(-kernel "$2"); shift 2;;
		--initrd)
			qemu_opts+=(-initrd "$2"); shift 2;;
		--append)
			qemu_opts+=(-append "$2"); shift 2;;
		*)
			break;;
	esac
done

for disk in "$@"; do
	qemu_opts+=(-drive "file=$disk,format=raw")
done

exec ./term_emu.py qemu-system-x86_64 \
	-machine "$machine" -cpu qemu64 -accel tcg -m 1024 \
	-nodefaults -no-reboot -nographic -serial "$serial" \
	"${qemu_opts[@]}"
