MAKEFLAGS += --no-builtin-rules
.SILENT:
.PHONY: all clean test

OUTPUTS := mbr.bin

all: $(OUTPUTS)

clean:
	rm -f $(OUTPUTS)

test: mbr.bin
	echo 'running $< in qemu'
	./run.sh '$<'

%.bin: %.asm
	echo 'assembling $< -> $@'
	nasm -f bin -o '$@' '$<'
	hexdump -vC '$@'
