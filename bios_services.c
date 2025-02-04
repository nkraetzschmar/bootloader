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

struct global_descriptor_table {
	uint16 limit;
	uint16 base_low;
	uint8 base_high;
	uint8 access_mode;
	uint16 reserved;
};

static_assert(sizeof(struct disk_address_packet) == 0x10);

void halt()
{
	while (1) __asm__ __volatile__ ( "hlt" );
}

void reset()
{
	__asm__ __volatile__ ( "ljmp $0xffff, $0x0000" );
}

void print_char(char c)
{
	__asm__ __volatile__ ( "int $0x10" : : "a" (0x0e00 | c) : );
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

	__asm__ __volatile__ ( "int $0x13\nsetb %[res]\nmov %%ah, %[err]"
	                       : [res] "=r" (result), [err] "=r" (error)
	                       : "a" (0x4200), "d" (0x0080), "S" (&disk_address_packet)
	                       : );

	return result ? -error : 0;
}

int16 mem_move(uint8 *dst, const uint8 *src, uint32 len)
{
	static struct global_descriptor_table global_descriptor_table[6];

	int8 result;
	int8 error;
	int16 words;

	if ((uint32) dst > 0x00ffffff || (uint32) src > 0x00ffffff) return -1;

	for (uint16 i = 0; i < sizeof(global_descriptor_table); ++i) {
		((uint8 *) global_descriptor_table)[i] = 0x00;
	}

	global_descriptor_table[2].limit = 0x7fff;
	global_descriptor_table[2].base_low = ((uint32) src) & 0x0000ffff;
	global_descriptor_table[2].base_high = (((uint32) src) & 0x00ff0000) >> 0x10;
	global_descriptor_table[2].access_mode = 0x93;

	global_descriptor_table[3].limit = 0x7fff;
	global_descriptor_table[3].base_low = ((uint32) dst) & 0x0000ffff;
	global_descriptor_table[3].base_high = (((uint32) dst) & 0x00ff0000) >> 0x10;
	global_descriptor_table[3].access_mode = 0x93;

	words = len >> 1;

	__asm__ __volatile__ ( "int $0x15\nsetb %[res]\nmov %%ah, %[err]"
	                       : [res] "=r" (result), [err] "=r" (error)
	                       : "a" (0x8700), "c" (words), "S" (global_descriptor_table)
	                       : );

	return result ? -error : 0;
}

void exec_kernel()
{
	__asm__ __volatile__ (
		"mov $0x1000, %ax\n"
		"mov %ax, %ds\n"
		"mov %ax, %es\n"
		"mov %ax, %fs\n"
		"mov %ax, %gs\n"
		"mov %ax, %ss\n"
		"mov $0xf000, %sp\n"
		"jmp $0x1020, $0\n"
	);
}
