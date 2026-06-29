#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PREFIX(name) bootloader_##name

#include "types.h"
#include "lib.h"
#include "io_buf.h"
#include "gpt.h"
#include "fat32.h"
#include "fat32_internal.h"

static uint8 *disk_map;

void bootloader_halt()
{
	abort();
}

void bootloader_reset()
{
	exit(0);
}

void bootloader_print_char(char c)
{
	if (c != 0) fputc(c, stderr);
	else fflush(stderr);
}

int16 bootloader_disk_read(uint8 *buffer, uint16 sectors, uint32 lba)
{
	uint32 len;
	uint32 offset;

	if (sectors > 64) return -1;

	len    = sectors * 0x0200;
	offset = lba * 0x0200;

	memcpy(buffer, disk_map + offset, len);
	return 0;
}

int16 bootloader_disk_write(const uint8 *buffer, uint16 sectors, uint32 lba)
{
	uint32 len;
	uint32 offset;

	if (sectors > 64) return -1;

	len    = sectors * 0x0200;
	offset = lba * 0x0200;

	memcpy(disk_map + offset, buffer, len);
	return 0;
}

static void check_file(const char *name, const char *expected_content)
{
	int16 error;
	uint32 expected_size;
	uint32 size;
	uint32 sectors_needed;
	static uint8 read_buf[0x1000];

	expected_size = (uint32) strlen(expected_content);

	error = bootloader_open(name);
	assert(error == 0);

	size = bootloader_get_size();
	assert(size == expected_size);

	sectors_needed = (size + 0x01ff) / 0x0200;
	assert(sectors_needed <= sizeof(read_buf) / 0x0200);

	bootloader_reset_seek();
	assert(bootloader_read(read_buf, sectors_needed) == sectors_needed);
	assert(memcmp(read_buf, expected_content, expected_size) == 0);
}

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	int16 error;
	int32 stored_entry_index;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <disk>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	assert(fd >= 0);

	assert(fstat(fd, &st) == 0);

	disk_map = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	assert(disk_map != MAP_FAILED);
	close(fd);

	error = bootloader_find_esp();
	assert(error == 0);

	error = bootloader_load_fat32();
	assert(error == 0);

	printf("Testing open/read of short-name file...\n");
	check_file("testA.txt", "hello from testA");

	printf("Testing open/read of long-name file...\n");
	check_file("test_with_long_name_B.txt", "hello from test_with_long_name_B");

	printf("Testing chdir into short-name directory...\n");
	error = bootloader_chdir("dirC");
	assert(error == 0);
	check_file("testC.txt", "hello from testC");

	printf("Testing chdir to root and long-name directory...\n");
	error = bootloader_chdir(NULL);
	assert(error == 0);
	error = bootloader_chdir("directory_with_long_name_D");
	assert(error == 0);
	check_file("testD.txt", "hello from testD");

	printf("Testing rename of short-name file...\n");
	error = bootloader_chdir(NULL);
	assert(error == 0);
	error = bootloader_open("testE.txt");
	assert(error == 0);
	stored_entry_index = bootloader_get_entry_index();
	error = bootloader_rename("testX.txt");
	assert(error == 0);
	check_file("testX.txt", "hello from testE");
	assert(stored_entry_index == bootloader_get_entry_index());

	printf("Testing rename of long-name file...\n");
	error = bootloader_open("test_with_long_name_F.txt");
	assert(error == 0);
	stored_entry_index = bootloader_get_entry_index();
	error = bootloader_rename("test_with_long_name_X.txt");
	assert(error == 0);
	check_file("test_with_long_name_X.txt", "hello from test_with_long_name_F");
	assert(stored_entry_index == bootloader_get_entry_index());

	printf("Testing rename from short name to long name...\n");
	error = bootloader_open("testG.txt");
	assert(error == 0);
	error = bootloader_rename("testG_now_with_a_long_name.txt");
	assert(error == 0);
	check_file("testG_now_with_a_long_name.txt", "hello from testG");

	printf("Testing rename from long name to even longer name...\n");
	error = bootloader_open("test_with_long_name_H.txt");
	assert(error == 0);
	error = bootloader_rename("test_with_an_even_longer_name_H.txt");
	assert(error == 0);
	check_file("test_with_an_even_longer_name_H.txt", "hello from test_with_long_name_H");

	printf("All tests passed!\n");

	munmap(disk_map, st.st_size);
	return 0;
}
