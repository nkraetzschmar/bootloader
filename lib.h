#pragma once

#include "prefix.h"

void PREFIX(flush)();
void PREFIX(print_str)(const char *str);

uint8 PREFIX(memeq)(const void *a, const void *b, uint16 len);
void PREFIX(memcpy)(void *dst, const void *src, uint16 len);
uint16 PREFIX(strlen)(const char *str);
uint8 PREFIX(streq)(const char *a, const char *b);
void PREFIX(strcpy)(char *dst, const char *src, uint16 len);
void PREFIX(print_hex_be)(const uint8 *buf, uint16 len);
void PREFIX(print_hex_le)(const uint8 *buf, uint16 len);
