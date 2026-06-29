#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define PREFIX(name) bootloader_##name

#include "types.h"
#include "bios_services.h"

#define __STR(x) #x
#define _STR(x) __STR(x)
#define debug_printf(fmt, ...) if (debug) fprintf(stderr, "\033[2mdebug: " __FILE__ ":" _STR(__LINE__) " (%s): " fmt "\033[0m\n", __func__, ##__VA_ARGS__)

static int debug = 0;
static int disk_fd = -1;

void bootloader_init();

int main(int argc, char **argv)
{
	char *debug_env;

	debug_env = getenv("BOOTLOADER_EMU_DEBUG");
	if (debug_env && *debug_env == '1') debug = 1;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <disk>", argv[0]);
		exit(1);
	}

	disk_fd = open(argv[1], O_RDWR);
	if (disk_fd == -1) {
		perror("open");
		exit(1);
	}

	bootloader_init();
}

void bootloader_halt()
{
	debug_printf();
	while (1) pause();
}

void bootloader_reset()
{
	debug_printf();
	exit(0);
}

void bootloader_print_char(char c)
{
	if (c != 0) fputc(c, stdout);
	else fflush(stdout);
}

int16 bootloader_disk_read(uint8 *buffer, uint16 sectors, uint32 lba)
{
	uint32 len;
	uint32 offset;
	ssize_t len_read;

	debug_printf("buffer=0x%08lx, sectors=%u, lba=%u)", (size_t) buffer, sectors, lba);

	if (sectors > 64) return -1;

	len    = sectors * 0x0200;
	offset = lba * 0x0200;

	while (len) {
		len_read = pread(disk_fd, buffer, len, offset);

		if (len_read == -1) {
			perror("pread");
			return -1;
		} else if (len_read == 0) {
			fprintf(stderr, "unexpected EOF\n");
			return -1;
		}

		len -= len_read;
		offset += len_read;
		buffer += len_read;
	}

	return 0;
}

int16 bootloader_disk_write(const uint8 *buffer, uint16 sectors, uint32 lba)
{
	uint32 len;
	uint32 offset;
	ssize_t len_written;

	debug_printf("buffer=0x%08lx, sectors=%u, lba=%u)", (size_t) buffer, sectors, lba);

	if (sectors > 64) return -1;

	len    = sectors * 0x0200;
	offset = lba * 0x0200;

	while (len) {
		len_written = pwrite(disk_fd, buffer, len, offset);

		if (len_written == -1) {
			perror("pwrite");
			return -1;
		} else if (len_written == 0) {
			fprintf(stderr, "unexpected zero-length write\n");
			return -1;
		}

		len -= len_written;
		offset += len_written;
		buffer += len_written;
	}

	return 0;
}

int16 bootloader_mem_move(uint8 *dst, const uint8 *src, uint32 len)
{
	debug_printf("dst=0x%08lx, src=0x%08lx, len=%u)", (size_t) dst, (size_t) src, len);
	return 0;
}

void bootloader_exec_kernel()
{
	debug_printf();
	exit(0);
}
