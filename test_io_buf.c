#include <assert.h>
#include <stdio.h>

#define PREFIX(name) bootloader_##name

#include "types.h"
#include "io_buf.h"

int main(void)
{
	uint8 *a;
	uint8 *b;
	uint16 notify1;
	uint16 notify2;

	printf("Testing hard acquire blocks hard acquire...\n");
	a = bootloader_acquire_io_buf_hard();
	assert(a != (void *) 0);
	b = bootloader_acquire_io_buf_hard();
	assert(b == (void *) 0);
	bootloader_release_io_buf();

	printf("Testing hard acquire blocks soft acquire...\n");
	a = bootloader_acquire_io_buf_hard();
	assert(a != (void *) 0);
	notify1 = 0xffff;
	b = bootloader_acquire_io_buf_soft(&notify1);
	assert(b == (void *) 0);
	assert(notify1 == 0xffff);
	bootloader_release_io_buf();

	printf("Testing soft acquire lets hard acquire through, notify fires...\n");
	notify1 = 0;
	a = bootloader_acquire_io_buf_soft(&notify1);
	assert(a != (void *) 0);
	assert(notify1 == 1);
	b = bootloader_acquire_io_buf_hard();
	assert(b != (void *) 0);
	assert(notify1 == 0);
	bootloader_release_io_buf();

	printf("Testing soft acquire lets soft acquire through, prior notify fires...\n");
	notify1 = 0;
	notify2 = 0;
	a = bootloader_acquire_io_buf_soft(&notify1);
	assert(a != (void *) 0);
	assert(notify1 == 1);
	b = bootloader_acquire_io_buf_soft(&notify2);
	assert(b != (void *) 0);
	assert(notify1 == 0);
	assert(notify2 == 1);
	bootloader_release_io_buf();

	printf("All tests passed!\n");
	return 0;
}
