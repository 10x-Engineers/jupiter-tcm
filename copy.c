#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include "aimm.h"

#define TCM_BUF_SIZE (1024*128*4)
#define DRAM_BUF_SIZE (240*1024*1024)

int main(void)
{
	aimm_init();
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	CPU_SET(1, &mask);
	CPU_SET(2, &mask);
	CPU_SET(3, &mask);

	pid_t pid = getpid();

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
		perror("sched_setaffinity");
		return 1;
	}
	char *tcm = aimm_tcm_malloc(TCM_BUF_SIZE);
	char *dram = malloc(DRAM_BUF_SIZE);
    if (!tcm || !dram) {
		printf("aimm malloc failed(tcm:%lx)(dram:%lx)\n", (size_t)tcm, (size_t)dram);
		return -1;
	}

	printf("aimm malloc succfully(tcm:%lx)(dram:%lx)\n", (size_t)tcm, (size_t)dram);

	memset(dram, 0xa5, DRAM_BUF_SIZE);

	int loop = ((DRAM_BUF_SIZE + TCM_BUF_SIZE -1) / TCM_BUF_SIZE);
	int cpy_size = TCM_BUF_SIZE;
	int remain = DRAM_BUF_SIZE;
	int offset = 0;

	printf("aimm cpy dram ---> tcm: total(%lx) copy size(%lx) loop(%lx)\n", DRAM_BUF_SIZE, TCM_BUF_SIZE, loop);
	for (int i =0; i < loop; i++) {
		memcpy(tcm, dram + offset, cpy_size);
		if (memcmp(tcm, dram + offset, cpy_size) != 0) {
			printf("aimm memcpy fail\n");
			break;
		}

		memcpy(dram + offset, tcm, cpy_size);
		if (memcmp(tcm, dram + offset, cpy_size) != 0) {
			printf("aimm memcpy fail 1111\n");
			break;
		}

		remain -= cpy_size;
		if (remain <= 0) {
			printf("aimm memcpy succfully\n");
			break;
		}
		offset += cpy_size;
		cpy_size = (remain > cpy_size) ? cpy_size : remain;
	}

	aimm_tcm_free(tcm);
	free(dram);
	return 0;
}
