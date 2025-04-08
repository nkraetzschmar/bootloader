#include "loader.c"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

void print_char(char c)
{
	if (c != 0) fputc(c, stdout);
	else fflush(stdout);
}

int16 disk_read(uint8 *, uint16, uint32)
{
	return -1;
}


int main()
{
	printf("Testing cmp_alpha...\n");

	assert(cmp_alpha("abc", "abc") == 0);
	assert(cmp_alpha("abc", "xyz") == -1);
	assert(cmp_alpha("xyz", "abc") == 1);
	assert(cmp_alpha("", "abc") == -1);
	assert(cmp_alpha("abc", "abcdef") == -1);

	printf("All tests passed!\n");

	printf("Testing cmp_version against spec examples...\n");

	// 11 == 11
	assert(cmp_version("11", "11") == 0);

	// systemd-123 == systemd-123
	assert(cmp_version("systemd-123", "systemd-123") == 0);

	// bar-123 < foo-123
	assert(cmp_version("bar-123", "foo-123") == -1);
	assert(cmp_version("foo-123", "bar-123") == 1);

	// 123a > 123
	assert(cmp_version("123", "123a") == -1);
	assert(cmp_version("123a", "123") == 1);

	// 123.a > 123
	assert(cmp_version("123", "123.a") == -1);
	assert(cmp_version("123.a", "123") == 1);

	// 123.a < 123.b
	assert(cmp_version("123.a", "123.b") == -1);
	assert(cmp_version("123.b", "123.a") == 1);

	// 123a > 123.a
	assert(cmp_version("123.a", "123a") == -1);
	assert(cmp_version("123a", "123.a") == 1);

	// 11α == 11β
	assert(cmp_version("11α", "11β") == 0);

	// B < a
	assert(cmp_version("B", "a") == -1);
	assert(cmp_version("a", "B") == 1);

	// ’’ < 0
	assert(cmp_version("", "0") == -1);
	assert(cmp_version("0", "") == 1);

	// 0. > 0
	assert(cmp_version("0", "0.") == -1);
	assert(cmp_version("0.", "0") == 1);

	// 0.0 > 0
	assert(cmp_version("0", "0.0") == -1);
	assert(cmp_version("0.0", "0") == 1);

	// 0 > ~
	assert(cmp_version("~", "0") == -1);
	assert(cmp_version("0", "~") == 1);

	// ’’ > ~
	assert(cmp_version("~", "") == -1);
	assert(cmp_version("", "~") == 1);

	// 1_ == 1
	assert(cmp_version("1_", "1") == 0);

	// _1 == 1
	assert(cmp_version("_1", "1") == 0);

	// 1_ < 1.2
	assert(cmp_version("1_", "1.2") == -1);
	assert(cmp_version("1.2", "1_") == 1);

	// 1_2_3 > 1.3.3
	assert(cmp_version("1.3.3", "1_2_3") == -1);
	assert(cmp_version("1_2_3", "1.3.3") == 1);

	// 1+ == 1
	assert(cmp_version("1+", "1") == 0);

	// +1 == 1
	assert(cmp_version("+1", "1") == 0);

	// 1+ < 1.2
	assert(cmp_version("1+", "1.2") == -1);
	assert(cmp_version("1.2", "1+") == 1);

	// 1+2+3 > 1.3.3
	assert(cmp_version("1.3.3", "1+2+3") == -1);
	assert(cmp_version("1+2+3", "1.3.3") == 1);

	printf("All tests passed!\n");

	return 0;
}
