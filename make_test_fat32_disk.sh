#!/usr/bin/env bash

set -eufo pipefail

disk="$1"; shift

truncate -s 0 "$disk"
truncate -s 64M "$disk"
sfdisk "$disk" << EOF
label: gpt
label-id: 01234567-abcd-0123-abcd-0123456789ab
start=2048, size=65536, type=uefi, uuid=aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee, name=EFI
EOF

mformat -i "$disk@@2048s" -T 65536 -c 2 -F -v EFI ::

printf 'hello from testA' | mcopy -i "$disk@@2048s" - ::/testA.txt
printf 'unused0' | mcopy -i "$disk@@2048s" - ::/unused0
printf 'unused1' | mcopy -i "$disk@@2048s" - ::/unused1
printf 'hello from test_with_long_name_B' | mcopy -i "$disk@@2048s" - ::/test_with_long_name_B.txt
printf 'unused2' | mcopy -i "$disk@@2048s" - ::/unused2

mmd -i "$disk@@2048s" ::/dirC
printf 'unused3' | mcopy -i "$disk@@2048s" - ::/unused3
printf 'hello from testC' | mcopy -i "$disk@@2048s" - ::/dirC/testC.txt
printf 'unused4' | mcopy -i "$disk@@2048s" - ::/dirC/unused4

mmd -i "$disk@@2048s" ::/directory_with_long_name_D
printf 'unused5' | mcopy -i "$disk@@2048s" - ::/unused5
printf 'hello from testD' | mcopy -i "$disk@@2048s" - ::/directory_with_long_name_D/testD.txt
printf 'unused6' | mcopy -i "$disk@@2048s" - ::/directory_with_long_name_D/unused6

printf 'hello from testE' | mcopy -i "$disk@@2048s" - ::/testE.txt
printf 'unused7' | mcopy -i "$disk@@2048s" - ::/unused7
printf 'hello from test_with_long_name_F' | mcopy -i "$disk@@2048s" - ::/test_with_long_name_F.txt
printf 'unused8' | mcopy -i "$disk@@2048s" - ::/unused8
printf 'hello from testG' | mcopy -i "$disk@@2048s" - ::/testG.txt
printf 'unused9' | mcopy -i "$disk@@2048s" - ::/unused9
printf 'hello from test_with_long_name_H' | mcopy -i "$disk@@2048s" - ::/test_with_long_name_H.txt
for i in {0..7}; do
	printf 'buf%s' "$i" | mcopy -i "$disk@@2048s" - "::/buf$i"
done
printf 'unused10' | mcopy -i "$disk@@2048s" - ::/unused10
for i in {0..7}; do
	mdel -i "$disk@@2048s" "::/buf$i"
done

mdir -i "$disk@@2048s" -/ ::
