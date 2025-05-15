target remote :1234
break *0x8000
set disassembly-flavor intel
set disassemble-next-line on
continue
