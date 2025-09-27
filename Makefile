MAKEFLAGS += --no-builtin-rules
.SILENT:
.PHONY: all clean distclean test uki_test kexec_test

CC := gcc
CC_X86 := x86_64-linux-gnu-gcc

CFLAGS := -std=c23 -Os -g -Wall -Wextra -Wdeclaration-after-statement -Werror -fpack-struct -fno-builtin
CFLAGS_M16 := $(CFLAGS) -m16 -march=i386 -nostdinc -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-exceptions -ffunction-sections -fdata-sections

LD := ld
LD_X86 := x86_64-linux-gnu-ld
LDFLAGS_M16 := -m elf_i386

OBJCOPY := objcopy
OBJCOPY_X86 := x86_64-linux-gnu-objcopy

OBJDUMP := objdump
OBJDUMP_X86 := x86_64-linux-gnu-objdump

OBJDUMP_FLAGS_M16 := -m i8086 -M intel

all: disk uki.efi kexec.cpio bootloader_emu kernel

clean:
	git clean -e '!kernel' -e '!kernel.tar.xz' -e '!busybox.tar.bz2' -fX

distclean:
	git clean -fX
	rm -rf kernel_headers busybox

test: disk
	echo 'running $< in qemu'
	./run.sh '$<' | tee serial.log
	grep -xF 'Found GPT disk: 01234567-ABCD-0123-ABCD-0123456789AB' < serial.log > /dev/null
	grep -xF 'Found ESP partition: AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE' < serial.log > /dev/null
	grep -xF 'Using ESP partiton @00000800' < serial.log > /dev/null
	grep -F 'hello from the initrd' < serial.log > /dev/null

uki_test: uki.efi uki_disk
	echo 'running $< as EFI binary'
	./run_efi.sh '$<' | tee serial.log
	grep -F 'hello from the initrd' < serial.log > /dev/null
	echo 'running $< as kernel bzImage'
	./run_kernel.sh '$<' | tee serial.log
	grep -F 'hello from the initrd' < serial.log > /dev/null
	echo 'running $(word 2,$^) in qemu'
	./run.sh '$(word 2,$^)' | tee serial.log
	grep -xF 'Found GPT disk: 01234567-ABCD-0123-ABCD-0123456789AB' < serial.log > /dev/null
	grep -xF 'Found ESP partition: AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE' < serial.log > /dev/null
	grep -xF 'Using ESP partiton @00000800' < serial.log > /dev/null
	grep -F 'hello from the initrd' < serial.log > /dev/null

kexec_test: kexec.cpio
	echo 'running $< as initrd in qemu'
	./run_init.sh kernel '$<' rdinit=/mini_kexec | tee serial.log
	grep -F 'kexec_core: Starting new kernel' < serial.log > /dev/null
	grep -F 'hello from the initrd' < serial.log > /dev/null

debug: disk bootloader.elf
	echo 'running $< in qemu in debug mode'
	./debug.sh $^

emu_test: bootloader_emu disk
	echo 'running $<'
	BOOTLOADER_EMU_DEBUG=1 ./$^

dependencies.make: *.c
	$(CC) -MM $^ | sed '/\\$$/ { N; s/ *\\\n//; }; /\.o:/{p;s/\.o/.m16.o/}' > '$@'

include dependencies.make

bootloader.o: main.o io_buf.o lib.o gpt.o fat32.o loader.o linux.o
	echo 'linking $^ -> $@'
	$(LD) -r -o '$@' $^
	$(OBJCOPY) --keep-global-symbol=init '$@'
	$(OBJDUMP) -h '$@'

bootloader_emu: bootloader.o bios_services_emu.o
	echo 'linking $^ -> $@'
	$(CC) -o '$@' $^

test_loader: lib.o gpt.o fat32.o io_buf.o

test_%: test_%.o
	echo 'linking $^ -> $@'
	$(CC) -o '$@' $^
	echo 'running $@'
	./'$@'

disk: make_disk.sh mbr.bin bootloader.bin kernel initrd.cpio
	echo 'creating $@'
	./$^ $@

demo: demo.img demo_uki.img

demo.img: make_demo.sh mbr.bin bootloader.bin kernel busybox
	echo 'creating $@'
	./$^ $@

demo_uki.img: make_demo_uki.sh mbr.bin bootloader.bin kernel busybox pe_inject.py kernel_stub.bin pe_loader.bin
	echo 'creating $@'
	./$^ $@

uki_disk: make_uki_disk.sh mbr.bin bootloader.bin uki.efi
	echo 'creating $@'
	./$^ $@

bootloader.elf: main.m16.o io_buf.m16.o lib.m16.o gpt.m16.o fat32.m16.o loader.m16.o linux.m16.o bios_services.m16.o

pe_loader.elf: pe_loader.m16.o

kernel.tar.xz:
	echo 'downloading kernel sources'
	curl -sSLf 'https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.16.8.tar.xz' > '$@'

kernel: build_kernel.sh kernel.tar.xz
	echo 'building kernel'
	./$^

kernel_headers: kernel

busybox.tar.bz2:
	echo 'downloading busybox sources'
	curl -sSLf 'https://busybox.net/downloads/busybox-1.37.0.tar.bz2' > '$@'

busybox: build_busybox.sh busybox.tar.bz2 kernel_headers
	echo 'building busybox'
	./$^

hello: hello.c
	echo 'compiling $^ -> $@'
	$(CC_X86) $(CFLAGS) -static -o '$@' $^

mini_kexec: mini_kexec.c
	echo 'compiling $^ -> $@'
	$(CC_X86) $(CFLAGS) -static -o '$@' $^

initrd.cpio: hello
	echo 'building $@'
	echo $< | cpio -o -H newc > '$@'

kexec.cpio: mini_kexec purgatory.bin uki.efi
	echo 'building $@'
	printf '%s\n' $^ | cpio -o -H newc > '$@'

uki_base.efi: kernel initrd.cpio
	ukify build --stub /usr/lib/systemd/boot/efi/linuxx64.efi.stub --linux '$(word 1,$^)' --initrd '$(word 2,$^)' --cmdline "rdinit=/hello" --os-release "VERSION=0.1" -o '$@'

uki.efi: pe_inject.py uki_base.efi kernel_stub.bin pe_loader.bin
	./$^ '$@'

%.bin: %.asm
	echo 'assembling $< -> $@'
	nasm -f bin -o '$@' '$<'
	hexdump -vC '$@'

%.o:
	echo 'compiling $< -> $@'
	$(CC) $(CFLAGS) -c '$<' -o '$@'
	$(OBJDUMP) -h '$@'

%.m16.o:
	echo 'compiling $< -> $@'
	$(CC_X86) $(CFLAGS_M16) -c '$<' -o '$@'
	$(OBJDUMP_X86) $(OBJDUMP_FLAGS_M16) -h -d '$@'

%.elf: %.ld
	echo 'linking $^ -> $@'
	$(LD_X86) $(LDFLAGS_M16) -o '$@' -T $^
	$(OBJDUMP_X86) $(OBJDUMP_FLAGS_M16) -h -d '$@'

%.bin: %.elf
	echo 'writing $< -> $@'
	$(OBJCOPY_X86) -O binary '$<' '$@'
	hexdump -C '$@'
