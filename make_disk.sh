#!/usr/bin/env bash

set -eufo pipefail

disk="$1"
mbr="$2"
stage2="$3"
kernel="$4"
initrd="$5"

truncate -s 0 "$disk"
truncate -s 1G "$disk"
sfdisk "$disk" << EOF
label: gpt
label-id: 01234567-abcd-0123-abcd-0123456789ab
start=2048, size=1048576, type=uefi, uuid=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee, name=EFI
EOF

dd if="$mbr" of="$disk" bs=446 count=1 conv=notrunc 2> /dev/null
dd if="$stage2" of="$disk" bs=512 count=32 seek=34 conv=notrunc 2> /dev/null
dd if="$kernel" of="$disk" bs=512 seek=2048 conv=notrunc 2> /dev/null
dd if="$initrd" of="$disk" bs=512 seek=34816 conv=notrunc 2> /dev/null
