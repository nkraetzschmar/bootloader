void flush();
void print_str(const char *str);

void seek(uint32 sectors);
int16 read(uint8 *buffer, uint16 sectors);
