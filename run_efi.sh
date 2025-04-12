#!/usr/bin/env bash

set -eufo pipefail

qemu-system-x86_64 -machine q35 -cpu qemu64 -accel tcg -m 1024 -nodefaults -no-reboot -nographic -serial stdio -drive if=pflash,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on -kernel "$1" \
| stdbuf -i0 -o0 sed 's/\x1b[\[0-9;?=]*[a-zA-Z]//g;s/\t/    /g;s/[^[:print:]]//g'
