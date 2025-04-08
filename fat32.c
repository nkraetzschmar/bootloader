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

struct fat32_lfn_entry {
	uint8  seq;
	uint16 name1[5];
	uint8  attr;
	uint8  reserved;
	uint8  checksum;
	uint16 name2[6];
	uint16 zero;
	uint16 name3[2];
};

static_assert(sizeof(struct fat32_lfn_entry) == 0x0020);

static struct {
	uint32 cluster_size;
	uint32 fat_start;
	uint32 fat_size;
	uint32 data_start;
	uint32 root_dir;
	uint32 current_dir;
} fs;

static struct {
	uint32 size;
	uint32 start_cluster;
	uint32 current_cluster;
	uint32 offset;
} file;

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

int16 load_fat32()
{
	int16 error;
	struct fat32_bpb *bpb;

	error = esp_read(io_buf, 0x0001, 0x0000);
	if (error != 0) return error;

	bpb = (struct fat32_bpb *) io_buf;

	error = check_bpb(bpb);
	if (error != 0) return error;

	fs.cluster_size = bpb->sectors_per_cluster;

	fs.fat_start = bpb->reserved_sectors_cnt;
	fs.fat_size = bpb->fat_size_32;

	print_str("FAT32 table @");
	print_hex_le((uint8 *) &fs.fat_start, 0x0004);
	print_str("\r\n");

	fs.data_start = fs.fat_start + (bpb->num_fats * fs.fat_size);

	fs.root_dir    = bpb->root_cluster;
	fs.current_dir = fs.root_dir;

	print_str("Root directory: ");
	print_hex_le((uint8 *) &fs.root_dir, 0x0004);
	print_str("\r\n");

	file.size            = 0;
	file.start_cluster   = 0;
	file.current_cluster = 0;
	file.offset          = 0;

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

	*ptr = 0x00;

	return buf;
}

static struct fat32_dir_entry * next_entry(struct fat32_dir_entry **entries, uint32 *entries_len, char **entry_name)
{
	static char buf[0x0100];

	struct fat32_dir_entry *entry;
	struct fat32_lfn_entry *lfn_entry;
	char *lfn_cursor;

	lfn_cursor = buf + sizeof(buf) - 1;
	*lfn_cursor = 0x00;

	while (*entries_len > 0) {
		entry = *entries;
		*entries += 1;
		*entries_len -= 1;

		if (*((uint8 *) entry->name) == 0x00) break;
		if (*((uint8 *) entry->name) == 0xe5) continue;

		if (entry->attr == 0x0F && lfn_cursor) {
			lfn_entry = (struct fat32_lfn_entry *) entry;

			if (lfn_cursor <= buf + 13) {
				print_str("LFN exceeds max range\r\n");
				lfn_cursor = (void *) 0;
			}

			for (int16 j = 0x0001; j >= 0x0000; --j) *(--lfn_cursor) = (lfn_entry->name3[j] <= 0x7f) ? (uint8)lfn_entry->name3[j] : 0x00;
			for (int16 j = 0x0005; j >= 0x0000; --j) *(--lfn_cursor) = (lfn_entry->name2[j] <= 0x7f) ? (uint8)lfn_entry->name2[j] : 0x00;
			for (int16 j = 0x0004; j >= 0x0000; --j) *(--lfn_cursor) = (lfn_entry->name1[j] <= 0x7f) ? (uint8)lfn_entry->name1[j] : 0x00;

			continue;
		}

		if ((entry->attr & 0x08) == 0x00) {
			if (lfn_cursor && *lfn_cursor != 0x00) *entry_name = lfn_cursor;
			else *entry_name = decode_name(entry);

			return entry;
		}

		lfn_cursor = buf + sizeof(buf) - 1;
		*lfn_cursor = 0x00;
	}

	return (void *) 0;
}

int16 for_each_dir_entry(void (*callback)(const char *entry_name, uint32 entry_cluster, uint32 entry_size))
{
	uint32 read_sectors;
	struct fat32_dir_entry *entries;
	uint32 entries_len;

	struct fat32_dir_entry *entry;
	char *entry_name;
	uint32 entry_cluster;

	file.size          = 0x00000000;
	file.start_cluster = fs.current_dir;
	reset_seek();

	read_sectors = read(io_buf, 0x00000080);
	if (read_sectors == 0x00000000) return -1;

	entries     = (struct fat32_dir_entry *) io_buf;
	entries_len = read_sectors * 0x0010;

	while ((entry = next_entry(&entries, &entries_len, &entry_name)) != (void *) 0) {
		entry_cluster = (((uint32) entry->cluster_high) << 0x10) + entry->cluster_low;
		callback(entry_name, entry_cluster, entry->size);
	}

	return 0;
}

void open_cluster(uint32 cluster, uint32 size)
{
	file.size = size;
	file.start_cluster = cluster;
	reset_seek();
}

int16 open(const char *name)
{
	uint32 read_sectors;
	struct fat32_dir_entry *entries;
	uint32 entries_len;

	struct fat32_dir_entry *entry;
	char *entry_name;

	file.size          = 0x00000000;
	file.start_cluster = fs.current_dir;
	reset_seek();

	read_sectors = read(io_buf, 0x00000080);
	if (read_sectors == 0x00000000) return -1;

	entries     = (struct fat32_dir_entry *) io_buf;
	entries_len = read_sectors * 0x0010;

	while ((entry = next_entry(&entries, &entries_len, &entry_name)) != (void *) 0) {
		if (streq(entry_name, name)) {
			open_cluster((((uint32) entry->cluster_high) << 0x10) + entry->cluster_low, entry->size);
			return 0;
		}
	}

	return -1;
}

int16 open_path(const char *path)
{
	static char buf[0x0080];
	char *ptr;
	int16 error;

	for (uint16 i = 0; i < sizeof(buf); ++i) buf[i] = 0x00;

	ptr = buf;
	while (*path != 0x00 && *path != '/') {
		if (ptr == buf + sizeof(buf) - 1) {
			print_str("Path component too long\r\n");
			return -1;
		}
		*(ptr++) = *(path++);
	}

	if (*path != '/') return open(buf);
	else {
		error = chdir(*buf != 0x00 ? buf : (void *) 0);
		if (error != 0) return error;

		return open_path(path + 1);
	}
}

int16 chdir(const char *dir)
{
	int16 error;

	if (dir) {
		error = open(dir);
		if (error != 0x0000) return error;

		fs.current_dir = file.start_cluster;
	} else {
		fs.current_dir = fs.root_dir;
	}

	file.size            = 0;
	file.start_cluster   = 0;
	file.current_cluster = 0;
	file.offset          = 0;

	return 0;
}

static uint32 read_fat_entry(uint32 cluster)
{
	static uint32 buf[0x0080];
	static uint32 buf_sector = 0;

	int16  error;
	uint32 sector;
	uint32 offset;

	sector = fs.fat_start + (cluster / 0x0080);
	offset = cluster % 0x0080;

	if (sector > fs.fat_size) {
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
	file.current_cluster = file.start_cluster;
	file.offset          = 0x00000000;
}

int16 seek(uint32 sectors)
{
	uint32 remaining_sectors_in_cluster;
	uint32 next_cluster;

	if (file.start_cluster == 0x00000000) {
		print_str("No file opened\r\n");
		return -1;
	}

	while (sectors > 0) {
		remaining_sectors_in_cluster = fs.cluster_size - file.offset;

		if (sectors < remaining_sectors_in_cluster) {
			file.offset += sectors;
			return 0;
		}

		next_cluster = read_fat_entry(file.current_cluster);

		if (next_cluster >= 0x0ffffff8) {
			file.offset = fs.cluster_size;
			return 0;
		} else if (next_cluster < 0x00000002 || next_cluster == 0x0ffffff7) {
			print_str("Bad cluster\r\n");
			return -1;
		}

		file.current_cluster = next_cluster;
		file.offset = 0;

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

	if (file.start_cluster == 0x00000000) {
		print_str("No file opened\r\n");
		return 0x00000000;
	}

	sectors_read = 0;

	/* offset == cluster_size indicates seek reached EOF */
	while (sectors_read < sectors && file.offset < fs.cluster_size) {
		sectors_to_read = sectors - sectors_read;
		remaining_sectors_in_cluster = fs.cluster_size - file.offset;
		if (sectors_to_read > remaining_sectors_in_cluster) sectors_to_read = remaining_sectors_in_cluster;

		disk_sector = fs.data_start + (fs.cluster_size * (file.current_cluster - 2)) + file.offset;
		error = esp_read(buf, sectors_to_read, disk_sector);
		if (error != 0) return 0x00000000;

		seek(sectors_to_read);
		sectors_read += sectors_to_read;
		buf += sectors_to_read * 0x0200;
	}

	return sectors_read;
}

uint32 get_size()
{
	if (file.start_cluster == 0x00000000) {
		print_str("No file opened\r\n");
		return 0x00000000;
	}

	return file.size;
}
