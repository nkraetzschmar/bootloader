typedef __INT8_TYPE__  int8;
typedef __INT16_TYPE__ int16;
typedef __INT32_TYPE__ int32;

typedef __UINT8_TYPE__  uint8;
typedef __UINT16_TYPE__ uint16;
typedef __UINT32_TYPE__ uint32;

typedef struct { uint32 low; uint32 high; } uint64;
static_assert(sizeof(uint64) == 0x08);
