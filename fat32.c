#include "types.h"
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

static int16 dump_cluster(uint32 cluster);
static int16 list_dir(uint32 cluster);

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

	data_start = fat_start + (bpb->num_fats * fat_size);

	root_dir = bpb->root_cluster;
	current_dir = root_dir;

	print_str("Root directory @");
	print_hex_le((uint8 *) &root_dir, 0x0004);
	print_str("\r\n");

	error = dump_cluster(root_dir);
	if (error != 0) return error;

	error = list_dir(root_dir);
	if (error != 0) return error;

	return 0;
}

static int16 read_cluster(uint8 *buf, uint32 cluster, uint32 offset, uint16 sectors)
{
	uint32 sector;

	if (offset + sectors > cluster_size) {
		print_str("Read beyond cluster size\r\n");
		return -1;
	}

	sector = data_start + (cluster_size * (cluster - 2)) + offset;
	return esp_read(buf, sectors, sector);
}

static int16 dump_cluster(uint32 cluster)
{
	int16 error;

	error = read_cluster(io_buf, cluster, 0, cluster_size);
	if (error != 0) return error;

	for (uint16 i = 0; i < 0x0010 * cluster_size; ++i) {
		print_hex_be(io_buf + (i * 0x0020), 0x0020);
		print_str("\r\n");
	}

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

// TODO: multi cluster dir
static int16 list_dir(uint32 cluster)
{
	int error;
	struct fat32_dir_entry *entries;

	error = read_cluster(io_buf, cluster, 0, cluster_size);
	if (error != 0) return error;

	entries = (struct fat32_dir_entry *) io_buf;

	for (uint16 i = 0; i < cluster_size * 0x0010; ++i) {
		if (*((uint8 *) entries[i].name) == 0x00) break;
		if (*((uint8 *) entries[i].name) == 0xe5) continue;

		print_str("[attr=");
		print_hex_be(&entries[i].attr, 0x0001);
		print_str("] ");

		print_str(decode_name(&entries[i]));
		print_str("\r\n");
	}

	return 0;
}

// static uint32 read_fat_entry(uint32 cluster)
// {
// 	static uint32 buf[0x0080];
// 	static uint32 buf_sector = 0;

// 	int16  error;
// 	uint32 sector;
// 	uint32 offset;

// 	sector = cluster / 0x0080;
// 	offset = cluster % 0x0080;

// 	if (sector > fat_size) {
// 		print_str("Cluster out of range\r\n");
// 		return -1;
// 	}

// 	if (buf_sector != sector) {
// 		buf_sector = 0;

// 		error = esp_read((uint8 *) buf, 0x0001, sector);
// 		if (error != 0) return -1;

// 		buf_sector = sector;
// 	}

// 	return buf[offset];
// }
