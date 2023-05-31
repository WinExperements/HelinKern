#ifndef CPIO_H
#define CPIO_H

#include <typedefs.h>
#include <vfs.h>

#define CPIO_MAGIC 0070707

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
    vfs_node_t *super;
    vfs_node_t *parent;
    vfs_node_t *dir;
    size_t count;
    size_t data;
    char *name;
    vfs_node_t *next;
};

void cpio_init(); // register CPIO FS

void cpio_load(void *initrd,int dev_size);

#endif
