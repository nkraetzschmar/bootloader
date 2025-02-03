MAKEFLAGS += --no-builtin-rules
.SILENT:
.PHONY: all clean distclean test

CC := gcc
CC_X86 := x86_64-linux-gnu-gcc

CFLAGS := -std=c23 -Os -g -Wall -Wextra -Wdeclaration-after-statement -Werror -fpack-struct
CFLAGS_M16 := $(CFLAGS) -m16 -march=i386 -nostdinc -ffreestanding -fno-pic -fno-stack-protector -ffunction-sections -fdata-sections

LD_X86 := x86_64-linux-gnu-ld
LDFLAGS_M16 := -m elf_i386

OBJCOPY := objcopy
OBJCOPY_X86 := x86_64-linux-gnu-objcopy

OBJDUMP := objdump
OBJDUMP_X86 := x86_64-linux-gnu-objdump

OBJDUMP_FLAGS_M16 := -m i8086 -M intel

all: disk bootloader_emu kernel

clean:
	git clean -e '!kernel' -e '!kernel.tar.xz' -fX

distclean:
	git clean -fX

test: disk
	echo 'running $< in qemu'
	./run.sh '$<'

debug: disk
	echo 'running $< in qemu in debug mode'
	./debug.sh '$<'

dependencies.make: *.c
	$(CC) -MM $^ | sed '/\.o:/{p;s/\.o/.m16.o/}' > '$@'

include dependencies.make

bootloader_emu: main.o bios_services_emu.o
	$(CC) -o '$@' $^

disk: mbr.bin bootloader.bin
	./make_disk.sh '$@' $^

bootloader.elf: main.m16.o bios_services.m16.o

kernel.tar.xz:
	echo 'downloading kernel sources'
	curl -sSLf 'https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.13.1.tar.xz' > '$@'

kernel: build_kernel.sh kernel.tar.xz
	echo 'building kernel'
	./$^

%.bin: %.asm
	echo 'assembling $< -> $@'
	nasm -f bin -o '$@' '$<'
	hexdump -vC '$@'

%.o:
	echo 'compiling $< -> $@'
	$(CC) $(CFLAGS) -c '$<' -o '$@'
	$(OBJDUMP) -h -d '$@'

%.m16.o:
	echo 'compiling $< -> $@'
	$(CC_X86) $(CFLAGS_M16) -c '$<' -o '$@'
	$(OBJDUMP_X86) $(OBJDUMP_FLAGS_M16) -h -d '$@'

%.elf: %.ld
	echo 'linking $^ -> $@'
	$(LD_X86) $(LDFLAGS_M16) -o '$@' -T $^

%.bin: %.elf
	echo 'writing $< -> $@'
	$(OBJCOPY_X86) -O binary '$<' '$@'
	hexdump -C '$@'
