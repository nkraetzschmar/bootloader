#include "types.h"
#include "setup_header.h"

struct pe_header {
	uint32 signature;
	uint16 machine;
	uint16 num_sections;
	uint32 timestamp;
	uint32 symbol_table;
	uint32 num_symbols;
	uint16 size_opt_header;
	uint16 flags;
};

static_assert(sizeof(struct pe_header) == 0x0018);

struct pe_section {
	char name[0x08];
	uint32 vm_size;
	uint32 vm_addr;
	uint32 data_size;
	uint32 data_offset;
	uint32 relocs;
	uint32 lines;
	uint16 num_relocs;
	uint16 num_lines;
	uint32 flags;
};

static_assert(sizeof(struct pe_section) == 0x0028);

static uint8 *real_mode_kernel = (void *) 0x00010000;
static uint8 *cmdline = (void *) 0x0001f000;
static uint8 *prot_mode_kernel = (void *) 0x00100000;
static uint8 *initrd = (void *) 0x04000000;

static void halt()
{
	while (1) __asm__ __volatile__ ( "hlt" );
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

static uint8 inb(uint16 port)
{
	uint8 b;
	__asm__ __volatile__ ( "inb %%dx, %%al" : "=a" (b) : "d" (port) : );
	return b;
}

static void outb(uint16 port, uint8 b)
{
	__asm__ __volatile__ ( "outb %%al, %%dx" : : "a" (b), "d" (port) : );
}

static void print_char(char c)
{
	while ((inb(0x03fd) & 0x20) == 0x00);
	outb(0x03f8, c);
}

static void print_str(const char *str)
{
	for (const char *ptr = str; *ptr; ++ptr) print_char(*ptr);
}

static void memcpy(void *dst, const void *src, uint32 len)
{
	__asm__ __volatile__ (
		"push %%esi\n"
		"push %%edi\n"
		"push %%ecx\n"
		"data32 addr32 rep movsb\n"
		"pop %%ecx\n"
		"pop %%edi\n"
		"pop %%esi\n"
	: : "S" (src), "D" (dst), "c" (len) : );
}

static uint8 memeq(const void *a, const void *b, uint16 len)
{
	for (uint16 i = 0; i < len; ++i) if (((uint8 *) a)[i] != ((uint8 *) b)[i]) return 0;
	return 1;
}

static const char *hex_map = "0123456789ABCDEF";

static void print_hex(const uint8 *buf, uint16 len, int8 step)
{
	uint8 value;
	uint8 low_bits;
	uint8 high_bits;

	for (int16 i = 0; i < len; ++i) {
		value = *(buf + (i * step));
		low_bits  = value & 0x0f;
		high_bits = (value >> 4) & 0x0f;

		print_char(hex_map[high_bits]);
		print_char(hex_map[low_bits]);
	}
}

static void print_hex_le(const uint8 *buf, uint16 len)
{
	print_hex(buf + len - 1, len, -1);
}

void init()
{
	static struct setup_header setup_header;

	struct pe_header *pe_header;
	struct pe_section *pe_sections;

	uint32 kernel_offset  = 0x00000000;
	uint32 kernel_len     = 0x00000000;
	uint32 cmdline_offset = 0x00000000;
	uint32 cmdline_len    = 0x00000000;
	uint32 initrd_offset  = 0x00000000;
	uint32 initrd_len     = 0x00000000;

	uint32 real_mode_kernel_len;
	uint32 prot_mode_kernel_len;

	char *kernel_uname;

	print_str("\npe_loader\n");

	pe_header = (void *) 0x8400;
	pe_sections = ((void *) pe_header) + sizeof(struct pe_header) + pe_header->size_opt_header;

	for (uint16 i = 0; i < pe_header->num_sections; ++i) {

		if (!kernel_offset && memeq(pe_sections[i].name, (char[0x08]) { ".linux" }, 0x08)) {
			kernel_offset = pe_sections[i].data_offset - 0x2000;
			kernel_len    = pe_sections[i].data_size;

		} else if (!cmdline_offset && memeq(pe_sections[i].name, (char[0x08]) { ".cmdline" }, 0x08)) {
			cmdline_offset = pe_sections[i].data_offset - 0x2000;
			cmdline_len    = pe_sections[i].data_size;

		} else if (!initrd_offset && memeq(pe_sections[i].name, (char[0x08]) { ".initrd" }, 0x08)) {
			initrd_offset = pe_sections[i].data_offset - 0x2000;
			initrd_len    = pe_sections[i].data_size;
		}

	}

	print_str("kernel @0x");
	print_hex_le((uint8 *) &kernel_offset, 4);
	print_str("\n");

	print_str("cmdline @0x");
	print_hex_le((uint8 *) &cmdline_offset, 4);
	print_str("\n");

	print_str("initrd @0x");
	print_hex_le((uint8 *) &initrd_offset, 4);
	print_str("\n");

	memcpy(cmdline, prot_mode_kernel + cmdline_offset, cmdline_len);
	memcpy(initrd, prot_mode_kernel + initrd_offset, initrd_len);

	memcpy(real_mode_kernel, prot_mode_kernel + kernel_offset, 0x0400);
	memcpy(&setup_header, real_mode_kernel + 0x01f1, sizeof(struct setup_header));

	real_mode_kernel_len = (setup_header.setup_sects + 0x0001) * 0x0200;
	prot_mode_kernel_len = setup_header.syssize * 0x0010;

	if (setup_header.header != 0x53726448 || real_mode_kernel_len + prot_mode_kernel_len > kernel_len) {
		print_str("invalid kernel image\n");
		halt();
	}

	memcpy(
		real_mode_kernel + 0x0400,
		prot_mode_kernel + kernel_offset + 0x0400,
		real_mode_kernel_len - 0x0400
	);

	memcpy(
		prot_mode_kernel,
		prot_mode_kernel + kernel_offset + real_mode_kernel_len,
		prot_mode_kernel_len
	);

	kernel_uname = (char *) real_mode_kernel + setup_header.kernel_version + 0x0200;
	print_str(kernel_uname);
	print_str("\n");

	setup_header.type_of_loader = 0xff;
	setup_header.loadflags |= 0x80;
	setup_header.heap_end_ptr = 0xee00;
	setup_header.ramdisk_image = (uint32) (unsigned long) initrd;
	setup_header.ramdisk_size = initrd_len;
	setup_header.cmd_line_ptr = (uint32) (unsigned long) cmdline;

	memcpy(real_mode_kernel + 0x01f1, &setup_header, sizeof(setup_header));

	exec_kernel();
}
