/** Virtual File System **/
#include<vfs.h>
#include <output.h>
#include <lib/string.h>
#include <thread.h>
#include <mm/alloc.h>
static vfs_fs_t *fs_start;
static vfs_node_t *fs_root;
static vfs_node_t *vfs_find_impl(vfs_node_t *start,char *path);
static vfs_mount_t *vfsmntlst;
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
int vfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
    if (!node ||!node->fs ||  !node->fs->read) {
        if (how <= 0) return -1;
        return -1;
    } else {
        return node->fs->read(node,offset,how,buf);
    }
}
int vfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf) {
    if (!node || !node->fs || !node->fs->write) {
        if (how <= 0) return -1;
        return -1;
    } else {
	if ((node->mount_flags & VFS_MOUNT_RO) == VFS_MOUNT_RO) {
		return -2;
	}
        return node->fs->write(node,offset,how,buf);
    }
}
void vfs_open(vfs_node_t *node,bool r,bool w) {
    if (!node || !node->fs || !node->fs->open) {
        if (!w) {
            return;
        }
    } else {
        node->fs->open(node,w,r);
    }
}
void vfs_close(vfs_node_t *node) {
    if (!node || !node->fs || !node->fs->close) { 
        return;
    } else {
        node->fs->close(node);
    }
}
struct dirent *vfs_readdir(vfs_node_t *in,uint32_t index) {
    if (!in) return NULL;
    if (!in->fs) return NULL;
    if (!in->fs->readdir) return NULL;
    struct dirent *ret = in->fs->readdir(in,index);
    //kprintf("Return from FS: %s, 0x%x\n",in->fs->fs_name,ret);
    return ret;
}
vfs_node_t *vfs_finddir(vfs_node_t *in,char *name) {
    if (!in || !name || !in->fs || !in->fs->finddir) return NULL;
    return in->fs->finddir(in,name);
}
vfs_node_t *vfs_getRoot() {
    return fs_root;
}
void vfs_putIntoMnt(vfs_node_t *dev,vfs_node_t *target,vfs_fs_t *fs,char *target_path) {
	vfs_mount_t *el = kmalloc(sizeof(vfs_mount_t));
	memset(el,0,sizeof(vfs_mount_t));
	el->device = dev;
	el->target = target;
	el->target_path = strdup(target_path);
	el->fs = fs;
	if (vfsmntlst == NULL) {
		vfsmntlst = el;
	} else {
		vfs_mount_t *mnt = NULL;
		for (mnt = vfsmntlst; mnt->next != NULL; mnt = mnt->next) {}
		mnt->next = el;
	}
}
bool vfs_mount(vfs_fs_t *fs,vfs_node_t *dev,char *mountPoint) {
    if (!fs) {
        kprintf("%s: filesystem are null!",__func__);
        return false;
    }
    if (!mountPoint || mountPoint[0] != '/') {
        kprintf("%s: mount point are invalid: %s!\n",__func__,mountPoint);
        return false;
    }
    if (strcmp(mountPoint,"/")) {
	if (!fs->mount) {
                kprintf("%s: filesystem mount function are not defined\n",mountPoint);
                return false;
        }
        if (fs_root == NULL) {            
            fs_root = kmalloc(sizeof(vfs_node_t));
            memset(fs_root,0,sizeof(vfs_node_t));
	    fs_root->mask = (S_IRWXU | S_IRWXG | S_IRWXO);
        }
	bool root = fs->mount(dev,fs_root,NULL);
        if (!root) {
                kprintf("%s: filesystem mount failed\n",mountPoint);
                return false;
        } else {
		vfs_putIntoMnt(dev,fs_root,fs,"/");
	}
    } else {
        vfs_node_t *mount_point = vfs_find(mountPoint);
	if (!mount_point) {
		kprintf("%s: no such file or directory\n",mountPoint);
		return false;
	} else if ((mount_point->flags & 0x7) != VFS_DIRECTORY) {
		kprintf("%s: not directory\n",mountPoint);
		return false;
	}
	// call the actual mount function(if present)
	if (!fs->mount) {
		kprintf("%s: filesystem mount function are not defined\n",__func__);
		return false;
	}
	vfs_fs_t *origFS = mount_point->fs;
	bool root = fs->mount(dev,mount_point,NULL);
        if (!root) {
                kprintf("%s: filesystem mount failed\n",mountPoint);
                return false;
        } else {
		mount_point->orig_fs = origFS;
		// Put this FS into mounted list.
		vfs_putIntoMnt(dev,mount_point,fs,mountPoint);
	}
   }
    return true;
}
vfs_node_t *vfs_creat(vfs_node_t *in,char *name,int flags) {
    if (!in || !in->fs || !in->fs->creat) {
        kprintf("touch: broken target\n");
        return NULL;
    }
    return in->fs->creat(in,name,flags);
}
void vfs_truncate(vfs_node_t *node,int size) {
    if (!node || !node->fs ||  node->fs->truncate != 0) {
        node->fs->truncate(node,size);
    }
}
vfs_node_t *vfs_find(char *path) {
    if (path[0] == '/') {
	    vfs_node_t *rootDir = vfs_getRoot();
	    process_t *caller = thread_getThread(thread_getCurrent());
	    if (caller != NULL) {
		    rootDir = caller->root;
		}
        	return vfs_find_impl(vfs_getRoot(),path);
    } else {
        vfs_node_t *workDir = NULL;
	process_t *curPRC = thread_getThread(thread_getCurrent());
	if (curPRC == NULL) {
		workDir = fs_root;
		goto search;
	} else {
		workDir = curPRC->workDir;
	}
        if (!workDir) {
            kprintf("vfs_find: Seems your process are broken, cannot find work dir\n");
            return NULL; // return
        }
        if (strcmp(path,"..")) {
            return workDir->prev;
        }
search:
        return vfs_find_impl(workDir,path);
    }
    return NULL;
}
bool vfs_readBlock(vfs_node_t *node,int blockN,int how,void *buff) {
    if (!node || !node->fs || !node->fs->readBlock) return false;
    return node->fs->readBlock(node,blockN,how,buff);
}
bool vfs_writeBlock(vfs_node_t *node,int blockN,int how,void *buff) {
    if (!node || !node->fs ||  !node->fs->writeBlock) return false;
    return node->fs->writeBlock(node,blockN,how,buff);
}
int vfs_ioctl(vfs_node_t *node,int request,...) {
    if (!node || !node->fs ||  !node->fs->ioctl) return -1;
    va_list list;
    va_start(list,request);
    int ret = node->fs->ioctl(node,request,list);
    va_end(list);
    return ret;
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
		if (n->name == NULL) {
			// Maybe root?
			char_index-=1;
			target_path[char_index] = '/';
			n = n->prev;
			continue;
		}
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
	if (!node || !node->fs || !node->fs->mmap) return NULL;
	return node->fs->mmap(node,addr,size,offset,flags);
}
bool vfs_rm(vfs_node_t *node) {
	if (!node || !node->fs || !node->fs->rm) return false;
	return node->fs->rm(node);
}
bool vfs_isReady(struct vfs_node *node) {
	if (!node || !node->fs || !node->fs->isReady) return false;
	return node->fs->isReady(node);
}
bool vfs_umount(vfs_node_t *node) {
	if (node == NULL || !node->fs || !node->fs->umount) return false;
	bool res = node->fs->umount(node);
	// restore previous fs
	if (res) {
		node->fs = node->orig_fs;
	}
	return res;
}
vfs_mount_t *vfs_getMntList() {
	return vfsmntlst;
}
int vfs_writeMetadata(vfs_node_t *node) {
	// Check if node isn't corrupted and if the FS itself support chmod.
	if (!node) return -1;
	if (!node->fs->writeMetadata) {
		return 0;
	}
	return node->fs->writeMetadata(node);
}
bool vfs_hasPerm(vfs_node_t *node,int perm,int gr,int us) {
	if (!node) return false;
	if (perm == PERM_READ) {
		// Check if owner has permissions.
		if ((us == node->uid) && (node->mask & S_IRUSR)) {
			return true;
		}
		// Group of caller.
		if ((gr == node->gid) && (node->mask & S_IRGRP)) {
			return true;
		}
		// No? Then check if public user can do it.
		if (node->mask & S_IROTH) {
			return true;
		}
	} else if (perm == PERM_WRITE) {
		// The same.
		if ((us == node->uid) && (node->mask & S_IWUSR)) {
			return true;
		}
		if ((gr == node->gid) && (node->mask & S_IWGRP)) {
			return true;
		}
		if (node->mask & S_IWOTH) {
			return true;
		}
	} else if (perm == PERM_EXEC) {
		if ((us == node->uid) && (node->mask & S_IXUSR)) {
			return true;
		}
		if ((gr == node->gid) && (node->mask & S_IXGRP)) {
			return true;
		}
		// Public user then.
		if (node->mask & S_IXOTH) {
			return true;
		}
	}
	return false;
}
