#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <linux/kexec.h>

typedef __INT8_TYPE__  int8;
typedef __INT16_TYPE__ int16;
typedef __INT32_TYPE__ int32;
typedef __INT64_TYPE__ int64;

typedef __UINT8_TYPE__  uint8;
typedef __UINT16_TYPE__ uint16;
typedef __UINT32_TYPE__ uint32;
typedef __UINT64_TYPE__ uint64;

#include "setup_header.h"

#define check(X) if ((X) == -1) { perror(#X); return -1; }
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

int main() {
	int fd;
	struct stat stat;

	uint8 *purgatory;
	uint64 purgatory_len;

	uint8 *kernel;
	uint64 kernel_len;

	struct setup_header *setup_header;
	uint64 real_mode_kernel_len;
	uint64 prot_mode_kernel_len;

	struct kexec_segment seg[3];

	check((fd = open("/purgatory.bin", O_RDONLY)));
	check(fstat(fd, &stat));
	purgatory_len = stat.st_size;

	check((int64) (purgatory = mmap(NULL, purgatory_len, PROT_READ, MAP_PRIVATE, fd, 0)));
	close(fd);

	check((fd = open("/uki.efi", O_RDONLY)));
	check(fstat(fd, &stat));
	kernel_len = stat.st_size;

	check((int64) (kernel = mmap(NULL, kernel_len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)));
	close(fd);

	setup_header = (struct setup_header *) (kernel + 0x01f1);
	real_mode_kernel_len = (setup_header->setup_sects + 1) * 0x0200;
	prot_mode_kernel_len = setup_header->syssize * 0x0010;

	if (setup_header->header != 0x53726448 || real_mode_kernel_len + prot_mode_kernel_len > kernel_len) {
		fputs("invalid kernel image\n", stderr);
		return -1;
	}

	setup_header->type_of_loader = 0xff;
	setup_header->loadflags |= 0x80;
	setup_header->heap_end_ptr = 0xee00;

	seg[0] = (struct kexec_segment) {
		.buf = purgatory,
		.bufsz = purgatory_len,
		.mem = (void*) 0x00008000,
		.memsz = ALIGN_UP(purgatory_len, 0x1000)
	};

	seg[1] = (struct kexec_segment) {
		.buf = kernel,
		.bufsz = real_mode_kernel_len,
		.mem = (void*) 0x00010000,
		.memsz = ALIGN_UP(real_mode_kernel_len, 0x1000)
	};

	seg[2] = (struct kexec_segment) {
		.buf = kernel + real_mode_kernel_len,
		.bufsz = prot_mode_kernel_len,
		.mem = (void*) 0x00100000,
		.memsz = ALIGN_UP(prot_mode_kernel_len, 0x1000)
	};

	check(syscall(SYS_kexec_load, 0x8000, 3, &seg, 0));
	check(reboot(RB_KEXEC));
}
