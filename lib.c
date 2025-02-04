#include "types.h"
#include "lib.h"
#include "bios_services.h"

static uint32 current_sector = 0;

void flush()
{
	print_char(0x00);
}

void print_str(const char *str)
{
	for (const char *ptr = str; *ptr; ++ptr) print_char(*ptr);
}

void reset_seek()
{
	current_sector = 0;
}

void seek(uint32 sectors)
{
	current_sector += sectors;
}

int16 read(uint8 *buffer, uint32 sectors)
{
	uint16 error;
	uint16 sectors_to_read;

	while (sectors) {
		sectors_to_read = sectors & 0x007f; // cap sectors at 64

		error = disk_read(buffer, sectors_to_read, current_sector);
		if (error != 0) return error;

		current_sector += sectors_to_read;
		sectors -= sectors_to_read;
		buffer += sectors_to_read * 0x0200;
	}

	return 0;
}
