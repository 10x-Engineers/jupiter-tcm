#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "tcm.h"
#include "udma.h"

#define TCM_MALLOC_SIZE (128*1024)

int aimm_init(void)
{
	if (tcm_init() < 0) return -1;
	if (udma_init() < 0) return -1;
	return 0;
}

int aimm_deinit(void)
{
	tcm_deinit();
	udma_deinit();
	return 0;
}

void *aimm_tcm_malloc(size_t size)
{
	if (size % TCM_MALLOC_SIZE != 0) {
		return NULL;
	}

	return tcm_malloc(size);
}

void *aimm_tcm_malloc_sync(size_t size, int timeout)
{
	if (size % TCM_MALLOC_SIZE != 0) {
		return NULL;
	}

	return tcm_malloc_sync(size, timeout);
}

void *aimm_dram_malloc(size_t size)
{
	return udma_malloc(size);
}

void *aimm_dram_realloc(void *ptr, size_t size)
{
	udma_free(ptr);
	return udma_malloc(size);
}

void *aimm_tcm_calloc(size_t nmemb, size_t size)
{
	if ((size * nmemb) % TCM_MALLOC_SIZE != 0) {
		return NULL;
	}
	return tcm_calloc(nmemb, size);
}

void *aimm_dram_calloc(size_t nmemb, size_t size)
{
	return udma_malloc(nmemb * size);
}

void aimm_tcm_free(void *ptr)
{
	tcm_free(ptr);
}

void aimm_dram_free(void *ptr)
{
	udma_free(ptr);
}

void *aimm_memcpy(void *dst, void *src, size_t size)
{
	// only support dram ---> tcm
	if (is_tcm_mm(dst) != 0 || is_udma_mm(src) != 0) {
		return NULL;
	}

	return udma_memcpy(dst, src, size);
}
