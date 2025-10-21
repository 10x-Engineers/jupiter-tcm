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

#include "tcm.h"

void tcm_mmap(void)
{
	printf("------------------%s case start-------------\n", __func__);

	void *tcm_p = tcm_malloc(1024);
	if (tcm_p) {
		printf("tcm alloc successfully!(%p)\n", tcm_p);
	} else {
		printf("tcm alloc failed!\n");
	}

	char buf[]  = "hello tcm";
	int len     = strlen(buf) + 1;

	memcpy(tcm_p, buf, len);

	tcm_info_t info;
	tcm_block_info_t b_info;
	tcm_get_info(&info);
	tcm_block_info_get(&b_info, 0);

	int fd = open("/dev/mem", (O_RDWR | O_SYNC));
	if (fd < 0) {
		printf("Unable to open /dev/mem: %d\n", fd);
		return;
	}

	void *map_base = mmap(NULL,
	                      b_info.size,
	                      (PROT_READ | PROT_WRITE),
	                      MAP_SHARED,
	                      fd,
	                      (off_t)(b_info.paddr));
	char *show = (char *)((size_t)(tcm_p) - (size_t)b_info.vaddr + (size_t)map_base);

	if (memcmp(tcm_p, show, len) != 0) {
		printf("/dev/mem check failed\n");
	} else {
		printf("/dev/mem check successfully\n");
	}

	munmap(b_info.paddr, b_info.size);

	free(buf1);
	tcm_free(tcm_p);
	printf("------------------%s case end-------------\n", __func__);
}
