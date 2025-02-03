[map symbols]

absolute 0x00007c00
mbr:
	.size:    equ 0x000001be
	.padding: equ 0x000001fe

absolute 0x00008000
stage2:
	.size:    equ 0x00004000
	.lba:     equ 0x000000022
	.sectors: equ .size / 0x200

section .text

org mbr
BITS 16

; perform basic init functions and setup stack
	cli                                         ; disable interrupts
	xor      ax,       ax                       ; set ax = 0
	mov      ds,       ax                       ; ensure data segment at 0
	mov      ss,       ax                       ; ensure stack segment at 0
	mov      es,       ax                       ; ensure extra segment at 0
	mov      fs,       ax                       ; ensure fs segment at 0
	mov      gs,       ax                       ; ensure gs segment at 0
	mov      sp,       mbr                      ; set stack pointer to start of bootloader code (grows down below it)
	cld                                         ; ensure forward direction for string operations (lodsb, movsb, etc)

; flush serial console and set output mode
	mov      ax,       0x0e00                   ; ah=0x0e (teletype mode) al=0x00 (null byte)
	mov      bx,       0x0000                   ; ensure page number is 0
	int      0x10                               ; output null byte to flush serial console
	mov      ax,       0x0002                   ; ah=0x00 (set video mode) al=0x02 (video mode 2: text mode 80x25 chars monochrome)
	int      0x10                               ; set video mode via interrupt

; load stage2 from disk into memory
	mov      si,       dap                      ; point si at dap (pre-initialized with source and destination addresses for stage2 load)
	mov      dl,       0x80                     ; select disk 0x80 (primary HDD) for disk access
	mov      ah,       0x42                     ; select LBA read mode for disk access
	int      0x13                               ; perform disk access via interrupt
	jc       halt                               ; on error jump to halt

	jmp      stage2                             ; pass control to stage2 code

; halts execution
halt:
	hlt                                         ; halt CPU
	jmp      halt                               ; keep halting if interrupted

; disk address packet
dap:
	db 0x10                                     ; size of struct = 16
	db 0x00                                     ; unused
	dw stage2.sectors                           ; number of sectors to read
	dw stage2                                   ; memory offset within segment
	dw 0x0000                                   ; memory segment
	dd stage2.lba                               ; low bytes of LBA address of data on disk
	dd 0x00000000                               ; high bytes of LBA address (unused by us)

; assert that we have not over-run the maximum size of an MBR bootloader
%if ($-$$) > mbr.size
	%error "MBR code exceeds 446 bytes"
%endif

; padd remaining space with zeros
%rep mbr.padding - ($-$$)
	db 0
%endrep

; add the required boot signature
	dw 0xaa55
