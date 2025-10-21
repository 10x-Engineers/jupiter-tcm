#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "tcm.h"

static int count = 0;
void tcm_rand_access(int total_cnt)
{
	printf("------------------%s case start-------------\n", __func__);
	tcm_info_t info;
	block_info_t *b_info;
	off_t base;
	size_t size = 0;;

	tcm_get_info(&info);

	b_info = calloc(sizeof(block_info_t), info.block_num);

	for (int i = 0; i < info.block_num; i++) {
		tcm_block_info_get(b_info + i, i);
		if (i == 0) {
			base = (off_t)b_info[i].paddr;
		}
		size += b_info[i].size;
	}

	printf("-----(%x)(%x)\n", base, size);

	int fd = open("/dev/mem", (O_RDWR | O_SYNC));
	if (fd < 0) {
		printf("Unable to open /dev/mem: %d\n", fd);
		return;
	}

	void *map_base = mmap(NULL,
	                      size,
	                      (PROT_READ | PROT_WRITE),
	                      MAP_SHARED,
	                      fd,
	                      (off_t)(base));

	char *start = (char *)map_base;
	size_t loop = size;
	int flag = 0;
	printf("start: %p, len:0x%x\n", start, size);
	while (loop --) {
		int offset = rand();
		char *val = start + offset % size;
		for (int i = 0; i < 4; i++) {
			memcpy(val, &offset, i);
			if (memcmp(val, &offset, i) != 0) {
				flag = 1;
				printf("rcm access failed(%p) len:%d (%x) (%x)\n", val, i, *val);
			}
		}
	}
	if (flag == 0) {
		printf("total:%d ,succfully rand access(%d)\n", total_cnt, ++count);
	}

	munmap((void *)base, size);
	close(fd);
	printf("------------------%s case end-------------\n", __func__);
}
