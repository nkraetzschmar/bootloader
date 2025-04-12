#!/usr/bin/env bash

set -eufo pipefail

qemu-system-x86_64 -machine pc -cpu qemu64 -accel tcg -m 1024 -nodefaults -no-reboot -nographic -serial stdio -drive file="$1",format=raw \
| stdbuf -i0 -o0 sed 's/\x1b[\[0-9;?=]*[a-zA-Z]//g;s/\t/    /g;s/[^[:print:]]//g'
