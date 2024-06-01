#ifndef CPIO_H
#define CPIO_H

#include <typedefs.h>
#include <vfs.h>

#define CPIO_MAGIC 0070707
#define C_ISDIR		0040000
struct cpio_hdr {
    uint16_t magic;
    uint16_t dev;
    uint16_t ino;
    uint16_t mode;
    uint16_t uid;
    uint16_t gid;
    uint16_t nlink;
    uint16_t rdev;
    uint16_t mtime[2];
    uint16_t namesize;
    uint16_t filesize[2];
} __attribute__((packed));

struct cpio {
	vfs_node_t *root;
	vfs_node_t *node;	// node attached to this structure.
	int data;
	int dirSize;
	struct cpio *next; 	// finddir.
	struct cpio *child;
};

void cpio_init(); // register CPIO FS

void cpio_load(void *initrd,int dev_size);

#endif
