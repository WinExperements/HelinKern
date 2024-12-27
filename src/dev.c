/* Character device communication */
#include<dev.h>
#include<typedefs.h>
#include <lib/string.h>
#include <output.h>
#include<vfs.h>
#include <mm/alloc.h>
#include <fs/parttab.h>
/* Since 30.01.2024 this is now a completlly independ DEVFS(must be mounted by init) */
static dev_t *device;
static vfs_fs_t *fs;
static uint64_t devfs_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static uint64_t devfs_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static void *devfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags);
static bool devfs_readBlock(struct vfs_node *node,int blockN,int how,void *buff);
static bool devfs_writeBlock(struct vfs_node *node,int blockN,int how,void *buff);
static uint64_t devfs_ioctl(struct vfs_node *node,int request,va_list args);
static bool devfs_isReady(struct vfs_node *node);
static void devfs_close(vfs_node_t *node);
static vfs_node_t *devfs_finddir(struct vfs_node *in,char *name);
static int devfs_readdir(struct vfs_node *dir,uint32_t index,struct dirent *to);
static struct dirent devDirent; // global dirent for all code :)
static bool devfs_mount(vfs_node_t *dev,vfs_node_t *mptr,void *unused);
void dev_init() {
    device = NULL;
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
    fs->readdir = devfs_readdir; // ls or something
    fs->finddir = devfs_finddir;
    fs->mount = devfs_mount;
    fs->close = devfs_close;
    vfs_addFS(fs);
    // Register some base devices(null,zero)
    dev_t *nulldev = (dev_t *)kmalloc(sizeof(dev_t));
    memset(nulldev,0,sizeof(dev_t));
    nulldev->name = "null";
    dev_add(nulldev);

}
void dev_add(dev_t *dev) {
    //create the file inode
    vfs_node_t *fil = kmalloc(sizeof(vfs_node_t));
    memset(fil,0,sizeof(vfs_node_t));
    fil->name = strdup(dev->name);
    fil->size = dev->buffer_sizeMax;
    fil->fs = fs;
    fil->device = dev->device;
    fil->priv_data = dev;
    fil->mask = dev->mode;
    fil->mask |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    if ((dev->type & DEVFS_TYPE_BLOCK) == DEVFS_TYPE_BLOCK) {
            fil->flags |= VFS_BLOCK;
        }
    dev->devNode = fil;
    if (!device) {
        device = dev;
    } else {
        dev_t *de = device;
        while(de && de->next) {
            de = de->next;
        }
        de->next = dev;
    }
    if (dev->type & DEVFS_TYPE_BLOCK) {
	    // Trigger parttab
	    partTab_trigger(fil);
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
static uint64_t devfs_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (d->read != NULL) {
        	return d->read(node,offset,how,buf);
        }
    }
    return 0;
}
static uint64_t devfs_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (d->write != NULL) {
        	return d->write(node,offset,how,buf);
        }
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
static bool devfs_readBlock(struct vfs_node *node,int blockN,int how,void *buff) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (d->readBlock != NULL) {
        	return d->readBlock(node,blockN,how,buff);
        }
    }
    return false;
}
static bool devfs_writeBlock(struct vfs_node *node,int blockN,int how,void *buff) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (d->writeBlock != NULL) {
        	return d->writeBlock(node,blockN,how,buff);
        }
    }
    return false;
}
static uint64_t devfs_ioctl(struct vfs_node *node,int request,va_list args) {
    if (node != NULL) {
        dev_t *d = (dev_t *)node->priv_data;
        if (!d) return -1;
        if (d->ioctl == NULL) return -1;
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
static vfs_node_t *devfs_finddir(struct vfs_node *in,char *name) {
    dev_t *start = device;
    while(start) {
        if (start->devNode != NULL) {
            if (strcmp(start->name,name)) {
                return start->devNode;
            }
        }
        start = start->next;
    }
    return NULL;
}
static int devfs_readdir(struct vfs_node *dir,uint32_t index,struct dirent *to) {
    uint32_t i = 0;
    dev_t *start = device;
    while(start) {
        if (start->devNode != NULL) {
            if (i == index) {
                strcpy(to->name,start->name);
                to->type = start->devNode->flags;
                return 1;
            }
        }
        i++;
        start = start->next;
    }
    return 0;
}
static bool devfs_mount(vfs_node_t *dev,vfs_node_t *mptr,void *unused) {
    if (dev == NULL || mptr == NULL) {
        kprintf("devfs: dev or mptr is NULL\n");
        return false;
    }
    mptr->fs = fs;
    mptr->priv_data = NULL; // clean this shit.
    return true;
}
dev_t *dev_getRoot() {
	return device;
}
static void devfs_close(vfs_node_t *dev) {
	if (dev != NULL) {
		dev_t *d = (dev_t *)dev->priv_data;
		if (d->close != NULL) {
			// Invoke this callback handler.
			d->close(dev);
		}
	}
}
