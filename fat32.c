#include "types.h"
#include "fat32.h"
#include "lib.h"
#include "gpt.h"
#include "io_buf.h"

struct fat32_bpb {
	uint8  jmp_boot[0x0003];
	uint8  oem_name[0x0008];
	uint16 bytes_per_sector;
	uint8  sectors_per_cluster;
	uint16 reserved_sectors_cnt;
	uint8  num_fats;
	uint16 root_entry_cnt;
	uint16 total_sectors_16;
	uint8  media;
	uint16 fat_size_16;
	uint16 sectors_per_track;
	uint16 num_heads;
	uint32 hidden_sectors;
	uint32 total_sectors_32;
	uint32 fat_size_32;
	uint16 ext_flags;
	uint16 fs_ver;
	uint32 root_cluster;
	uint16 fs_info_sector;
	uint16 backup_boot_sector;
	uint8  reserved[0x000c];
	uint8  drive_num;
	uint8  reserved_nt;
	uint8  boot_sig;
	uint32 volume_id;
	uint8  volume_label[0x000b];
	uint8  fs_type[0x0008];
	uint8  unused[0x01a4];
	uint16 signature;
};

static_assert(sizeof(struct fat32_bpb) == 0x0200);

struct fat32_dir_entry {
	char   name[8];
	char   ext[3];
	uint8  attr;
	uint8  reserved;
	uint8  time_ts;
	uint16 time;
	uint16 date;
	uint16 access_date;
	uint16 cluster_high;
	uint16 mod_time;
	uint16 mod_date;
	uint16 cluster_low;
	uint32 size;
};

static_assert(sizeof(struct fat32_dir_entry) == 0x0020);

static int16 check_bpb(struct fat32_bpb *bpb)
{
	if (bpb->signature != 0xaa55) {
		print_str("Not a FAT file system\r\n");
		return -1;
	}

	if (bpb->bytes_per_sector != 0x200) {
		print_str("Unsupported sector size\r\n");
		return -1;
	}

	if (bpb->sectors_per_cluster > 0x80) {
		print_str("Cluster size out of range\r\n");
		return -1;
	}

	if (bpb->num_fats < 0x01) {
		print_str("Invalid number of FATs\r\n");
		return -1;
	}

	if (bpb->fat_size_32 == 0 || bpb->fat_size_16 != 0) {
		print_str("Not a FAT32 file system, FAT12/16 not supported\r\n");
		return -1;
	}

	if (bpb->root_cluster < 2) {
		print_str("No valid root cluster\r\n");
		return -1;
	}

	return 0;
}

static uint32 cluster_size;

static uint32 fat_start;
static uint32 fat_size;

static uint32 data_start;

static uint32 root_dir;
static uint32 current_dir;

static uint32 current_file;
static uint32 current_cluster;
static uint32 current_offset;

uint32 file_size;

int16 load_fat32()
{
	int16 error;
	struct fat32_bpb *bpb;

	error = esp_read(io_buf, 0x0001, 0x0000);
	if (error != 0) return error;

	bpb = (struct fat32_bpb *) io_buf;

	error = check_bpb(bpb);
	if (error != 0) return error;

	cluster_size = bpb->sectors_per_cluster;

	fat_start = bpb->reserved_sectors_cnt;
	fat_size = bpb->fat_size_32;

	print_str("FAT32 table @");
	print_hex_le((uint8 *) &fat_start, 0x0004);
	print_str("\r\n");

	data_start = fat_start + (bpb->num_fats * fat_size);

	root_dir = bpb->root_cluster;
	current_dir = root_dir;

	current_file    = 0;
	current_cluster = 0;
	current_offset  = 0;

	print_str("Root directory: ");
	print_hex_le((uint8 *) &root_dir, 0x0004);
	print_str("\r\n");

	return 0;
}

static char * decode_name(struct fat32_dir_entry *entry)
{
	static char buf[0x000a];

	char c;
	char *ptr;

	ptr = buf;

	for (uint16 i = 0; i < 0x0008; ++i)
	{
		c = entry->name[i];
		if (c != ' ') *(ptr++) = c;
	}

	if (entry->ext[0] != ' ') {
		*(ptr++) = '.';

		for (uint16 i = 0; i < 0x0003; ++i)
		{
			c = entry->ext[i];
			if (c != ' ') *(ptr++) = c;
		}
	}

	return buf;
}

static uint32 find_file(const char *file)
{
	uint32 read_sectors;
	struct fat32_dir_entry *entries;

	current_file    = current_dir;
	current_cluster = current_dir;
	current_offset  = 0x00000000;

	read_sectors = read(io_buf, 0x00000080);
	if (read_sectors == 0x00000000) return -1;

	entries = (struct fat32_dir_entry *) io_buf;

	for (uint16 i = 0; i < read_sectors * 0x0010; ++i) {
		if (*((uint8 *) entries[i].name) == 0x00) break;
		if (*((uint8 *) entries[i].name) == 0xe5) continue;

		if (streq(decode_name(&entries[i]), file) && (entries[i].attr & 0x08) == 0x00) {
			file_size = entries[i].size;
			return (((uint32) entries[i].cluster_high) << 0x10) + entries[i].cluster_low;
		}
	}

	return 0x00000000;
}

int16 open(const char *file)
{
	uint32 cluster;

	cluster = find_file(file);
	if (cluster == 0x00000000) {
		print_str("File not found\r\n");
		return -1;
	}

	current_file    = cluster;
	current_cluster = cluster;
	current_offset  = 0x00000000;

	return 0;
}

static uint32 read_fat_entry(uint32 cluster)
{
	static uint32 buf[0x0080];
	static uint32 buf_sector = 0;

	int16  error;
	uint32 sector;
	uint32 offset;

	sector = fat_start + (cluster / 0x0080);
	offset = cluster % 0x0080;

	if (sector > fat_size) {
		print_str("Cluster out of range\r\n");
		return 0x00000000;
	}

	if (buf_sector != sector) {
		buf_sector = 0;

		error = esp_read((uint8 *) buf, 0x0001, sector);
		if (error != 0) return 0x00000000;

		buf_sector = sector;
	}

	return buf[offset];
}

void reset_seek()
{
	current_cluster = current_file;
	current_offset  = 0;
}

int16 seek(uint32 sectors)
{
	uint32 remaining_sectors_in_cluster;
	uint32 next_cluster;

	if (current_file == 0) {
		print_str("No file opened\r\n");
		return -1;
	}

	while (sectors > 0) {
		remaining_sectors_in_cluster = cluster_size - current_offset;

		if (sectors < remaining_sectors_in_cluster) {
			current_offset += sectors;
			return 0;
		}

		next_cluster = read_fat_entry(current_cluster);

		if (next_cluster >= 0x0ffffff8) {
			current_offset = cluster_size;
			return 0;
		} else if (next_cluster < 0x00000002 || next_cluster == 0x0ffffff7) {
			print_str("Bad cluster\r\n");
			return -1;
		}

		current_cluster = next_cluster;
		current_offset = 0;

		sectors -= remaining_sectors_in_cluster;
	}

	return 0;
}

uint32 read(uint8 *buf, uint32 sectors)
{
	int16 error;
	uint32 sectors_read;
	uint32 remaining_sectors_in_cluster;
	uint32 sectors_to_read;
	uint32 disk_sector;

	sectors_read = 0;

	/* current_offset == cluster_size indicates seek reached EOF */
	while (sectors_read < sectors && current_offset < cluster_size) {
		remaining_sectors_in_cluster = cluster_size - current_offset;
		if (sectors < remaining_sectors_in_cluster) sectors_to_read = sectors;
		else sectors_to_read = remaining_sectors_in_cluster;

		disk_sector = data_start + (cluster_size * (current_cluster - 2)) + current_offset;
		error = esp_read(buf, sectors_to_read, disk_sector);
		if (error != 0) return 0x00000000;

		seek(sectors_to_read);
		sectors_read += sectors_to_read;
		buf += sectors_to_read * 0x0200;
	}

	return sectors_read;
}
