org 0x8000
BITS 64

long_mode:
	cli                                         ; disable interrupts
	cld                                         ; ensure forward direction for string operations (lodsb)
	lgdt     [gdt_info]                         ; load gdt register

	mov      ecx,      0xC0000080               ; select MSR_EFER
	rdmsr                                       ; read MSR_EFER into eax
	and      eax,      ~0x00000100              ; clear LME (bit 8)
	wrmsr                                       ; write MSR_EFER

	mov      rax,      cr0                      ; load cr0 into rax
	and      eax,      ~0x80000000              ; clear PG
	mov      cr0,      rax                      ; write back cr0

	mov      rsp,      0x8000                   ; set stack pointer
	push     0x0008                             ; push code segment selector
	push     prot_mode                          ; push code offset
	retf                                        ; far jump using pushed values on stack

gdt_info:
	dw gdt.end - gdt - 1                        ; last byte in gdt
	dd gdt                                      ; start of gdt

gdt:
.null:
	dq 0x0000000000000000
.code:
	dw 0xffff                                   ; limit low
	dw 0x0000                                   ; base low
	db 0x00                                     ; base mid
	db 10011010b                                ; access mode
	db 00001111b                                ; flags and limit high
	db 0x00                                     ; base high
.end:

BITS 16

prot_mode:
	mov      eax,      cr4                      ; load cr4 into eax
	and      eax,      ~0x00000020              ; clear PAE (bit 5)
	mov      cr4,      eax                      ; write back cr4

	mov      eax,      cr0                      ; lead cr0 into eax
	and      eax,      ~0x00000001              ; clear PE (bit 0)
	mov      cr0,      eax                      ; write back cr4

	jmp      0x0000:real_mode                   ; far jump into real mode

real_mode:
	lidt     [idt]                              ; reset to BIOS IDT

	mov      ax,       0x1000                   ; clear all segment registers
	mov      ds,       ax
	mov      es,       ax
	mov      fs,       ax
	mov      gs,       ax
	mov      ss,       ax
	mov      sp,       0xf000
	jmp      0x1020:0x0000                      ; far jump to bzImage entry point

idt:
	dw 0x400 - 1                                ; IDT limit
	dw 0, 0                                     ; IDT base address
