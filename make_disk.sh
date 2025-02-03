#!/usr/bin/env bash

set -eufo pipefail

disk="$1"
mbr="$2"
stage2="$3"

truncate -s 0 "$disk"
truncate -s 1G "$disk"
printf 'label: gpt\n' | sfdisk "$disk"
dd if="$mbr" of="$disk" bs=446 count=1 conv=notrunc 2> /dev/null
dd if="$stage2" of="$disk" bs=512 count=32 seek=34 conv=notrunc 2> /dev/null
