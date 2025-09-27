#include "types.h"
#include "loader.h"
#include "lib.h"
#include "fat32.h"
#include "io_buf.h"
#include "setup_header.h"

struct entry {
	char title[0x0080];
	char version[0x0020];
	char sort_key[0x0020];
	char machine_id[0x0020];
	char linux[0x0100];
	char initrd[0x0100];
	char options[0x0400];
	uint32 uki_cluster;
	uint32 uki_size;
};

static struct entry highest_entry = { };

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

static uint16 num_config_entries;

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

static void dir_entry_callback_uki(const char *entry_name, uint32 entry_cluster, uint32 entry_size)
{
	uint16 name_len;

	name_len = strlen(entry_name);
	if (streq(entry_name + name_len - 4, ".efi")) {
		if (num_config_entries > 0x0010) print_str("Too many config entries - ignoring excess ones\r\n");

		config_entries[num_config_entries].cluster = entry_cluster;
		config_entries[num_config_entries].size = entry_size;
		++num_config_entries;
	}
}

static int16 cmp_alpha(const char *a, const char *b)
{
	while (*a) {
		if (*a > *b) return 1;
		if (*b > *a) return -1;
		++a;
		++b;
	}

	if (*b) return -1;
	return 0;
}

static uint16 str_to_uint16(const char *str, const char **end_ptr)
{
	uint16 value;
	uint8 digit;

	value = 0;

	while (*str) {
		if (*str < '0' || *str > '9') break;
		digit = *str - '0';

		if (value > 0xffff / 10) return 0xffff;
		value *= 10;

		if (value > 0xffff - digit) return 0xffff;

		value += digit;
		str++;
	}

	if (end_ptr) *end_ptr = str;
	return value;
}

static uint8 is_version_char(char c)
{
	if (c >= '0' && c <= '9') return 1;
	if (c >= 'a' && c <= 'z') return 1;
	if (c >= 'A' && c <= 'Z') return 1;
	if (c == '.' || c == '-' || c == '~' || c == '^') return 1;
	return 0;
}

static int16 cmp_version(const char *a, const char *b)
{
	uint16 int_a, int_b;

	/* https://uapi-group.org/specifications/specs/version_format_specification/ (2025-03-23)
	 *
	 * Both strings are compared from the beginning until the end, or until the strings are found
	 * to compare as different. In a loop:
	 */

	while (1) {
		/* Any characters which are outside of the set of listed above (a-z, A-Z, 0-9, -, ., ~, ^)
		 * are skipped in both strings. In particular, this means that non-ASCII characters that
		 * are Unicode digits or letters are skipped too.
		 */

		while (*a && !is_version_char(*a)) ++a;
		while (*b && !is_version_char(*b)) ++b;

		/* If the remaining part of one of strings starts with ~: if other remaining part does
		 * not start with ~, the string with ~ compares lower. Otherwise, both tilde characters
		 * are skipped.
		 */

		if (*a == '~') {
			if (*b != '~') return -1;
			++a;
			++b;
			continue;
		} else if (*b == '~') return 1;

		/* If one of the strings has ended: if the other string hasn’t, the string that has remaining
		 * characters compares higher. Otherwise, the strings compare equal.
		 */

		if (*a == 0x00 && *b == 0x00) return 0;
		if (*b == 0x00) return 1;
		if (*a == 0x00) return -1;

		/* If the remaining part of one of strings starts with -: if the other remaining part does not
		 * start with -, the string with - compares lower. Otherwise, both minus characters are skipped.
		 */

		if (*a == '-') {
			if (*b != '-') return -1;
			++a;
			++b;
			continue;
		} else if (*b == '-') return 1;

		/* If the remaining part of one of strings starts with ^: if the other remaining part does not
		 * start with ^, the string with ^ compares lower. Otherwise, both caret characters are skipped.
		 */

		if (*a == '^') {
			if (*b != '^') return -1;
			++a;
			++b;
			continue;
		} else if (*b == '^') return 1;

		/* If the remaining part of one of strings starts with .: if the other remaining part does not
		 * start with ., the string with . compares lower. Otherwise, both dot characters are skipped.
		 */

		if (*a == '.') {
			if (*b != '.') return -1;
			++a;
			++b;
			continue;
		} else if (*b == '.') return 1;

		/* If either of the remaining parts starts with a digit: numerical prefixes are compared numerically.
		 * Any leading zeroes are skipped. The numerical prefixes (until the first non-digit character) are
		 * evaluated as numbers. If one of the prefixes is empty, it evaluates as 0. If the numbers are different,
		 * the string with the bigger number compares higher. Otherwise, the comparison continues at the following
		 * characters at point 1.
		 */

		if ((*a >= '0' && *a <= '9') || (*b >= '0' && *b <= '9')) {
			int_a = str_to_uint16(a, &a);
			int_b = str_to_uint16(b, &b);

			if (int_a > int_b) return 1;
			if (int_b > int_a) return -1;

			continue;
		}

		/* Leading alphabetical prefixes are compared alphabetically. The substrings are compared letter-by-letter.
		 * If both letters are the same, the comparison continues with the next letter. All capital letters compare
		 * lower than lower-case letters (B < a). When the end of one substring has been reached (a non-letter
		 * character or the end of the whole string), if the other substring has remaining letters, it compares higher.
		 * Otherwise, the comparison continues at the following characters at point 1.
		 */

		// We already know that both chars must be [a-zA-Z] => simple comparison is enough
		if (*a > *b) return 1;
		if (*b > *a) return -1;

		++a;
		++b;
	}
}

static int16 cmp_entry(const struct entry *a, const struct entry *b)
{
	int16 cmp;

	if (a->uki_cluster != 0 && b->uki_cluster == 0) return 1;
	if (b->uki_cluster != 0 && a->uki_cluster == 0) return -1;

	cmp = cmp_alpha(a->sort_key, b->sort_key);
	if (cmp != 0) return cmp;

	cmp = cmp_alpha(a->machine_id, b->machine_id);
	if (cmp != 0) return cmp;

	return cmp_version(a->version, b->version);
}

int16 find_entry_conf()
{
	static struct entry entry;

	int16 error;

	error = chdir((void *) 0);
	if (error != 0) return error;
	error = chdir("loader");
	if (error != 0) return 0;
	error = chdir("entries");
	if (error != 0) return 0;

	num_config_entries = 0;
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

		if (cmp_entry(&entry, &highest_entry) == 1) memcpy(&highest_entry, &entry, sizeof(struct entry));
	}

	return 0;
}

int16 find_entry_uki()
{
	static struct entry entry;

	int16 error;
	struct setup_header *setup_header;
	char *version_str;

	error = chdir((void *) 0);
	if (error != 0) return error;
	error = chdir("EFI");
	if (error != 0) return 0;
	error = chdir("Linux");
	if (error != 0) return 0;

	num_config_entries = 0;
	error = for_each_dir_entry(dir_entry_callback_uki);
	if (error != 0) return error;

	for (uint16 i = 0; i < num_config_entries; ++i) {
		open_cluster(config_entries[i].cluster, config_entries[i].size);
		error = read(io_buf, 0x00000002);
		if (error == 0x00000000) {
			print_str("Failed to read entry\r\n");
			return error;
		}

		setup_header = (struct setup_header *) (io_buf + 0x01f1);
		if (setup_header->header != 0x53726448) continue;
		if (setup_header->kernel_version > 0x0100) continue;

		version_str = (char *) io_buf + setup_header->kernel_version + 0x0200;
		if (!memeq(version_str, "UKI:", 0x04)) continue;

		entry = (struct entry) { };
		entry.uki_cluster = config_entries[i].cluster;
		entry.uki_size = config_entries[i].size;
		strcpy(entry.version, version_str, sizeof(entry.version));

		if (cmp_entry(&entry, &highest_entry) == 1) memcpy(&highest_entry, &entry, sizeof(struct entry));
	}

	return 0;
}

int16 find_entry()
{
	int16 error;

	error = find_entry_conf();
	if (error != 0) return error;

	error = find_entry_uki();
	if (error != 0) return error;

	if (*highest_entry.linux == 0x00 && highest_entry.uki_cluster == 0x00) return -1;

	print_str("Selected Boot Entry: ");
	if (highest_entry.uki_cluster) {
		print_str("UKI\r\n");
		print_str("cluster=");
		print_hex_le((uint8 *) &highest_entry.uki_cluster, 0x04);
		print_str("\r\n");
	} else {
		print_str(highest_entry.title);
		print_str("\r\n");
		print_str("kernel=");
		print_str(highest_entry.linux);
		print_str("\r\n");
		print_str("initrd=");
		print_str(highest_entry.initrd);
		print_str("\r\n");
		print_str("cmdline=");
		print_str(highest_entry.options);
		print_str("\r\n");
	}

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

uint32 get_uki_cluster()
{
	return highest_entry.uki_cluster;
}

uint32 get_uki_size()
{
	return highest_entry.uki_size;
}
