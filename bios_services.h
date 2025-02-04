void halt();
void reset();
void print_char(char c);
int16 disk_read(uint8 *buffer, uint16 sectors, uint32 lba);
int16 mem_move(uint8 *dst, const uint8 *src, uint32 len);
void exec_kernel();
