#include "types.h"
#include "bios_services.h"

struct disk_address_packet {
	uint8 size;
	uint8 reserved;
	uint16 sectors;
	uint16 buffer_offset;
	uint16 buffer_segment;
	uint32 lba_low;
	uint32 lba_high;
};

static_assert(sizeof(struct disk_address_packet) == 0x10);

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

int16 disk_read(uint8 *buffer, uint16 sectors, uint32 lba)
{
	static struct disk_address_packet disk_address_packet;

	uint8 result;
	uint8 error;
	uint16 offset;
	uint16 segment;

	if (sectors > 64) return -1;

	if ((uint32) buffer > 0x000fffff) return -1;
	offset  = ((uint32) buffer & 0x0000ffff);
	segment = ((uint32) buffer & 0x000f0000) >> 4;

	disk_address_packet = (struct disk_address_packet) {
		.size           = 0x10,
		.reserved       = 0x00,
		.sectors        = sectors,
		.buffer_offset  = offset,
		.buffer_segment = segment,
		.lba_low        = lba,
		.lba_high       = 0x00000000
	};

	__asm__ __volatile__ ( "mov %[dap], %%si\nint $0x13\nmov %%ah, %[err]"
	                       : "=cf" (result), [err] "=r" (error)
	                       : "ax" (0x4200), "dx" (0x0080), [dap] "i" (&disk_address_packet)
	                       : "si" );

	return result ? -error : 0;
}
