#include "types.h"
#include "loader.h"
#include "lib.h"
#include "fat32.h"
#include "io_buf.h"

struct entry {
	char title[0x0080];
	char version[0x0020];
	char sort_key[0x0020];
	char machine_id[0x0020];
	char linux[0x0100];
	char initrd[0x0100];
	char options[0x0400];
};

static struct entry highest_entry;

static void key_value(char **line, char **key, char **value)
{
	char *ptr = *line;
	*key = ptr;

	while (*ptr != ' ' && *ptr != '\n' && *ptr != 0x00) ++ptr;
	*ptr = 0x00;
	++ptr;

	while (*ptr == ' ') ++ptr;
	*value = ptr;

	while (*ptr != '\n' && *ptr != 0x00) ++ptr;
	if (*ptr == '\n') {
		*ptr = 0x00;
		++ptr;
	}

	*line = ptr;
}

static void parse_entry(struct entry *entry, char *buf)
{
	char *line;
	char *key;
	char *value;

	*entry = (struct entry) { };
	line = buf;

	while (*line) {
		key_value(&line, &key, &value);
		if (*key == 0x00 || *key == '#') continue;

		if (streq(key, "title")) {
			strcpy(entry->title, value, sizeof(entry->title));
		} else if (streq(key, "version")) {
			strcpy(entry->version, value, sizeof(entry->version));
		} else if (streq(key, "sort-key")) {
			strcpy(entry->sort_key, value, sizeof(entry->sort_key));
		} else if (streq(key, "machine-id")) {
			strcpy(entry->machine_id, value, sizeof(entry->machine_id));
		} else if (streq(key, "linux")) {
			strcpy(entry->linux, value, sizeof(entry->linux));
		} else if (streq(key, "initrd")) {
			strcpy(entry->initrd, value, sizeof(entry->initrd));
		} else if (streq(key, "options")) {
			strcpy(entry->options, value, sizeof(entry->options));
		}
	}
}

static struct {
	uint32 cluster;
	uint32 size;
} config_entries[0x0010];

static uint16 num_config_entries = 0;

static void dir_entry_callback(const char *entry_name, uint32 entry_cluster, uint32 entry_size)
{
	uint16 name_len;

	name_len = strlen(entry_name);
	if (streq(entry_name + name_len - 5, ".conf")) {
		if (num_config_entries > 0x0010) print_str("Too many config entries - ignoring excess ones\r\n");

		config_entries[num_config_entries].cluster = entry_cluster;
		config_entries[num_config_entries].size = entry_size;
		++num_config_entries;
	}
}

int16 find_entry()
{
	static struct entry entry;

	int16 error;

	error = chdir((void *) 0);
	if (error != 0) return error;
	error = chdir("loader");
	if (error != 0) return error;
	error = chdir("entries");
	if (error != 0) return error;

	error = for_each_dir_entry(dir_entry_callback);
	if (error != 0) return error;

	for (uint16 i = 0; i < num_config_entries; ++i) {
		open_cluster(config_entries[i].cluster, config_entries[i].size);
		error = read(io_buf, 0x00000020);
		if (error == 0x00000000) {
			print_str("Failed to read entry\r\n");
			return error;
		}

		parse_entry(&entry, (char *) io_buf);

		// TODO: sorting logic to decide which entry to use
		memcpy(&highest_entry, &entry, sizeof(struct entry));
	}

	if (*highest_entry.linux == 0x00) return -1;

	print_str("Selected Boot Entry: ");
	print_str(highest_entry.title);
	print_str("\r\n");
	print_str("kernel=");
	print_str(highest_entry.linux);
	print_str("\r\n");
	print_str("inintrd=");
	print_str(highest_entry.initrd);
	print_str("\r\n");
	print_str("cmdline=");
	print_str(highest_entry.options);
	print_str("\r\n");

	return 0;
}

char * get_kernel_path()
{
	return highest_entry.linux;
}

char * get_initrd_path()
{
	return highest_entry.initrd;
}

char * get_cmdline()
{
	return highest_entry.options;
}
