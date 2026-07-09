#include "types.h"
#include "fat32.h"
#include "fat32_internal.h"
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
	int32  entry_index;
} file;

static struct {
	uint32 current_dir;
	uint16 is_valid;
	struct fat32_dir_entry *buf;
	uint32 num_entries;
} dir_cache;

static int16 load_dir(void);
static int16 write_dir(void);
static int16 unlink_loaded(void);
static int16 link_loaded(const char *name);

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

int16 load_fat32(void)
{
	int16 error;
	auto_release uint8 *io_buf = (void *) 0;
	struct fat32_bpb *bpb;

	io_buf = acquire_io_buf_hard();
	if (!io_buf) {
		print_str("Failed to acquire io_buf\r\n");
		return -1;
	}

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
	file.entry_index     = -1;

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

static int16 load_dir(void)
{
	uint8 *io_buf;
	uint8 file_save[sizeof(file)];

	if (dir_cache.is_valid && dir_cache.current_dir == fs.current_dir) return 0;

	io_buf = acquire_io_buf_soft(&dir_cache.is_valid);
	if (!io_buf) {
		print_str("Failed to acquire io_buf\r\n");
		return -1;
	}

	dir_cache.buf         = (struct fat32_dir_entry *) io_buf;
	dir_cache.current_dir = fs.current_dir;
	dir_cache.num_entries = io_buf_len / sizeof(struct fat32_dir_entry);

	memcpy(file_save, &file, sizeof(file));

	file.size          = 0x00000000;
	file.start_cluster = fs.current_dir;
	reset_seek();

	if (read((uint8 *) dir_cache.buf, io_buf_len / 0x0200) == 0) {
		dir_cache.is_valid = 0;
		memcpy(&file, file_save, sizeof(file));
		return -1;
	}

	memcpy(&file, file_save, sizeof(file));
	return 0;
}

static int16 write_dir(void)
{
	uint8 file_save[sizeof(file)];
	uint32 written;

	memcpy(file_save, &file, sizeof(file));

	file.size          = 0x00000000;
	file.start_cluster = fs.current_dir;
	reset_seek();

	written = write((const uint8 *) dir_cache.buf, io_buf_len / 0x0200);

	memcpy(&file, file_save, sizeof(file));

	if (written == 0) return -1;
	return 0;
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

int16 for_each_dir_entry(void (*callback)(const char *entry_name, int32 entry_index, uint32 entry_cluster, uint32 entry_size))
{
	int16 error;
	struct fat32_dir_entry *entries_start;
	struct fat32_dir_entry *entries;
	uint32 entries_len;
	struct fat32_dir_entry *entry;
	char *entry_name;
	uint32 entry_cluster;

	error = load_dir();
	if (error != 0) return error;

	entries_start = dir_cache.buf;
	entries       = entries_start;
	entries_len   = dir_cache.num_entries;

	while ((entry = next_entry(&entries, &entries_len, &entry_name)) != (void *) 0) {
		entry_cluster = (((uint32) entry->cluster_high) << 0x10) + entry->cluster_low;
		callback(entry_name, (int32)(entry - entries_start), entry_cluster, entry->size);
	}

	return 0;
}

struct dir_entry_handle make_handle(int32 entry_index)
{
	struct dir_entry_handle h;
	h.dir_cluster = fs.current_dir;
	h.entry_index = entry_index;
	return h;
}

int16 open_handle(struct dir_entry_handle h)
{
	int16 error;
	struct fat32_dir_entry *entry;

	fs.current_dir = h.dir_cluster;

	error = load_dir();
	if (error != 0) return error;

	if (h.entry_index < 0 || (uint32) h.entry_index >= dir_cache.num_entries) {
		print_str("Handle entry_index out of range\r\n");
		return -1;
	}

	entry = &dir_cache.buf[h.entry_index];

	open_cluster((((uint32) entry->cluster_high) << 0x10) + entry->cluster_low, entry->size);
	file.entry_index = h.entry_index;

	return 0;
}

int16 get_file_name(char *name_out, uint16 name_out_len)
{
	int16 error;
	struct fat32_dir_entry *entries_start;
	struct fat32_dir_entry *entries;
	uint32 entries_len;
	struct fat32_dir_entry *entry;
	char *entry_name;
	uint16 i;

	if (file.entry_index < 0) {
		print_str("No file opened\r\n");
		return -1;
	}

	error = load_dir();
	if (error != 0) return error;

	entries_start = dir_cache.buf;
	entries       = entries_start;
	entries_len   = dir_cache.num_entries;

	while ((entry = next_entry(&entries, &entries_len, &entry_name)) != (void *) 0) {
		if ((int32)(entry - entries_start) == file.entry_index) {
			for (i = 0; i + 1 < name_out_len && entry_name[i] != 0x00; ++i) name_out[i] = entry_name[i];
			name_out[i] = 0x00;
			return 0;
		}
	}

	print_str("Entry index not found in directory\r\n");
	return -1;
}

void open_cluster(uint32 cluster, uint32 size)
{
	file.size = size;
	file.start_cluster = cluster;
	file.entry_index = -1;
	reset_seek();
}

int16 open(const char *name)
{
	int16 error;
	struct fat32_dir_entry *entries_start;
	struct fat32_dir_entry *entries;
	uint32 entries_len;
	struct fat32_dir_entry *entry;
	char *entry_name;

	error = load_dir();
	if (error != 0) return error;

	file.size          = 0x00000000;
	file.start_cluster = fs.current_dir;
	file.entry_index   = -1;
	reset_seek();

	entries_start = dir_cache.buf;
	entries       = entries_start;
	entries_len   = dir_cache.num_entries;

	while ((entry = next_entry(&entries, &entries_len, &entry_name)) != (void *) 0) {
		if (streq(entry_name, name)) {
			open_cluster((((uint32) entry->cluster_high) << 0x10) + entry->cluster_low, entry->size);
			file.entry_index = (int32)(entry - entries_start);
			return 0;
		}
	}

	return -1;
}

int16 unlink(void)
{
	int16 error;

	error = load_dir();
	if (error != 0) return error;

	error = unlink_loaded();
	if (error != 0) return error;

	return write_dir();
}

static int16 unlink_loaded(void)
{
	struct fat32_dir_entry *entries;
	int32 lfn_start;
	int32 i;

	if (!dir_cache.is_valid || dir_cache.current_dir != fs.current_dir) {
		print_str("Directory cache not loaded\r\n");
		return -1;
	}

	entries = dir_cache.buf;

	if (file.entry_index < 0) {
		print_str("No file opened\r\n");
		return -1;
	}

	if (entries[file.entry_index].attr & 0x10) {
		print_str("Cannot unlink directory\r\n");
		return -1;
	}

	lfn_start = file.entry_index;
	for (i = file.entry_index - 1; i >= 0; --i) {
		if (entries[i].attr == 0x0F)
			lfn_start = i;
		else
			break;
	}

	for (i = lfn_start; i <= file.entry_index; ++i) {
		*((uint8 *) entries[i].name) = 0xe5;
	}

	return 0;
}

static uint16 lfn_entries_needed(const char *name)
{
	return (strlen(name) + 12) / 13;
}

static uint8 check_slot(struct fat32_dir_entry *entries_start, uint32 total_entries, uint32 index, uint32 count)
{
	uint8 first_byte;

	if (index + count > total_entries) return 0;

	for (uint32 i = 0; i < count; ++i) {
		first_byte = *((uint8 *) entries_start[index + i].name);
		if (first_byte != 0xe5 && first_byte != 0x00) return 0;
	}

	return 1;
}

static int32 find_free_slot(struct fat32_dir_entry *entries_start, uint32 total_entries, uint32 count)
{
	uint32 run_start;
	uint32 run_len;
	uint8 first_byte;

	run_start = 0;
	run_len = 0;

	for (uint32 i = 0; i < total_entries; ++i) {
		first_byte = *((uint8 *) entries_start[i].name);
		if (first_byte == 0xe5 || first_byte == 0x00) {
			if (run_len == 0) run_start = i;
			run_len += 1;
			if (run_len >= count) return (int32) run_start;
		} else {
			run_len = 0;
		}
	}

	return -1;
}

static void make_short_name(const char *name, char *short_name)
{
	uint16 i;
	uint16 j;
	const char *dot;
	char c;

	for (i = 0; i < 11; ++i) short_name[i] = ' ';

	dot = (void *) 0;
	for (i = 0; name[i] != 0x00; ++i) {
		if (name[i] == '.') dot = &name[i];
	}

	j = 0;
	for (i = 0; name[i] != 0x00 && j < 6; ++i) {
		if (&name[i] == dot) break;
		c = name[i];
		if (c >= 'a' && c <= 'z') c = 'A' + (c - 'a');
		short_name[j++] = c;
	}
	short_name[6] = '~';
	short_name[7] = '1';

	if (dot) {
		j = 8;
		for (i = 1; dot[i] != 0x00 && j < 11; ++i) {
			c = dot[i];
			if (c >= 'a' && c <= 'z') c = 'A' + (c - 'a');
			short_name[j++] = c;
		}
	}
}

static uint8 lfn_checksum(const char *short_name)
{
	uint8 sum;

	sum = 0;
	for (uint16 i = 0; i < 11; ++i) {
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8) short_name[i];
	}

	return sum;
}

static void write_lfn_entry(struct fat32_lfn_entry *entry, const char *name, uint16 name_len, uint8 seq, uint8 checksum)
{
	uint16 offset;
	uint16 i;
	uint16 ch;

	entry->seq = seq;
	entry->attr = 0x0F;
	entry->reserved = 0x00;
	entry->checksum = checksum;
	entry->zero = 0x0000;

	offset = ((seq & 0x3f) - 1) * 13;

	for (i = 0; i < 5; ++i) {
		ch = (offset + i < name_len) ? (uint16)(uint8) name[offset + i] : (offset + i == name_len) ? 0x0000 : 0xFFFF;
		entry->name1[i] = ch;
	}
	for (i = 0; i < 6; ++i) {
		ch = (offset + 5 + i < name_len) ? (uint16)(uint8) name[offset + 5 + i] : (offset + 5 + i == name_len) ? 0x0000 : 0xFFFF;
		entry->name2[i] = ch;
	}
	for (i = 0; i < 2; ++i) {
		ch = (offset + 11 + i < name_len) ? (uint16)(uint8) name[offset + 11 + i] : (offset + 11 + i == name_len) ? 0x0000 : 0xFFFF;
		entry->name3[i] = ch;
	}
}

static void write_short_entry(struct fat32_dir_entry *entry, const char *short_name, uint32 cluster, uint32 size)
{
	for (uint16 i = 0; i < 8; ++i) entry->name[i] = short_name[i];
	for (uint16 i = 0; i < 3; ++i) entry->ext[i] = short_name[8 + i];
	entry->attr = 0x20;
	entry->reserved = 0x00;
	entry->time_ts = 0x00;
	entry->time = 0x0000;
	entry->date = 0x0000;
	entry->access_date = 0x0000;
	entry->cluster_high = (uint16)(cluster >> 0x10);
	entry->mod_time = 0x0000;
	entry->mod_date = 0x0000;
	entry->cluster_low = (uint16)(cluster & 0xffff);
	entry->size = size;
}

int16 link(const char *name)
{
	int16 error;

	error = load_dir();
	if (error != 0) return error;

	error = link_loaded(name);
	if (error != 0) return error;

	return write_dir();
}

static int16 link_loaded(const char *name)
{
	struct fat32_dir_entry *entries;
	uint16 lfn_count;
	uint32 needed;
	int32 slot;

	char short_name[11];
	uint8 checksum;
	uint16 name_len;

	uint32 cluster;
	uint32 size;

	if (!dir_cache.is_valid || dir_cache.current_dir != fs.current_dir) {
		print_str("Directory cache not loaded\r\n");
		return -1;
	}

	entries = dir_cache.buf;

	cluster = file.start_cluster;
	size    = file.size;

	lfn_count = lfn_entries_needed(name);
	needed = lfn_count + 1;

	slot = -1;
	if (file.entry_index >= 0) {
		int32 candidate;
		candidate = file.entry_index - (int32) lfn_count;
		if (candidate >= 0 && check_slot(entries, dir_cache.num_entries, (uint32) candidate, needed)) {
			slot = candidate;
		}
	}
	if (slot < 0) {
		slot = find_free_slot(entries, dir_cache.num_entries, needed);
	}
	if (slot < 0) {
		print_str("No free directory entries\r\n");
		return -1;
	}

	make_short_name(name, short_name);
	checksum = lfn_checksum(short_name);
	name_len = strlen(name);

	for (uint16 i = 0; i < lfn_count; ++i) {
		uint8 seq;
		seq = (lfn_count - i);
		if (i == 0) seq |= 0x40;
		write_lfn_entry(
			(struct fat32_lfn_entry *) &entries[slot + i],
			name, name_len, seq, checksum
		);
	}

	write_short_entry(&entries[slot + lfn_count], short_name, cluster, size);

	file.start_cluster = cluster;
	file.size          = size;
	file.entry_index   = slot + (int32) lfn_count;

	return 0;
}

int16 rename(const char *name)
{
	int16 error;

	error = load_dir();
	if (error != 0) return error;

	error = unlink_loaded();
	if (error != 0) return error;

	error = link_loaded(name);
	if (error != 0) return error;

	return write_dir();
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
		if (error != 0) return error;

		fs.current_dir = file.start_cluster;
	} else {
		fs.current_dir = fs.root_dir;
	}

	file.size            = 0;
	file.start_cluster   = 0;
	file.current_cluster = 0;
	file.offset          = 0;
	file.entry_index     = -1;

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

void reset_seek(void)
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

uint32 write(const uint8 *buf, uint32 sectors)
{
	int16 error;
	uint32 sectors_written;
	uint32 remaining_sectors_in_cluster;
	uint32 sectors_to_write;
	uint32 disk_sector;

	if (file.start_cluster == 0x00000000) {
		print_str("No file opened\r\n");
		return 0x00000000;
	}

	sectors_written = 0;

	while (sectors_written < sectors && file.offset < fs.cluster_size) {
		sectors_to_write = sectors - sectors_written;
		remaining_sectors_in_cluster = fs.cluster_size - file.offset;
		if (sectors_to_write > remaining_sectors_in_cluster) sectors_to_write = remaining_sectors_in_cluster;

		disk_sector = fs.data_start + (fs.cluster_size * (file.current_cluster - 2)) + file.offset;
		error = esp_write(buf, sectors_to_write, disk_sector);
		if (error != 0) return 0x00000000;

		seek(sectors_to_write);
		sectors_written += sectors_to_write;
		buf += sectors_to_write * 0x0200;
	}

	return sectors_written;
}

uint32 get_size(void)
{
	if (file.start_cluster == 0x00000000) {
		print_str("No file opened\r\n");
		return 0x00000000;
	}

	return file.size;
}

#ifdef NO_STATIC_INTERNALS
int32 get_entry_index(void)
{
	return file.entry_index;
}
#endif
