#ifndef __TCM_MM_H__
#define __TCM_MM_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
  \brief       Initialize Tcm Interface.
  \param[in]   None
  \return      Error code
*/
int tcm_init(void);

/**
  \brief       De-initialize Tcm Interface.
  \param[in]   None
  \return      Error code
*/
int tcm_deinit(void);

/**
  \brief       Allocates size bytes and returns a pointer to the allocated memory
  \param[in]   size    allocated memory size(bytes)
  \return      a pointer to the allocated memory
*/
void *tcm_malloc(size_t size);

/**
  \brief       Sync allocates size bytes and returns a pointer to the allocated memory
  \param[in]   size    allocated memory size(bytes)
  \param[in]   timeout    allocated wait time(ms)
  \return      a pointer to the allocated memory
*/
void *tcm_malloc_sync(size_t size, int timeout);

/**
  \brief       Changes the size of the memory block pointed to by ptr to size bytes
  \param[in]   ptr  an old pointer to the allocated memory
  \param[in]   size allocated memory size(bytes)
  \return      a new pointer to the allocated memoryError code
*/
void *tcm_realloc(void *ptr, size_t size);

/**
  \brief       Allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated memory
  \param[in]   nmenb  nmemb elements
  \param[in]   size    allocated memory size(bytes)
  \return      a pointer to the allocated memoryError code
*/
void *tcm_calloc(size_t nmemb, size_t size);

/**
  \brief       Frees the memory space pointed to by ptr
  \param[in]   ptr    a pointer to the allocated memoryError code
  \return      None
*/
void tcm_free(void *ptr);

/**
  \brief       va to pa
  \param[in]   va
  \return      pa
*/
void *tcm_va_to_pa(void *va);

/**
  \brief       tcm memory shwo
  \param[in]   None
  \return      None
*/
void tcm_mm_show(void);

int is_tcm_mm(void *p);

#ifdef __cplusplus
}
#endif
#endif
