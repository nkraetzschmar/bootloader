#pragma once

#include "prefix.h"

int16 PREFIX(load_fat32)(void);
int16 PREFIX(open)(const char *file);
void PREFIX(open_cluster)(uint32 cluster, uint32 size);
int16 PREFIX(open_path)(const char *path);
int16 PREFIX(chdir)(const char *dir);
int16 PREFIX(unlink)(void);
int16 PREFIX(link)(const char *name);
int16 PREFIX(rename)(const char *name);
int16 PREFIX(for_each_dir_entry)(void (*callback)(const char *entry_name, uint32 entry_cluster, uint32 entry_size));
void PREFIX(reset_seek)(void);
int16 PREFIX(seek)(uint32 sectors);
uint32 PREFIX(read)(uint8 *buf, uint32 sectors);
uint32 PREFIX(write)(const uint8 *buf, uint32 sectors);
uint32 PREFIX(get_size)(void);
