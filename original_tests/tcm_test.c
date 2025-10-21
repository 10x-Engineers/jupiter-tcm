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

void tcm_test(void)
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
	if (memcmp(tcm_p, buf, len) != 0) {
		printf("write ddr --> tcm failed\n");
	} else {
		printf("write ddr --> tcm successfully\n");
	}

	char *buf1 = malloc(len);
	if (!buf1) {
		printf("%s_%d malloc failed\n", __func__, __LINE__);
	}

	memcpy(buf1, tcm_p, len);
	if (memcmp(tcm_p, buf1, len) != 0) {
		printf("write tcm --> ddr failed\n");
	} else {
		printf("write tcm --> ddr successfully\n");
	}

	printf("------------------%s case end-------------\n", __func__);
}
