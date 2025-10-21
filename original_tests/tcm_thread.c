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

static void *thread_func1(void *args)
{
	int id = tcm_block_cur();
	void *p = tcm_malloc(1024);
	printf("thread:%s id[%d]  p(%p)\n", __func__, id, p);
	tcm_free(p);

	tcm_block_bind_thread(1);
	id = tcm_block_cur();
	p = tcm_malloc(1024);
	printf("thread:%s id[%d]  p(%p)\n", __func__, id, p);
	tcm_free(p);
}

static void *thread_func2(void *args)
{
	int id = tcm_block_cur();
	void *p = tcm_malloc(1024);
	printf("thread:%s id[%d]  p(%p)\n", __func__, id, p);
	tcm_free(p);

	tcm_block_bind_thread(1);
	id = tcm_block_cur();
	p = tcm_malloc(1024);
	printf("thread:%s id[%d]  p(%p)\n", __func__, id, p);
	tcm_free(p);
}

void tcm_mul_thread(void)
{
	printf("------------------%s case start-------------\n", __func__);
	pthread_t pa, pb;
	pthread_create(&pa, NULL, thread_func1, NULL);
	pthread_create(&pb, NULL, thread_func2, NULL);
	pthread_join(pa, NULL);
	pthread_join(pb, NULL);

	printf("------------------%s case end-------------\n", __func__);
}
