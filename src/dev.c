/* Character device communication */
#include<dev.h>
#include<typedefs.h>
#include <lib/string.h>
#include <output.h>
#include<vfs.h>
#include <mm/alloc.h>
static dev_t *device;
static vfs_node_t *dev_dir;
static vfs_fs_t *fs;
static int devfs_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static int devfs_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static void *devfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags);
static void devfs_readBlock(struct vfs_node *node,int blockN,int how,void *buff);
static void devfs_writeBlock(struct vfs_node *node,int blockN,int how,void *buff);
static int devfs_ioctl(struct vfs_node *node,int request,va_list args);
static bool devfs_isReady(struct vfs_node *node);
void dev_init() {
    device = NULL;
    dev_dir = vfs_finddir(vfs_getRoot(),"dev");
    if (!dev_dir) {
        kprintf("no /dev");
        return;
    }
    // register devfs as FS
    fs = kmalloc(sizeof(vfs_fs_t));
    memset(fs,0,sizeof(vfs_fs_t));
    fs->fs_name = "devfs";
    fs->write = devfs_write;
    fs->read = devfs_read;
    fs->mmap = devfs_mmap;
    fs->readBlock = devfs_readBlock;
    fs->writeBlock = devfs_writeBlock;
    fs->ioctl = devfs_ioctl;
    fs->isReady = devfs_isReady;
    vfs_addFS(fs);

}
void dev_add(dev_t *dev) {
    vfs_node_t *fil = vfs_creat(dev_dir,dev->name,0);
    vfs_truncate(fil,dev->buffer_sizeMax);
    fil->fs = fs;
    fil->device = dev->device;
    fil->priv_data = dev;
    if (!device) {
        device = dev;
    } else {
        dev_t *de = device;
        while(de->next != NULL) {
            de = de->next;
        }
        de->next = dev;
    }
}
dev_t *dev_find(char *name) {
    dev_t *d = device;
    while(d != NULL) {
        if (strcmp(d->name,name)) {
            return d;
        }
        d = d->next;
    }
    return NULL;
}
void dev_remove(int id) {}
static int devfs_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        return d->read(node,offset,how,buf);
    }
    return 0;
}
static int devfs_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        return d->write(node,offset,how,buf);
    }
    return 0;
}
static void *devfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        return d->mmap(node,addr,size,offset,flags);
    }
    return NULL;
}
static void devfs_readBlock(struct vfs_node *node,int blockN,int how,void *buff) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        d->readBlock(node,blockN,how,buff);
    }
}
static void devfs_writeBlock(struct vfs_node *node,int blockN,int how,void *buff) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        d->writeBlock(node,blockN,how,buff);
    }
}
static int devfs_ioctl(struct vfs_node *node,int request,va_list args) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (!d) return -1;
        return d->ioctl(node,request,args);
    }
    return -1;
}
static bool devfs_isReady(struct vfs_node *node) {
	if (node != NULL) {
		dev_t *d = (dev_t *)node->priv_data;
		if (!d->isReady) return false;
		return d->isReady(node);
	}
	return false;
}
