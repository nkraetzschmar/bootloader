#include "types.h"
#include "io_buf.h"

enum io_buf_state {
	IO_BUF_FREE,
	IO_BUF_HARD,
	IO_BUF_SOFT,
};

static uint8 heap[0x00010000];
static enum io_buf_state state = IO_BUF_FREE;
static uint16 *soft_notify = (void *) 0;

uint8 * acquire_io_buf_hard(void)
{
	if (state == IO_BUF_HARD) return (void *) 0;

	if (state == IO_BUF_SOFT && soft_notify) *soft_notify = 0;

	state = IO_BUF_HARD;
	soft_notify = (void *) 0;
	return heap;
}

uint8 * acquire_io_buf_soft(uint16 *notify)
{
	if (state == IO_BUF_HARD) return (void *) 0;

	if (state == IO_BUF_SOFT && soft_notify) *soft_notify = 0;

	state = IO_BUF_SOFT;
	soft_notify = notify;
	if (notify) *notify = 1;
	return heap;
}

void release_io_buf(void)
{
	state = IO_BUF_FREE;
	soft_notify = (void *) 0;
}
