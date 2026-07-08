#pragma once

#include "prefix.h"

int16 PREFIX(find_esp)(void);
int16 PREFIX(esp_read)(uint8 *buffer, uint16 sectors, uint32 sector);
int16 PREFIX(esp_write)(const uint8 *buffer, uint16 sectors, uint32 sector);
