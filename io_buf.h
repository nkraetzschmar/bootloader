#pragma once

#include "types.h"
#include "prefix.h"

extern const uint32 PREFIX(io_buf_len);

uint8 * PREFIX(acquire_io_buf_hard)(void);
uint8 * PREFIX(acquire_io_buf_soft)(uint16 *notify);
void    PREFIX(release_io_buf)(void);

static inline void PREFIX(release_io_buf_cleanup)(uint8 **ptr)
{
	if (*ptr) PREFIX(release_io_buf)();
}

#define auto_release __attribute__((cleanup(PREFIX(release_io_buf_cleanup))))
