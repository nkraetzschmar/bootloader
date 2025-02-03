#include "types.h"
#include "bios_services.h"

void flush()
{
	print_char(0x00);
}

void print_str(const char *str)
{
	for (const char *ptr = str; *ptr; ++ptr) print_char(*ptr);
}

void init()
{
	print_str("hello\r\n");
	flush();
	reset();
}
