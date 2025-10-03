#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "list.h"

#define IOC_MAGIC       'c'
#define UDMA_MEMCPY_CMD  _IOR(IOC_MAGIC, 0, int)

typedef struct {
	void 	*src;
	void 	*dst;
	size_t 	size;
} memcpy_msg_t;

typedef struct {
	struct list_head list;
	void 		*ptr;
	size_t 		size;
} mm_node_t;

typedef struct {
	int fd;
	struct list_head head;
} udma_t;

static udma_t udma;

static int add_node(mm_node_t *node)
{
	list_add(&node->list, &udma.head);
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
	list_for_each_entry(node, &udma.head, list) {
		if (node->ptr == ptr) {
			return node;
		}
	}

	return NULL;
}

static mm_node_t *match_node(void *ptr)
{
	mm_node_t *node;
	list_for_each_entry(node, &udma.head, list) {
		if ((size_t)ptr >= (size_t)node->ptr &&
			(size_t)ptr < ((size_t)node->ptr + node->size)) {
			return node;
		}
	}

	return NULL;
}

int udma_init(void)
{
	int fd = open("/dev/udma", (O_RDWR | O_SYNC));
	if (fd < 0) {
		return -1;
	}

	udma.fd = fd;
	INIT_LIST_HEAD(&udma.head);

	return 0;
}

int udma_deinit(void)
{
	close(udma.fd);
	return 0;
}

void *udma_malloc(size_t size)
{
	mm_node_t *node = malloc(sizeof(mm_node_t));
	if (!node)  return NULL;

	void *p = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, udma.fd, 0);
	if (p == MAP_FAILED) {
		perror("Failed to malloc the DMA memory mapped region");
		return NULL;
	}

	node->ptr = p;
	node->size = size;
	add_node(node);

    return p;
}

void udma_free(void *ptr)
{
	mm_node_t *node = get_node(ptr);
	if(!node) return;

	if (munmap(ptr, node->size) < 0) {
		perror("Failed to free the DMA memory mapped region");
	}
	del_node(node);
	free(node);
}

void *udma_memcpy(void *dest, void *src, size_t n)
{
	memcpy_msg_t msg;

	msg.size    = n;
	msg.dst     = dest;
	msg.src     = src;

	if (ioctl(udma.fd, UDMA_MEMCPY_CMD, &msg) < 0) {
		return NULL;
	}

	return dest;
}

int is_udma_mm(void *p)
{
	mm_node_t *node = match_node(p);
	return node ? 0 : -1;
}
