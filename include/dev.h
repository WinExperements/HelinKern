#ifndef DEV_H
#define DEV_H
#include<typedefs.h>
#include<vfs.h>

#define DEVFS_TYPE_BLOCK	0x2

typedef struct dev {
    char *name;
    int buffer_sizeMax;
    int (*write)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    int (*read)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    void *(*mmap)(struct vfs_node *node,int addr,int size,int offset,int flags);
    void (*readBlock)(struct vfs_node *node,int blockNo,int how,void *buf);
    void (*writeBlock)(struct vfs_node *node,int blockNo,int how,void *buf);
    int (*ioctl)(struct vfs_node *node,int request,va_list args);
    bool (*isReady)(struct vfs_node *node); // select syscall
    void *device;
    int type;
    vfs_node_t *devNode;
    struct dev *next;
    struct dev *prev;
} dev_t;
void dev_init();
void dev_add(dev_t *dev);
dev_t *dev_find(char *name);
void dev_remove(int id);
#endif
