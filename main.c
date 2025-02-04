#include "types.h"
#include "bios_services.h"
#include "lib.h"
#include "linux.h"

uint8 buf[0x0200];

void init()
{
	uint16 error;

	error = load_kernel();
	if (error != 0) {
		print_str("failed to load kernel\r\n");
		flush();
		halt();
	}

	flush();
	exec_kernel();
}
