#include "types.h"
#include "bios_services.h"
#include "lib.h"
#include "gpt.h"
#include "fat32.h"
#include "loader.h"
#include "linux.h"

void init()
{
	uint16 error;

	error = find_esp();
	if (error != 0) {
		print_str("Failed to load ESP\r\n");
		flush();
		halt();
	}

	error = load_fat32();
	if (error != 0) {
		print_str("Failed to load FAT32 file system\r\n");
		flush();
		halt();
	}

	error = find_entry();
	if (error != 0) {
		print_str("Failed to find boot entry\r\n");
		flush();
		halt();
	}

	error = load_kernel();
	if (error != 0) {
		print_str("Failed to load kernel\r\n");
		flush();
		halt();
	}

	flush();
	exec_kernel();
}
