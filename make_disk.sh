#!/usr/bin/env bash

set -eufo pipefail

mbr="$1"; shift
stage2="$1"; shift
kernel="$1"; shift
initrd="$1"; shift
disk="$1"; shift

truncate -s 0 "$disk"
truncate -s 1G "$disk"
sfdisk "$disk" << EOF
label: gpt
label-id: 01234567-abcd-0123-abcd-0123456789ab
start=2048, size=1048576, type=uefi, uuid=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee, name=EFI
EOF

dd if="$mbr" of="$disk" bs=446 count=1 conv=notrunc 2> /dev/null
dd if="$stage2" of="$disk" bs=512 count=32 seek=34 conv=notrunc 2> /dev/null

mformat -i "$disk@@2048s" -T 1048576 -c 2 -F -v EFI ::
mmd -i "$disk@@2048s" ::/Linux
mcopy -i "$disk@@2048s" "$kernel" ::/Linux/kernel
mcopy -i "$disk@@2048s" "$initrd" ::/Linux/initrd_with_long_name
mmd -i "$disk@@2048s" ::/loader
mmd -i "$disk@@2048s" ::/loader/entries

mcopy -i "$disk@@2048s" - ::/loader/entries/example-00.conf << EOF
title   Test Loader Entry
version 0.1
linux   /Linux/kernel
initrd  /Linux/initrd_with_long_name
options init=/nope
EOF

mcopy -i "$disk@@2048s" - ::/loader/entries/example-02.conf << EOF
title    Test Loader Entry
sort-key xyz
version  0.1
linux    /Linux/kernel
initrd   /Linux/initrd_with_long_name
options  init=/nope
EOF

mcopy -i "$disk@@2048s" - ::/loader/entries/example-03.conf << EOF
title    Test Loader Entry
sort-key xyz
version  0.2
linux    /Linux/kernel
initrd   /Linux/initrd_with_long_name
options  init=/hello
EOF

mcopy -i "$disk@@2048s" - ::/loader/entries/example-01.conf << EOF
title    Test Loader Entry
sort-key abc
version  0.3
linux    /Linux/kernel
initrd   /Linux/initrd_with_long_name
options  init=/nope
EOF

mdir -i "$disk@@2048s" -/ ::
