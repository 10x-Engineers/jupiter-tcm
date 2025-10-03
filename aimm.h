#ifndef __AIMM_H__
#define __AIMM_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int aimm_init(void);

/**
  \brief       De-initialize Tcm Interface.
  \param[in]   None
  \return      Error code
*/
int aimm_deinit(void);

/**
  \brief       Allocates size bytes and returns a pointer to the allocated memory 
  \param[in]   size    allocated memory size(bytes)
  \param[in]   timeout    allocated wait time(ms)
  \return      a pointer to the allocated memory
*/
void *aimm_tcm_malloc(size_t size);
void *aimm_dram_malloc(size_t size);
void *aimm_tcm_malloc_sync(size_t size, int timeout);

/**
  \brief       Changes the size of the memory block pointed to by ptr to size bytes
  \param[in]   ptr  an old pointer to the allocated memory
  \param[in]   size allocated memory size(bytes)
  \return      a new pointer to the allocated memoryError code
*/
void *aimm_tcm_realloc(void *ptr, size_t size);
void *aimm_dram_realloc(void *ptr, size_t size);

/**
  \brief       Allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated memory
  \param[in]   nmenb  nmemb elements
  \param[in]   size    allocated memory size(bytes)
  \return      a pointer to the allocated memoryError code
*/
void *aimm_tcm_calloc(size_t nmemb, size_t size);
void *aimm_dram_calloc(size_t nmemb, size_t size);

/**
  \brief       Frees the memory space pointed to by ptr
  \param[in]   ptr    a pointer to the allocated memoryError code
  \return      None
*/
void aimm_tcm_free(void *ptr);
void aimm_dram_free(void *ptr);

/**
  \brief       Frees the memory space pointed to by ptr
  \param[in]   ptr    a pointer to the allocated memoryError code
  \return      None
*/
void *aimm_memcpy(void *dst, void *src, size_t size);

#ifdef __cplusplus
}
#endif
#endif
