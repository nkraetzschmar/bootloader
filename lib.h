void flush();
void print_str(const char *str);

void reset_seek();
void seek(uint32 sectors);
int16 read(uint8 *buffer, uint16 sectors);
