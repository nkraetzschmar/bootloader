#include "types.h"
#include "lib.h"
#include "bios_services.h"
#include "gpt.h"

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

int16 read(uint8 *buffer, uint16 sectors)
{
	uint16 error;

	error = esp_read(buffer, sectors, current_sector);
	if (error != 0) return error;

	current_sector += sectors;
	return 0;
}

uint8 memeq(const void *a, const void *b, uint16 len)
{
	for (uint16 i = 0; i < len; ++i) if (((uint8 *) a)[i] != ((uint8 *) b)[i]) return 0;
	return 1;
}
