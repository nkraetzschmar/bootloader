int16 load_fat32();
int16 open(const char *file);
int16 chdir(const char *dir);
void reset_seek();
int16 seek(uint32 sectors);
uint32 read(uint8 *buf, uint32 sectors);
uint32 get_size();
