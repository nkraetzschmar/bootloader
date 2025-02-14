int16 load_fat32();
int16 open(const char *file);
void reset_seek();
int16 seek(uint32 sectors);
uint32 read(uint8 *buf, uint32 sectors);
uint32 get_size();
