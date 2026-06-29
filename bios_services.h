#pragma once

#include "prefix.h"

void PREFIX(halt)();
void PREFIX(reset)();
void PREFIX(print_char)(char c);
int16 PREFIX(disk_read)(uint8 *buffer, uint16 sectors, uint32 lba);
int16 PREFIX(disk_write)(const uint8 *buffer, uint16 sectors, uint32 lba);
int16 PREFIX(mem_move)(uint8 *dst, const uint8 *src, uint32 len);
void PREFIX(exec_kernel)();
