#!/usr/bin/env bash

set -eufo pipefail

fifo=
qemu_pid=
sed_pid=

on_exit() {
	sleep 1
	[ -z "$qemu_pid" ] || kill -9 "$qemu_pid" 2> /dev/null
	[ -z "$sed_pid" ] || kill -9 "$sed_pid" 2> /dev/null
	[ -z "$fifo" ] || rm "$fifo"
}

trap on_exit EXIT

fifo="$(mktemp -u)"
mkfifo "$fifo"
stdbuf -i0 -o0 -e0 qemu-system-i386 -s -S -machine pc -cpu qemu32 -accel tcg -m 1024 -nodefaults -no-reboot -nographic -serial stdio -drive file="$1",format=raw < /dev/null 2>&1 > "$fifo" &
qemu_pid="$!"

stdbuf -i0 -o0 -e0 sed 's/\x1b[\[0-9;?]*[a-zA-Z]//g;s/[^[:print:]\t]//g' < "$fifo" 2>&1 > serial.log &
sed_pid="$!"

sleep 1
kill -0 "$qemu_pid" 2> /dev/null || { cat serial.log; exit 1; }

gdb-multiarch -x real-mode.gdbinit
