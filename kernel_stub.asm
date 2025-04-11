org 0x0000
BITS 16

times 0x01f1 - ($ - $$) db 0x00

setup_header:
	db 0x0f                                     ; setup_sects
	dw 0x0000                                   ; root_flags
	dd 0x00000000                               ; syssize
	dw 0x0000                                   ; ram_size
	dw 0x0000                                   ; vid_mode
	dw 0x0000                                   ; root_dev
	dw 0xaa55                                   ; boot_flag
	jmp short init                              ; jump
	db 'HdrS'                                   ; header
	dw 0x020f                                   ; version = 2.15
	dd 0x00000000                               ; realmode_swtch
	dw 0x0000                                   ; start_sys_seg
	dw (version - 0x0200)                       ; kernel_version
	db 0x00                                     ; type_of_loader
	db 0x01                                     ; loadflags (LOADED_HIGH set)
	dw 0x0000                                   ; setup_move_size
	dd 0x00000000                               ; code32_start
	dd 0x00000000                               ; ramdisk_image
	dd 0x00000000                               ; ramdisk_size
	dd 0x00000000                               ; bootsect_kludge
	dw 0x0000                                   ; heap_end_ptr
	db 0x00                                     ; ext_loader_ver
	db 0x00                                     ; ext_loader_type
	dd 0x00000000                               ; cmd_line_ptr
	dd 0x00000000                               ; initrd_addr_max
	dd 0x00000000                               ; kernel_alignment
	db 0x00                                     ; relocatable_kernel
	db 0x00                                     ; min_alignment
	dw 0x0000                                   ; xloadflags
	dd 0x00000000                               ; cmdline_size
	dd 0x00000000                               ; hardware_subarch
	dq 0x0000000000000000                       ; hardware_subarch_data
	dd 0x00000000                               ; payload_offset
	dd 0x00000000                               ; payload_length
	dq 0x0000000000000000                       ; setup_data
	dq 0x0000000000000000                       ; pref_address
	dd 0x00000000                               ; init_size
	dd 0x00000000                               ; handover_offset
	dd 0x00000000                               ; kernel_info_offset

times 0x0280 - ($ - $$) db 0x00

init:
	cli                                         ; disable interrupts
	cld                                         ; ensure forward direction for string operations (lodsb)
	mov      ax,       ds                       ; load ds into ax
	cmp      ax,       0x1000                   ; compare ax to 0x1000 (check that segment starts at least at addr 0x00010000)
	jb       halt                               ; if below 0xa000 jump to halt
	xor      ax,       ax                       ; set ax=0x0000
	mov      es,       ax                       ; set es=0x0000

; memcopy(es:di, ds:si, 0x4000)
	mov      si,       0x0000                   ; set si=0x0000
	mov      di,       0x8000                   ; set di=0x8000
	mov      cx,       0x1000                   ; set cx=0x1000 (0x1000 dwords = 0x4000 bytes)
	rep movsd                                   ; while (cx != 0) {
	                                            ;     set dword [es:di] = dword [ds:si]
	                                            ;     increment si
	                                            ;     increment di
	                                            ;     decrement cx
	                                            ; }

; reset segment registers and setup stack
	xor      ax,       ax                       ; set ax = 0
	mov      ds,       ax                       ; ensure data segment at 0
	mov      es,       ax                       ; ensure extra segment at 0
	mov      fs,       ax                       ; ensure fs segment at 0
	mov      gs,       ax                       ; ensure gs segment at 0
	mov      ss,       ax                       ; ensure stack segment at 0
	mov      sp,       0x7c00                   ; set stack pointer to start of bootloader code (grows down below it)
	jmp      0x0000:setup_unreal                ; far jump to main, setting cs = 0x0000

halt:
	hlt
	jmp halt


times 0x02c0 - ($ - $$) db 0x00

version:
	db 'UKI:'

times 0x0300 - ($ - $$) db 0x00

section .main vstart=0x8300

setup_unreal:
	push     ds                                 ; save ds
	push     es                                 ; save es
	lgdt     [gdt_info]                         ; load gdt register
	mov      eax,      cr0                      ; load cr0
	or       eax,      0x00000001               ; set the protected mode bit
	mov      cr0,      eax                      ; switch to protected mode by setting cr0
	jmp      0x0008:prot_mode                   ; far jump to use protected mode

BITS 32
prot_mode:
	mov      ebx,      0x0010                   ; select descriptor 2
	mov      ds,       ebx                      ; set ds
	mov      es,       ebx                      ; set es

	and      eax,      0x11111110               ; unset the protected mode bit
	mov      cr0,      eax                      ; switch to real mode by setting cr0
	jmp      0x0000:unreal_mode                 ; far jump to use real mode

BITS 16
unreal_mode:
	pop      es                                 ; restore es
	pop      ds                                 ; restore ds
	jmp      0x0000:0x9000                      ; jump to main (C code)

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
	db 00000000b                                ; flags and limit high
	db 0x00                                     ; base high
.data:
	dw 0xffff                                   ; limit low
	dw 0x0000                                   ; base low
	db 0x00                                     ; base mid
	db 10010010b                                ; access mode
	db 11001111b                                ; flags and limit high
	db 0x00                                     ; base high
.end:

times 0x0100 - ($ - $$) db 0x00
