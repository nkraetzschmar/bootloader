#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PREFIX(name) bootloader_##name

#include "types.h"
#include "loader.h"
#include "loader_internal.h"

void bootloader_print_char(char c)
{
	if (c != 0) fputc(c, stdout);
	else fflush(stdout);
}

int16 bootloader_disk_read(uint8 *, uint16, uint32)
{
	return -1;
}

int16 bootloader_disk_write(const uint8 *, uint16, uint32)
{
	return -1;
}


int main()
{
	char buf[64];

	printf("Testing cmp_alpha...\n");

	assert(bootloader_cmp_alpha("abc", "abc") == 0);
	assert(bootloader_cmp_alpha("abc", "xyz") == -1);
	assert(bootloader_cmp_alpha("xyz", "abc") == 1);
	assert(bootloader_cmp_alpha("", "abc") == -1);
	assert(bootloader_cmp_alpha("abc", "abcdef") == -1);

	printf("All tests passed!\n");

	printf("Testing cmp_version against spec examples...\n");

	// 11 == 11
	assert(bootloader_cmp_version("11", "11") == 0);

	// systemd-123 == systemd-123
	assert(bootloader_cmp_version("systemd-123", "systemd-123") == 0);

	// bar-123 < foo-123
	assert(bootloader_cmp_version("bar-123", "foo-123") == -1);
	assert(bootloader_cmp_version("foo-123", "bar-123") == 1);

	// 123a > 123
	assert(bootloader_cmp_version("123", "123a") == -1);
	assert(bootloader_cmp_version("123a", "123") == 1);

	// 123.a > 123
	assert(bootloader_cmp_version("123", "123.a") == -1);
	assert(bootloader_cmp_version("123.a", "123") == 1);

	// 123.a < 123.b
	assert(bootloader_cmp_version("123.a", "123.b") == -1);
	assert(bootloader_cmp_version("123.b", "123.a") == 1);

	// 123a > 123.a
	assert(bootloader_cmp_version("123.a", "123a") == -1);
	assert(bootloader_cmp_version("123a", "123.a") == 1);

	// 11α == 11β
	assert(bootloader_cmp_version("11α", "11β") == 0);

	// B < a
	assert(bootloader_cmp_version("B", "a") == -1);
	assert(bootloader_cmp_version("a", "B") == 1);

	// ’’ < 0
	assert(bootloader_cmp_version("", "0") == -1);
	assert(bootloader_cmp_version("0", "") == 1);

	// 0. > 0
	assert(bootloader_cmp_version("0", "0.") == -1);
	assert(bootloader_cmp_version("0.", "0") == 1);

	// 0.0 > 0
	assert(bootloader_cmp_version("0", "0.0") == -1);
	assert(bootloader_cmp_version("0.0", "0") == 1);

	// 0 > ~
	assert(bootloader_cmp_version("~", "0") == -1);
	assert(bootloader_cmp_version("0", "~") == 1);

	// ’’ > ~
	assert(bootloader_cmp_version("~", "") == -1);
	assert(bootloader_cmp_version("", "~") == 1);

	// 1_ == 1
	assert(bootloader_cmp_version("1_", "1") == 0);

	// _1 == 1
	assert(bootloader_cmp_version("_1", "1") == 0);

	// 1_ < 1.2
	assert(bootloader_cmp_version("1_", "1.2") == -1);
	assert(bootloader_cmp_version("1.2", "1_") == 1);

	// 1_2_3 > 1.3.3
	assert(bootloader_cmp_version("1.3.3", "1_2_3") == -1);
	assert(bootloader_cmp_version("1_2_3", "1.3.3") == 1);

	// 1+ == 1
	assert(bootloader_cmp_version("1+", "1") == 0);

	// +1 == 1
	assert(bootloader_cmp_version("+1", "1") == 0);

	// 1+ < 1.2
	assert(bootloader_cmp_version("1+", "1.2") == -1);
	assert(bootloader_cmp_version("1.2", "1+") == 1);

	// 1+2+3 > 1.3.3
	assert(bootloader_cmp_version("1.3.3", "1+2+3") == -1);
	assert(bootloader_cmp_version("1+2+3", "1.3.3") == 1);

	printf("All tests passed!\n");

	printf("Testing is_bad_entry...\n");

	assert(bootloader_is_bad_entry("foo.conf") == 0);
	assert(bootloader_is_bad_entry("foo+3.conf") == 0);
	assert(bootloader_is_bad_entry("foo+0.conf") == 1);
	assert(bootloader_is_bad_entry("foo+0-3.conf") == 1);
	assert(bootloader_is_bad_entry("foo+00.efi") == 1);
	assert(bootloader_is_bad_entry("foo+01.conf") == 0);

	printf("Testing tick_counters_str...\n");

	// Plain filename — pass-through
	bootloader_tick_counters_str("foo.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo.conf") == 0);

	// +N only, no -M materialized
	bootloader_tick_counters_str("foo+3.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo+2.conf") == 0);

	// +N-M: decrement left, increment done, widths preserved
	bootloader_tick_counters_str("foo+3-00.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo+2-01.conf") == 0);

	// TRIES_LEFT width preserved on decrement
	bootloader_tick_counters_str("foo+10.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo+09.conf") == 0);

	// TRIES_DONE allowed to grow
	bootloader_tick_counters_str("foo+9-99.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo+8-100.conf") == 0);

	// +0 stays at 0 (still bootable, but pointless — caller guards via is_bad)
	bootloader_tick_counters_str("foo+1.conf", buf, sizeof(buf));
	assert(strcmp(buf, "foo+0.conf") == 0);

	// UKI (.efi) suffix
	bootloader_tick_counters_str("uki+3-0.efi", buf, sizeof(buf));
	assert(strcmp(buf, "uki+2-1.efi") == 0);

	printf("All tests passed!\n");

	return 0;
}
