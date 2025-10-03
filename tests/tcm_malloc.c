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

void tcm_malloc_test(void)
{
#define TCM_MALLOC_CASE_LEN 1024
	printf("------------------%s case start-------------\n", __func__);

	char *p;
	char *s = malloc(TCM_MALLOC_CASE_LEN);
	if (!s) {
		printf("malloc failed, case return\n");
		return;
	}
	memset(s, 0xa5, TCM_MALLOC_CASE_LEN);

	p = tcm_aligned_alloc(2, TCM_MALLOC_CASE_LEN);
	if (!p) {
		printf("aligned alloc failed size: %d\n", TCM_MALLOC_CASE_LEN);
	}
	memset(p, 0xa5, TCM_MALLOC_CASE_LEN);
	if (memcmp(p, s, TCM_MALLOC_CASE_LEN) != 0) {
		printf("aligned alloc set failed\n");
		return;
	}
	printf("aligned alloc(%d) successfully\n", TCM_MALLOC_CASE_LEN);
	tcm_free(p);

	p = tcm_aligned_alloc(3, TCM_MALLOC_CASE_LEN);
	if (!p) {
		printf("aligned alloc: %d\n", 3);
	}

	p = tcm_malloc(TCM_MALLOC_CASE_LEN);
	if (!p) {
		printf("malloc failed size: %d\n", TCM_MALLOC_CASE_LEN);
	}
	memset(s, 0xa6, TCM_MALLOC_CASE_LEN);
	memset(p, 0xa6, TCM_MALLOC_CASE_LEN);
	if (memcmp(p, s, TCM_MALLOC_CASE_LEN) != 0) {
		printf("malloc alloc set failed\n");
		return;
	}
	printf("malloc(%d) successfully\n", TCM_MALLOC_CASE_LEN);
	tcm_free(p);

	p = tcm_calloc(1, TCM_MALLOC_CASE_LEN);
	if (!p) {
		printf("calloc failed size: %d\n", TCM_MALLOC_CASE_LEN);
	}
	memset(s, 0xa7, TCM_MALLOC_CASE_LEN);
	memset(p, 0xa7, TCM_MALLOC_CASE_LEN);
	if (memcmp(p, s, TCM_MALLOC_CASE_LEN) != 0) {
		printf("calloc set failed\n");
		return;
	}
	printf("calloc(%d) successfully\n", TCM_MALLOC_CASE_LEN);
	tcm_free(p);

	printf("------------------%s case end-------------\n", __func__);
}