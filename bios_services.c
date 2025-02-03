#include "types.h"
#include "bios_services.h"

void halt()
{
	while (1) __asm__ __volatile__ ("hlt");
}

void reset()
{
	__asm__ __volatile__ ("ljmp $0xffff, $0x0000");
}

void print_char(char c)
{
	__asm__ __volatile__ ("int $0x10" : : "ax" (0x0e00 | c) : );
}
