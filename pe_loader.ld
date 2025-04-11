OUTPUT_FORMAT("elf32-i386")
ENTRY(init)

MEMORY {
	TEXT   (rx) : ORIGIN = 0x00009000, LENGTH = 0x00000e00
	DATA   (rw) : ORIGIN = 0x00009e00, LENGTH = 0x00000200
	BSS    (rw) : ORIGIN = 0x0000c000, LENGTH = 0x00004000
	KERNEL (rw) : ORIGIN = 0x00010000, LENGTH = 0x0000f800
}

SECTIONS {
	.text : {
		*(.text.init)
		*(.text.*)
		*(.text)
	} > TEXT

	.data : {
		*(.rodata.*)
		*(.rodata)
		*(.data.*)
		*(.data)

		. = 0x0200;
	} > DATA

	.bss : {
		*(.bss.*)
		*(.bss)
	} > BSS
}
