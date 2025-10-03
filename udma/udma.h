#ifndef __UDMA_H__
#define __UDMA_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int udma_init(void);
int udma_deinit(void);
void *udma_malloc(size_t size);
void udma_free(void *ptr);
void *udma_memcpy(void *dest, const void *src, size_t n);
int is_udma_mm(void *p);

#ifdef __cplusplus
}
#endif
#endif
