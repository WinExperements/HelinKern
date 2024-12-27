#ifndef DEV_H
#define DEV_H
#include<typedefs.h>
#include<vfs.h>

#define DEVFS_TYPE_BLOCK	0x2
#define DEVFS_TYPE_POWER	0x3	// This device type means that driver has functions poweroff and poweron.

typedef struct dev {
    char *name;
    int buffer_sizeMax;
    uint64_t (*write)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    uint64_t (*read)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    void *(*mmap)(struct vfs_node *node,int addr,int size,int offset,int flags);
    bool (*readBlock)(struct vfs_node *node,int blockNo,int how,void *buf);
    bool (*writeBlock)(struct vfs_node *node,int blockNo,int how,void *buf);
    uint64_t (*ioctl)(struct vfs_node *node,int request,va_list args);
    bool (*isReady)(struct vfs_node *node); // select syscall
    void (*close)(struct vfs_node *node); // some devices need this.
    // Power device type functions. Can be ignored.
    bool (*poweroff)(struct vfs_node *node);
    bool (*poweron)(struct vfs_node *node);

    void *device;
    int type;
    int mode;
    vfs_node_t *devNode;
    struct dev *next;
    struct dev *prev;
} dev_t;
void dev_init();
void dev_add(dev_t *dev);
dev_t *dev_find(char *name);
void dev_remove(int id);
dev_t *dev_getRoot();
#endif
