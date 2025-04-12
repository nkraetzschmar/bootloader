#include "types.h"
#include "bios_services.h"
#include "lib.h"
#include "fat32.h"
#include "loader.h"
#include "io_buf.h"
#include "setup_header.h"

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
	uint32 initrd_size = 0;
	uint32 initrd_sectors;
	uint32 sectors_to_read;
	uint32 sectors_read;
	uint8 *ptr;
	const char *kernel_uname;

	if (get_uki_cluster()) {
		open_cluster(get_uki_cluster(), get_uki_size());
	} else {
		error = open_path(get_kernel_path());
		if (error != 0) return error;
	}

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

	if (*get_initrd_path() != 0x00) {
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
	}

	if (*get_cmdline() != 0x00) {
		strcpy((char *) kernel_cmdline, get_cmdline(), sizeof(kernel_cmdline));
	}

	setup_header->type_of_loader = 0xff;
	setup_header->loadflags |= 0x80;
	setup_header->heap_end_ptr = 0xee00;
	setup_header->ramdisk_image = (uint32) (unsigned long) initrd;
	setup_header->ramdisk_size = initrd_size;
	setup_header->cmd_line_ptr = (uint32) (unsigned long) kernel_cmdline;

	return 0;
}
