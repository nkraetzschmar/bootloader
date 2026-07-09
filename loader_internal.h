#pragma once

#include "prefix.h"

_static int16 PREFIX(cmp_alpha)(const char *a, const char *b);
_static int16 PREFIX(cmp_version)(const char *a, const char *b);
_static uint8 PREFIX(is_bad_entry)(const char *filename);
_static void  PREFIX(tick_counters_str)(const char *in, char *out, uint16 out_len);
