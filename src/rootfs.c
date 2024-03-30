/** Root file system **/
#include<vfs.h>
#include<typedefs.h>
#include <output.h>
#include <lib/string.h>
#include <mm/alloc.h>
static vfs_node_t *root;
static vfs_fs_t *rootfs_fs;
static struct dirent rootfs_dirent;
static int files = 0;
static int rootfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
static struct dirent *rootfs_readdir(vfs_node_t *in,uint32_t index);
static vfs_node_t *rootfs_finddir(vfs_node_t *root,char *name);
static vfs_node_t *rootfs_creat(vfs_node_t *node,char *name,int flags);
static int rootfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
static void rootfs_truncate(vfs_node_t *in,int size);
static void rootfs_close(vfs_node_t *in);
static bool __rootfs_mount(vfs_node_t *,vfs_node_t *mptr,void *);
static bool rootfs_rm(vfs_node_t *node);
static char **data;
void rootfs_init() {
    // we need to register new FS
    rootfs_fs = kmalloc(sizeof(vfs_fs_t));
    memset(rootfs_fs,0,sizeof(vfs_fs_t));
    rootfs_fs->fs_name = "rootfs";
    rootfs_fs->mount = __rootfs_mount;
    root = kmalloc(sizeof(vfs_node_t));
    memset(root,0,sizeof(vfs_node_t));
    root->name = "/";
    root->flags = VFS_DIRECTORY;
    rootfs_fs->read = rootfs_read;
    rootfs_fs->readdir = rootfs_readdir;
    rootfs_fs->finddir = rootfs_finddir;
    rootfs_fs->creat = rootfs_creat;
    rootfs_fs->write = rootfs_write;
    rootfs_fs->truncate = rootfs_truncate;
    rootfs_fs->close = rootfs_close;
    rootfs_fs->rm = rootfs_rm;
    root->fs = rootfs_fs;
    vfs_addFS(rootfs_fs);
    data = kmalloc(100);
    memset(data,0,100);
}
static int rootfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
    if ((node->flags & 0x7) != VFS_DIRECTORY) {
        memcpy(buf,data[node->inode]+offset,how);
    }
    return how;
}
static struct dirent *rootfs_readdir(vfs_node_t *in,uint32_t index) {
    // find node
    uint32_t i = 0;
    vfs_node_t *n = in->first_child;
    while(n != NULL) {
        if (i == index) {
            memset(&rootfs_dirent,0,sizeof(rootfs_dirent));
            rootfs_dirent.node = n->inode;
            strcpy(rootfs_dirent.name,n->name);
            return &rootfs_dirent;
        }
        n = n->next_child;
        ++i;
    }
    return NULL;
}
static vfs_node_t *rootfs_finddir(vfs_node_t *root,char *name) {
    vfs_node_t *n = root->first_child;
    while(n != NULL) {
        if (strcmp(n->name,name)) {
            return n;
        }
        n = n->next_child;
    }
    return NULL;
}
static vfs_node_t *rootfs_creat(vfs_node_t *node,char *name,int flags) {
    int nameLen = strlen(name);
    vfs_node_t *newDir =  kmalloc(sizeof(vfs_node_t));
    memset(newDir,0,sizeof(vfs_node_t));
    newDir->name = kmalloc(nameLen);
    strcpy(newDir->name,name);
    newDir->name[nameLen] = 0;
    newDir->flags = flags;
    newDir->fs = rootfs_fs;
    newDir->next_child = NULL;
    newDir->prev = node;
    int i = 1;
    if (node->first_child == NULL) {
        node->first_child = newDir;
        return newDir;
    }
    vfs_node_t *n = node->first_child;
    while(n->next_child != NULL) {
        n = n->next_child;
        i++;
    }
    newDir->inode = i;
    n->next_child = newDir;
    files++;
    return newDir;
}
void rootfs_mount(char *to) {
    vfs_mount(rootfs_fs,NULL,to);
}
static int rootfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
   return 0;
}
static void rootfs_truncate(vfs_node_t *node,int size) {
    node->size = size;
}
static void rootfs_close(vfs_node_t *node) {}
void rootfs_insertModuleData(vfs_node_t *node,int size,char *addr) {
	data[node->inode] = addr;
	node->size = size;
}
static bool __rootfs_mount(vfs_node_t *to,vfs_node_t *mp,void *params) {
	mp->name = "/";
    mp->flags = VFS_DIRECTORY;
    mp->fs = rootfs_fs;
    return true;
}

static bool rootfs_rm(vfs_node_t *node) {
	return true;
}
