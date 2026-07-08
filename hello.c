#include <stdio.h>
#include <sys/reboot.h>

int main()
{
	puts("hello from the initrd");
	reboot(RB_POWER_OFF);
	return 0;
}
