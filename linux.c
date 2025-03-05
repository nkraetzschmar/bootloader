#include "types.h"
#include "bios_services.h"
#include "lib.h"
#include "fat32.h"
#include "loader.h"
#include "io_buf.h"
#ifndef NO_INC_GEN
#include "initrd.h"
#endif

struct setup_header {
	uint8  setup_sects;
	uint16 root_flags;
	uint32 syssize;
	uint16 ram_size;
	uint16 vid_mode;
	uint16 root_dev;
	uint16 boot_flag;
	uint16 jump;
	uint32 header;
	uint16 version;
	uint32 realmode_swtch;
	uint16 start_sys_seg;
	uint16 kernel_version;
	uint8  type_of_loader;
	uint8  loadflags;
	uint16 setup_move_size;
	uint32 code32_start;
	uint32 ramdisk_image;
	uint32 ramdisk_size;
	uint32 bootsect_kludge;
	uint16 heap_end_ptr;
	uint8  ext_loader_ver;
	uint8  ext_loader_type;
	uint32 cmd_line_ptr;
	uint32 initrd_addr_max;
	uint32 kernel_alignment;
	uint8  relocatable_kernel;
	uint8  min_alignment;
	uint16 xloadflags;
	uint32 cmdline_size;
	uint32 hardware_subarch;
	uint64 hardware_subarch_data;
	uint32 payload_offset;
	uint32 payload_length;
	uint64 setup_data;
	uint64 pref_address;
	uint32 init_size;
	uint32 handover_offset;
	uint32 kernel_info_offset;
};

static_assert(sizeof(struct setup_header) == 0x007b);

/* validate all struct members match boot protocol spec */
static_assert(&((struct setup_header *) 0x01f1)->setup_sects == (void *) 0x01f1);
static_assert(&((struct setup_header *) 0x01f1)->root_flags == (void *) 0x01f2);
static_assert(&((struct setup_header *) 0x01f1)->syssize == (void *) 0x01f4);
static_assert(&((struct setup_header *) 0x01f1)->ram_size == (void *) 0x01f8);
static_assert(&((struct setup_header *) 0x01f1)->vid_mode == (void *) 0x01fa);
static_assert(&((struct setup_header *) 0x01f1)->root_dev == (void *) 0x01fc);
static_assert(&((struct setup_header *) 0x01f1)->boot_flag == (void *) 0x01fe);
static_assert(&((struct setup_header *) 0x01f1)->jump == (void *) 0x0200);
static_assert(&((struct setup_header *) 0x01f1)->header == (void *) 0x0202);
static_assert(&((struct setup_header *) 0x01f1)->version == (void *) 0x0206);
static_assert(&((struct setup_header *) 0x01f1)->realmode_swtch == (void *) 0x0208);
static_assert(&((struct setup_header *) 0x01f1)->start_sys_seg == (void *) 0x020c);
static_assert(&((struct setup_header *) 0x01f1)->kernel_version == (void *) 0x020e);
static_assert(&((struct setup_header *) 0x01f1)->type_of_loader == (void *) 0x0210);
static_assert(&((struct setup_header *) 0x01f1)->loadflags == (void *) 0x0211);
static_assert(&((struct setup_header *) 0x01f1)->setup_move_size == (void *) 0x0212);
static_assert(&((struct setup_header *) 0x01f1)->code32_start == (void *) 0x0214);
static_assert(&((struct setup_header *) 0x01f1)->ramdisk_image == (void *) 0x0218);
static_assert(&((struct setup_header *) 0x01f1)->ramdisk_size == (void *) 0x021c);
static_assert(&((struct setup_header *) 0x01f1)->bootsect_kludge == (void *) 0x0220);
static_assert(&((struct setup_header *) 0x01f1)->heap_end_ptr == (void *) 0x0224);
static_assert(&((struct setup_header *) 0x01f1)->ext_loader_ver == (void *) 0x0226);
static_assert(&((struct setup_header *) 0x01f1)->ext_loader_type == (void *) 0x0227);
static_assert(&((struct setup_header *) 0x01f1)->cmd_line_ptr == (void *) 0x0228);
static_assert(&((struct setup_header *) 0x01f1)->initrd_addr_max == (void *) 0x022c);
static_assert(&((struct setup_header *) 0x01f1)->kernel_alignment == (void *) 0x0230);
static_assert(&((struct setup_header *) 0x01f1)->relocatable_kernel == (void *) 0x0234);
static_assert(&((struct setup_header *) 0x01f1)->min_alignment == (void *) 0x0235);
static_assert(&((struct setup_header *) 0x01f1)->xloadflags == (void *) 0x0236);
static_assert(&((struct setup_header *) 0x01f1)->cmdline_size == (void *) 0x0238);
static_assert(&((struct setup_header *) 0x01f1)->hardware_subarch == (void *) 0x023c);
static_assert(&((struct setup_header *) 0x01f1)->hardware_subarch_data == (void *) 0x0240);
static_assert(&((struct setup_header *) 0x01f1)->payload_offset == (void *) 0x0248);
static_assert(&((struct setup_header *) 0x01f1)->payload_length == (void *) 0x024c);
static_assert(&((struct setup_header *) 0x01f1)->setup_data == (void *) 0x0250);
static_assert(&((struct setup_header *) 0x01f1)->pref_address == (void *) 0x0258);
static_assert(&((struct setup_header *) 0x01f1)->init_size == (void *) 0x0260);
static_assert(&((struct setup_header *) 0x01f1)->handover_offset == (void *) 0x0264);
static_assert(&((struct setup_header *) 0x01f1)->kernel_info_offset == (void *) 0x0268);

uint8 real_mode_kernel_code[0x8000];
uint8 real_mode_kernel_heap[0x7000];
uint8 kernel_cmdline[0x0800];

struct setup_header *setup_header = (void *) (real_mode_kernel_code + 0x01f1);
uint8 *prot_mode_kernel = (void *) 0x00100000;
uint8 *initrd = (void *) 0x04000000;

int16 load_kernel()
{
	int16 error;
	uint8 real_mode_sectors;
	uint32 prot_mode_size;
	uint32 prot_mode_sectors;
	uint32 initrd_size;
	uint32 initrd_sectors;
	uint32 sectors_to_read;
	uint32 sectors_read;
	uint8 *ptr;
	const char *kernel_uname;

	error = open_path(get_kernel_path());
	if (error != 0) return error;

	sectors_read = read(real_mode_kernel_code, 0x00000002);
	if (sectors_read != 0x00000002) return -1;

	real_mode_sectors = setup_header->setup_sects;
	sectors_to_read = real_mode_sectors -1;
	sectors_read = read(real_mode_kernel_code + 0x0400, sectors_to_read);
	if (sectors_read != sectors_to_read) return -1;

	kernel_uname = (char *) real_mode_kernel_code + setup_header->kernel_version + 0x0200;
	print_str(kernel_uname);
	print_str("\r\n");

	prot_mode_size = setup_header->syssize * 0x10;
	prot_mode_sectors = (prot_mode_size + 0x1ff) / 0x200;
	ptr = prot_mode_kernel;

	while (prot_mode_sectors) {
		sectors_to_read = prot_mode_sectors > 0x80 ? 0x80 : prot_mode_sectors;

		sectors_read = read(io_buf, sectors_to_read);
		if (sectors_read != sectors_to_read) return -1;

		error = mem_move(ptr, io_buf, sectors_to_read * 0x0200);
		if (error != 0) return error;

		prot_mode_sectors -= sectors_to_read;
		ptr += sectors_to_read * 0x0200;
	}

	error = open_path(get_initrd_path());
	if (error != 0) return error;

	initrd_size = get_size();

	initrd_sectors = (initrd_size + 0x01ff) / 0x0200;
	ptr = initrd;

	while (initrd_sectors) {
		sectors_to_read = initrd_sectors > 0x80 ? 0x80 : initrd_sectors;

		sectors_read = read(io_buf, sectors_to_read);
		if (sectors_read != sectors_to_read) return -1;

		error = mem_move(ptr, io_buf, sectors_to_read * 0x0200);
		if (error != 0) return error;

		initrd_sectors -= sectors_to_read;
		ptr += sectors_to_read * 0x0200;
	}

	strcpy((char *) kernel_cmdline, get_cmdline(), sizeof(kernel_cmdline));

	setup_header->type_of_loader = 0xff;
	setup_header->loadflags |= 0x80;
	setup_header->heap_end_ptr = 0xee00;
	setup_header->ramdisk_image = (uint32) (unsigned long) initrd;
	setup_header->ramdisk_size = initrd_size;
	setup_header->cmd_line_ptr = (uint32) (unsigned long) kernel_cmdline;

	return 0;
}
