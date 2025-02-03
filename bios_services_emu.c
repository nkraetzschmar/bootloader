#include <unistd.h>

#include "types.h"
#include "bios_services.h"

void init();

int main()
{
	init();
}

void halt()
{
	while (1) pause();
}

void reset()
{
	_exit(0);
}

void print_char(char c)
{
	write(1, &c, 1);
}
