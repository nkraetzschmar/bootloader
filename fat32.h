#pragma once

#include "prefix.h"

struct dir_entry_handle {
	uint32 dir_cluster;
	int32  entry_index;
};

int16 PREFIX(load_fat32)(void);
int16 PREFIX(open)(const char *file);
void PREFIX(open_cluster)(uint32 cluster, uint32 size);
int16 PREFIX(open_path)(const char *path);
int16 PREFIX(chdir)(const char *dir);
int16 PREFIX(unlink)(void);
int16 PREFIX(link)(const char *name);
int16 PREFIX(rename)(const char *name);
int16 PREFIX(for_each_dir_entry)(void (*callback)(const char *entry_name, int32 entry_index, uint32 entry_cluster, uint32 entry_size));
struct dir_entry_handle PREFIX(make_handle)(int32 entry_index);
int16 PREFIX(open_handle)(struct dir_entry_handle h);
int16 PREFIX(get_file_name)(char *name_out, uint16 name_out_len);
void PREFIX(reset_seek)(void);
int16 PREFIX(seek)(uint32 sectors);
uint32 PREFIX(read)(uint8 *buf, uint32 sectors);
uint32 PREFIX(write)(const uint8 *buf, uint32 sectors);
uint32 PREFIX(get_size)(void);
