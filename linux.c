#include "types.h"
#include "bios_services.h"
#include "lib.h"

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
	uint32 hardware_subarch_data[2];
	uint32 payload_offset;
	uint32 payload_length;
	uint32 setup_data[2];
	uint32 pref_address[2];
	uint32 init_size;
	uint32 handover_offset;
	uint32 kernel_info_offset;
};

uint8 real_mode_kernel_code[0x8000];
uint8 real_mode_kernel_heap[0x7000];
uint8 kernel_cmdline[0x0800];

uint8 io_buf[0x8000];

struct setup_header *setup_header = (void *) (real_mode_kernel_code + 0x01f1);
uint8 *prot_mode_kernel = (void *) 0x00100000;

const uint32 kernel_lba = 0x0800; // hardcoded for now, should be file based later

int16 load_kernel()
{
	int16 error;
	uint8 real_mode_sectors;
	uint32 prot_mode_size;
	uint32 prot_mode_sectors;
	uint16 read_sectors;
	uint8 *ptr;
	const char *kernel_uname;

	seek(kernel_lba);
	error = read(real_mode_kernel_code, 2);
	if (error != 0) return error;

	real_mode_sectors = setup_header->setup_sects;
	error = read(real_mode_kernel_code + 0x0400, real_mode_sectors - 1);
	if (error != 0) return error;

	kernel_uname = (char *) real_mode_kernel_code + setup_header->kernel_version + 0x0200;
	print_str(kernel_uname);
	print_str("\r\n");

	prot_mode_size = setup_header->syssize * 0x10;
	prot_mode_sectors = (prot_mode_size + 0x1ff) / 0x200;
	ptr = prot_mode_kernel;

	while (prot_mode_sectors) {
		read_sectors = prot_mode_sectors > 0x40 ? 0x40 : prot_mode_sectors;

		error = read(io_buf, read_sectors);
		if (error != 0) return error;

		error = mem_move(ptr, io_buf, read_sectors * 0x0200);
		if (error != 0) return error;

		prot_mode_sectors -= read_sectors;
		ptr += read_sectors * 0x0200;
	}

	setup_header->type_of_loader = 0xff;
	setup_header->loadflags |= 0x80;
	setup_header->heap_end_ptr = 0xee00;

	return 0;
}
