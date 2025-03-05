void flush();
void print_str(const char *str);

uint8 memeq(const void *a, const void *b, uint16 len);
void memcpy(void *dst, const void *src, uint16 len);
uint16 strlen(const char *str);
uint8 streq(const char *a, const char *b);
void strcpy(char *dst, const char *src, uint16 len);
void print_hex_be(const uint8 *buf, uint16 len);
void print_hex_le(const uint8 *buf, uint16 len);
