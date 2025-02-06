#include "types.h"
#include "gpt.h"
#include "bios_services.h"
#include "lib.h"
#include "io_buf.h"

struct gpt_header {
	uint8  signature[0x0008];
	uint16 revision[0x0002];
	uint32 header_size;
	uint32 header_crc32;
	uint32 reserved;
	uint64 header_lba;
	uint64 backup_lba;
	uint64 first_block;
	uint64 last_block;
	uint8  guid[0x0010];
	uint64 entries_lba;
	uint32 num_entries;
	uint32 entry_size;
	uint32 entries_crc32;
};

static_assert(sizeof(struct gpt_header) == 0x005c);

struct gpt_entry {
	uint8  type[0x0010];
	uint8  guid[0x0010];
	uint64 start;
	uint64 end;
	uint64 attributes;
	uint16 partition_name[0x0024];
};

static_assert(sizeof(struct gpt_entry) == 0x0080);

static void print_guid(uint8 *guid)
{
	print_hex_le(guid + 0x00, 0x04);
	print_char('-');
	print_hex_le(guid + 0x04, 0x02);
	print_char('-');
	print_hex_le(guid + 0x06, 0x02);
	print_char('-');
	print_hex_be(guid + 0x08, 0x02);
	print_char('-');
	print_hex_be(guid + 0x0a, 0x06);
}

static int16 check_header(struct gpt_header *gpt_header)
{
	if (!memeq(gpt_header->signature, "EFI PART", 0x0008)) {
		print_str("Not a GPT partitioned disk\r\n");
		return -1;
	}

	if (gpt_header->entries_lba.low != 0x00000002 || gpt_header->entries_lba.high != 0x00000000) {
		print_str("Unsupported GPT config: entries not at LBA 2\r\n");
		return -1;
	}

	if (gpt_header->num_entries > 0x00000080) {
		print_str("Unsupported GPT config: too many entries\r\n");
		return -1;
	}

	if (gpt_header->entry_size != 0x00000080) {
		print_str("Unsupported GPT config: non standard entry size\r\n");
		return -1;
	}

	print_str("Found GPT disk: ");
	print_guid((uint8 *) &gpt_header->guid);
	print_str("\r\n");

	return 0;
}

static const uint8 esp_type[0x0010] = { 0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b };

static uint32 esp_start = 0;
static uint32 esp_end;

int16 find_esp()
{
	int16 error;
	struct gpt_header *header;
	struct gpt_entry *entries;

	error = disk_read(io_buf, 0x0020, 0x0001);
	if (error != 0) return error;

	header = (struct gpt_header *) io_buf;
	entries = (struct gpt_entry *) (io_buf + 0x0200);

	error = check_header(header);
	if (error != 0) return error;

	for (uint16 i = 0; i < header->num_entries; ++i) {
		if (memeq(&entries[i].type, esp_type, 0x0010)) {
			if (entries[i].start.high != 0x0000 || entries[i].end.high != 0x0000) {
				print_str("ESP partition out of range\r\n");
				return -1;
			}

			esp_start = entries[i].start.low;
			esp_end = entries[i].end.low;

			if (esp_end < esp_start) {
				print_str("Invalid partition\r\n");
				return -1;
			}

			print_str("Found ESP partition: ");
			print_guid((uint8 *) &entries[i].guid);
			print_str("\r\n");
		}
	}

	if (esp_start == 0) {
		print_str("No ESP partition found\r\n");
		return 0;
	}

	print_str("Using ESP partiton @");
	print_hex_le((uint8 *) &esp_start, 0x0004);
	print_str("\r\n");

	return 0;
}

int16 esp_read(uint8 *buffer, uint16 sectors, uint32 sector)
{
	uint32 lba;

	if (!esp_start) {
		print_str("No partition loaded yet\r\n");
		return -1;
	}

	lba = esp_start + sector;
	if (lba > esp_end) {
		print_str("Sector beyond partition end\r\n");
		return -1;
	}

	return disk_read(buffer, sectors, lba);
}
