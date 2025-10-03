#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

#include "tcm.h"
#include "list.h"

#define TCM_NAME		"tcm"
#define IOC_MAGIC		'c'
#define TCM_MEM_SHOW		_IOR(IOC_MAGIC, 2, int)
#define TCM_VA_TO_PA		_IOR(IOC_MAGIC, 4, int)
#define TCM_REQUEST_MEM		_IOR(IOC_MAGIC, 5, int)
#define TCM_RELEASE_MEM		_IOR(IOC_MAGIC, 6, int)


#define tcm_mutex_init()	pthread_mutex_init(&tcm.mutex, NULL)
#define tcm_mutex_lock()	pthread_mutex_lock(&tcm.mutex)
#define tcm_mutex_try_lock()	pthread_mutex_trylock(&tcm.mutex)
#define tcm_mutex_unlock()	pthread_mutex_unlock(&tcm.mutex)
#define tcm_mutex_deinit()	pthread_mutex_destroy(&tcm.mutex)

#define tcm_check_return_val(X, ret)								\
	do {											\
		if (!(X)) {									\
			printf("tcm check param err--->fun:%s + line:%d", __func__, __LINE__);	\
			return ret;								\
		}										\
	} while (0)

#define tcm_check_return(X)									\
	do {											\
		if (!(X)) {									\
			printf("tcm check param err--->fun:%s + line:%d", __func__, __LINE__);	\
			return;									\
		}										\
	} while (0)

#define timeval_sub(a, b, res)					\
	do {							\
		(res)->tv_sec = (a)->tv_sec - (b)->tv_sec;	\
		(res)->tv_usec = (a)->tv_usec - (b)->tv_usec;	\
		if ((res)->tv_usec < 0) {			\
			(res)->tv_sec--;			\
			(res)->tv_usec += 1000000;		\
		}						\
	} while (0)

typedef struct {
	struct list_head list;
	void 		*ptr;
	size_t 		size;
} mm_node_t;

typedef struct {
	int		inited;
	int		debug;
	int		fd;
	int		block_num;
	pthread_mutex_t mutex;
	struct list_head head;
} tcm_t;

typedef struct {
	void		*vaddr;
	void		*paddr;
} va_to_pa_msg_t;

static tcm_t    tcm;

static void mem_show(void)
{
	ioctl(tcm.fd, TCM_MEM_SHOW, NULL);
}

static int mem_open(void)
{
	int fd = open("/dev/"TCM_NAME, (O_RDWR | O_SYNC));
	if (fd < 0) {
		printf("open failed(%d)\n", fd);
		return -1;
	}

	tcm.fd = fd;

	return 0;
}

static int mem_close(void)
{
	close(tcm.fd);
	return 0;
}

static int add_node(mm_node_t *node)
{
	list_add(&node->list, &tcm.head);
	return 0;
}

static int del_node(mm_node_t *node)
{
	list_del(&node->list);
	return 0;
}

static mm_node_t *get_node(void *ptr)
{
	mm_node_t *node;
	list_for_each_entry(node, &tcm.head, list) {
		if (node->ptr == ptr) {
			return node;
		}
	}

	return NULL;
}

static mm_node_t *match_node(void *ptr)
{
	mm_node_t *node;
	list_for_each_entry(node, &tcm.head, list) {
		if ((size_t)ptr >= (size_t)node->ptr && 
			(size_t)ptr < ((size_t)node->ptr + node->size)) {
			return node;
		}
	}

	return NULL;
}

static void *alloc(size_t size)
{
	mm_node_t *node = malloc(sizeof(mm_node_t));
	if (!node)  return NULL;

	void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, tcm.fd, 0);
	if (p == MAP_FAILED) {
		free(node);
		printf("%s failed(%ld)\n", __func__, size);
		return NULL;
	}

	node->ptr = p;
	node->size = size;
	tcm_mutex_lock();
	add_node(node);
	tcm_mutex_unlock();

	return p;
}

void *tcm_malloc_sync(size_t size, int timeout)
{
	tcm_check_return_val(tcm.inited, NULL);
	struct timeval now;
	struct timeval end;
	struct timeval sub;
	int remain_wait = timeout;

	gettimeofday(&now, NULL);
	void *p = alloc(size);	
	while ((p == NULL) && (timeout != 0)) {
		int ret;
		struct pollfd events;
		events.fd = tcm.fd;
		events.events = POLLIN | POLLERR;

		if (tcm.debug) printf("thread(%d) %s timeout:%d(ms)\n", getpid(), __func__, remain_wait);

		tcm_mutex_lock();
		if (ioctl(tcm.fd, TCM_REQUEST_MEM, &size) < 0) {
			tcm_mutex_unlock();
			return NULL;
		}

		ret = poll((struct pollfd *)&events, 1, remain_wait);
		if(ret <= 0 && events.revents == POLLERR) {
			tcm_mutex_unlock();
			break;
		}

		if (ioctl(tcm.fd, TCM_RELEASE_MEM, &size) < 0) {
			tcm_mutex_unlock();
			return NULL;
		}
		tcm_mutex_unlock();
		
		if (tcm.debug) printf("thread(%d) %s wait\n", getpid(), __func__);
		p = alloc(size);
		if (p) {
			break;
		}

		if (tcm.debug) printf("thread(%d) %s failed\n", getpid(), __func__);
		if (timeout != -1) {
			gettimeofday(&end, NULL);
			timeval_sub(&end, &now, &sub);
			long elapsed = end.tv_sec * 1000 + end.tv_usec / 1000;
			if (elapsed > timeout) {
				if (tcm.debug) printf("thread(%d) %s timeout\n", getpid(), __func__);
				break;
			} else {
				remain_wait = (timeout - elapsed);
			}
		}
	}

	return p;
}

void *tcm_malloc(size_t size)
{
	tcm_check_return_val(tcm.inited, NULL);

	return alloc(size);
}

void *tcm_calloc(size_t nmemb, size_t size)
{
	tcm_check_return_val(tcm.inited, NULL);

	return alloc(size * nmemb);
}

void tcm_free(void *ptr)
{
	tcm_check_return(tcm.inited);
	
	mm_node_t *node = get_node(ptr);
	if(!node) return;
	munmap(ptr, node->size);
	tcm_mutex_lock();
	del_node(node);
	tcm_mutex_unlock();
	free(node);
}

int is_tcm_mm(void *p)
{
	mm_node_t *node = match_node(p);
	return node ? 0 : -1;
}

void *tcm_va_to_pa(void *va)
{
	va_to_pa_msg_t msg;

	msg.vaddr = va;
	if (ioctl(tcm.fd, TCM_VA_TO_PA, &msg) < 0) {
		return NULL;
	}

	return msg.paddr;
}

void tcm_mm_show(void)
{
	mem_show();
}

int tcm_init(void)
{
	int ret = 0;

	if (!tcm.inited) {
		ret = mem_open();
		if ( ret == 0) {
			INIT_LIST_HEAD(&tcm.head);
			tcm_mutex_init();
			tcm.inited  = 1;
			tcm.debug = 1;
		}
	}

	return ret;
}

int tcm_deinit(void)
{
	if (tcm.inited) {
		mem_close();
		tcm_mutex_deinit();
		tcm.inited = 0;
		return 0;
	}

	return -1;
}
