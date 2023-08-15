#ifndef VFS_H
#define VFS_H
#include<typedefs.h>
#include <stdarg.h>
#define VFS_DIRECTORY 0x02
typedef struct vfs_node {
    char *name;
    uint32_t mask;
    uint32_t guid;
    uint32_t uid;
    uint32_t flags;
    uint32_t inode;
    uint32_t size;
    struct vfs_node *prev;
    struct vfs_node *first_child;
    struct vfs_node *next_child;
    struct vfs_fs *fs;
    void *device;
    void *priv_data;
} vfs_node_t;
struct dirent {
    char name[128];
    uint32_t node;
};
typedef struct vfs_fs {
    char *fs_name;
    bool (*mount)(struct vfs_node *dev,struct vfs_node *mountpoint,void *);
    int (*read)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    int (*write)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    void (*open)(struct vfs_node *node,bool w,bool r);
    void (*close)(struct vfs_node *node);
    struct vfs_node *(*finddir)(struct vfs_node *in,char *name);
    struct dirent *(*readdir)(struct vfs_node *dir,uint32_t index);
    struct vfs_node *(*creat)(struct vfs_node *in,char *name,int flags);
    void (*truncate)(struct vfs_node *in,int size);
    void (*readBlock)(struct vfs_node *node,int blockN,int how,void *buff);
    void (*writeBlock)(struct vfs_node *node,int blockN,int how,void *buff);
    int (*ioctl)(struct vfs_node *node,int request,va_list args);
    void *(*mmap)(struct vfs_node *node,int addr,int size,int offset,int flags);
    void (*rm)(struct vfs_node *node);
    struct vfs_fs *next;
} vfs_fs_t;
typedef struct file_descriptor {
	int pid; // owner of the FD
	vfs_node_t *node; // address of node
	int offset;
} file_descriptor_t;
void vfs_init();
void vfs_addFS(vfs_fs_t *fs);
vfs_fs_t *vfs_findFS(char *name);
int vfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
int vfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
void vfs_open(vfs_node_t *node,bool w,bool r);
void vfs_close(vfs_node_t *node);
struct dirent *vfs_readdir(vfs_node_t *in,uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *in,char *name);
vfs_node_t *vfs_getRoot();
bool vfs_mount(vfs_fs_t *fs,vfs_node_t *dev,char *mountPoint);
vfs_node_t *vfs_creat(vfs_node_t *in,char *name,int flags);
void rootfs_init();
void rootfs_mount(char *to);
vfs_node_t *vfs_find(char *path);
void vfs_truncate(vfs_node_t *node,int size);
void vfs_readBlock(vfs_node_t *node,int blockN,int how,void *buff);
void vfs_writeBlock(vfs_node_t *node,int blockN,int how,void *buff);
int vfs_ioctl(vfs_node_t *node,int request,...);
void vfs_node_path(vfs_node_t *node,char *path,int size);
void rootfs_insertModuleData(vfs_node_t *node,int size,char *addr);
void *vfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags);
void vfs_rm(struct vfs_node *node);
#endif
