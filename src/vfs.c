/** Virtual File System **/
#include<vfs.h>
#include <output.h>
#include <lib/string.h>
#include <thread.h>
#include <mm/alloc.h>
static vfs_fs_t *fs_start;
static vfs_node_t *fs_root;
static vfs_node_t *vfs_find_impl(vfs_node_t *start,char *path);
void vfs_init() {
}
void vfs_addFS(vfs_fs_t *fs) {
    if (!fs_start) {
        fs_start = fs;
        return;
    } else {
        vfs_fs_t *ffs = fs_start;
        while(ffs->next) {
            ffs = ffs->next;
        }
        ffs->next = fs; 
    }
}
void vfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
    if (!node) {
        if (how <= 0) return;
        kprintf("vfs: file seems broken, or you trying to read don't exists file\n");
        return;
    } else {
        node->fs->read(node,offset,how,buf);
    }
}
void vfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
    if (!node) {
        if (how <= 0) return;
        kprintf("vfs: file seems broken, or you trying to write to don't exists file\n");
        return;
    } else {
        node->fs->write(node,offset,how,buf);
    }
}
void vfs_open(vfs_node_t *node,bool r,bool w) {
    if (!node) {
        if (!w) {
            kprintf("vfs: file seems broken, or you trying to open don't exists file\n");
            return;
        }
    } else {
        node->fs->open(node,w,r);
    }
}
void vfs_close(vfs_node_t *node) {
    if (!node || !node->fs->close) {
        kprintf("vfs: file seems broken, or you trying to close, didn't oppened file\n");
        return;
    } else {
        node->fs->close(node);
    }
}
struct dirent *vfs_readdir(vfs_node_t *in,uint32_t index) {
    if (!in) return NULL;
    if (!in->fs->readdir) return NULL;
    return in->fs->readdir(in,index);
}
vfs_node_t *vfs_finddir(vfs_node_t *in,char *name) {
    if (!in || !name) return NULL;
    return in->fs->finddir(in,name);
}
vfs_node_t *vfs_getRoot() {
    return fs_root;
}
void vfs_mount(vfs_fs_t *fs,vfs_node_t *dev,char *mountPoint) {
    if (!fs) {
        kprintf("%s: filesystem are null!",__func__);
        return;
    }
    if (!mountPoint || mountPoint[0] != '/') {
        kprintf("%s: mount point are invalid",__func__);
    }
    if (strcmp(mountPoint,"/")) {
	if (!fs->mount) {
                kprintf("%s: filesystem mount function are not defined\n",mountPoint);
                return;
        }
        fs_root = kmalloc(sizeof(vfs_node_t));
        memset(fs_root,0,sizeof(vfs_node_t));
	    bool root = fs->mount(dev,fs_root,NULL);
        if (!root) {
                kprintf("%s: filesystem mount failed\n",mountPoint);
                return;
        }
    } else {
        vfs_node_t *mount_point = vfs_find(mountPoint);
	if (!mount_point) {
		kprintf("%s: no such file or directory\n",mountPoint);
		return;
	} else if ((mount_point->flags & 0x7) != VFS_DIRECTORY) {
		kprintf("%s: not directory\n",mountPoint);
		return;
	}
	// call the actual mount function(if present)
	if (!fs->mount) {
		kprintf("%s: filesystem mount function are not defined\n");
		return;
	}
	bool root = fs->mount(dev,mount_point,NULL);
        if (!root) {
                kprintf("%s: filesystem mount failed\n",mountPoint);
                return;
        }
   }
}
vfs_node_t *vfs_creat(vfs_node_t *in,char *name,int flags) {
    if (!in || !in->fs->creat) {
        kprintf("touch: broken target\n");
        return NULL;
    }
    return in->fs->creat(in,name,flags);
}
void vfs_truncate(vfs_node_t *node,int size) {
    if (!node || node->fs->truncate != 0) {
        node->fs->truncate(node,size);
    }
}
vfs_node_t *vfs_find(char *path) {
    if (path[0] == '/') {
        return vfs_find_impl(vfs_getRoot(),path);
    } else {
        vfs_node_t *workDir = thread_getThread(thread_getCurrent())->workDir;
        if (!workDir) {
            kprintf("vfs_find: Seems your process are broken, cannot find work dir\n");
            return NULL; // return
        }
        if (strcmp(path,"..")) {
            return workDir->prev;
        }
        return vfs_find_impl(workDir,path);
    }
    return NULL;
}
void vfs_readBlock(vfs_node_t *node,int blockN,int how,void *buff) {
    if (!node || !node->fs->readBlock) return;
    node->fs->readBlock(node,blockN,how,buff);
}
void vfs_writeBlock(vfs_node_t *node,int blockN,int how,void *buff) {
    if (!node || !node->fs->writeBlock) return;
    node->fs->writeBlock(node,blockN,how,buff);
}
int vfs_ioctl(vfs_node_t *node,int request,void *argp) {
    if (!node || !node->fs->ioctl) return -1;
    return node->fs->ioctl(node,request,argp);
}
static vfs_node_t *vfs_find_impl(vfs_node_t *start,char *path) {
    char *t = strtok(path,"/");
    vfs_node_t *node = start;
    while(t) {
        node = vfs_finddir(node,t);
        if (!node) return NULL;
        t = strtok(NULL,"/");
    }
    return node;
}
void vfs_node_path(vfs_node_t *node,char *path,int size) {
    if (node == fs_root) {
        if (size > 1) {
            path[0] = '/';
            path[1] = '\0';
            return;
        } else {
            return;
        }
    } else {
        char target_path[128];
        vfs_node_t *n = node;
        int char_index = 127;
        while (n != NULL) {
            int len = strlen(n->name);
            char_index-=len;
            if (char_index < 2) {
                return;
            }
            if (n->prev != NULL) {
                strcpy(target_path+char_index,n->name);
                char_index-=1;
                target_path[char_index] = '/';
            }
            n = n->prev;
        }
        int len = 127-char_index;
        if (size < len) {
            return;
        }
        strcpy(path,target_path+char_index+1);
    }
}
vfs_fs_t *vfs_findFS(char *name) {
    vfs_fs_t *start = fs_start;
    while(start != NULL) {
        if (strcmp(start->fs_name,name)) {
            return start;
        }
        start = start->next;
    }
    return NULL;
}
void *vfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags) {
	if (!node || !node->fs->mmap) return NULL;
	return node->fs->mmap(node,addr,size,offset,flags);
}
