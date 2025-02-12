OUTPUT_FORMAT("elf32-i386")
ENTRY(init)

MEMORY {
	TEXT   (rx) : ORIGIN = 0x00008000, LENGTH = 0x00003000
	DATA   (rw) : ORIGIN = 0x0000b000, LENGTH = 0x00001000
	KERNEL (rw) : ORIGIN = 0x00010000, LENGTH = 0x0000f800
	HEAP   (rw) : ORIGIN = 0x00040000, LENGTH = 0x00010000
}

SECTIONS {
	.text : {
		*(.text.init)
		*(.text.*)
		*(.text)
	} > TEXT

	.kernel : {
		*(.bss.real_mode_kernel_code)
		*(.bss.real_mode_kernel_heap)
		*(.bss.kernel_cmdline)
	} > KERNEL

	.heap : {
		*(.bss.heap)
	} > HEAP

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
