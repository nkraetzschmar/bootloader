#pragma once

#include "prefix.h"

int16 PREFIX(find_entry)();
char * PREFIX(get_kernel_path)();
char * PREFIX(get_initrd_path)();
char * PREFIX(get_cmdline)();
uint32 PREFIX(get_uki_cluster)();
uint32 PREFIX(get_uki_size)();
