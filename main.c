#include "types.h"
#include "bios_services.h"
#include "lib.h"

uint8 buf[0x0200];

void init()
{
	print_str("hello ");
	seek(2048);
	read(buf, 1);
	print_str((char *) buf);
	print_str("\r\n");
	flush();
	reset();
}
