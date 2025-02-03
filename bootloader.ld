OUTPUT_FORMAT("elf32-i386")
ENTRY(init)

MEMORY {
	TEXT (rx) : ORIGIN = 0x8000, LENGTH = 0x3000
	DATA (rw) : ORIGIN = 0xb000, LENGTH = 0x1000
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
		*(.bss.*)
		*(.bss)

		. = ALIGN(0x0010);
	} > DATA
}
