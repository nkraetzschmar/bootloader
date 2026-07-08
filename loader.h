#pragma once

#include "prefix.h"

int16 PREFIX(find_entry)(void);
char * PREFIX(get_kernel_path)(void);
char * PREFIX(get_initrd_path)(void);
char * PREFIX(get_cmdline)(void);
uint32 PREFIX(get_uki_cluster)(void);
uint32 PREFIX(get_uki_size)(void);
