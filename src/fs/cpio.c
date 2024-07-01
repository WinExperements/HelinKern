#include <fs/cpio.h>
#include <output.h>
#include <debug.h>
#include <lib/string.h>
#include <mm/alloc.h>
#include <arch/mmu.h>
static vfs_fs_t *cpio_fs;
static vfs_node_t *root;
static vfs_node_t *d;

static vfs_node_t *new_node(const char *name,struct cpio_hdr *hdr,size_t sz,size_t data,int inode_index) {
    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    memset(node,0,sizeof(vfs_node_t));
    struct cpio *child_cpio = kmalloc(sizeof(struct cpio));
    memset(child_cpio,0,sizeof(struct cpio));
    node->size = sz;
    node->uid = node->gid = 0;
    node->fs = cpio_fs;
    node->name = strdup(name);
    node->mount_flags = VFS_MOUNT_RO;
    node->inode = inode_index;
    child_cpio->node = node;
    child_cpio->data = data;
    node->priv_data = child_cpio;
    if ((hdr->mode & C_ISDIR) == C_ISDIR) {
      node->flags |= VFS_DIRECTORY;
    }
    return node;
}

static void new_child_node(vfs_node_t *parent,vfs_node_t *child) {
    if (!parent || !child) return;
    if (parent->flags != VFS_DIRECTORY) return;
    struct cpio *inform = (struct cpio *)parent->priv_data;
    struct cpio *child_cpio = (struct cpio *)child->priv_data;
    if (inform->child == NULL) {
	    inform->child = child_cpio;
	    child_cpio->root = parent;
    } else {
	    for (; inform->child != NULL; inform = inform->child);
	    inform->child = child_cpio;
	    child_cpio->root = parent;
   }
    ((struct cpio*)parent->priv_data)->dirSize++;
}

static bool cpio_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *params) {
    DEBUG("Loading CPIO initrd from %s\r\n",dev->name);
    // HACK!
    process_t *me = thread_getThread(thread_getCurrent());
    vfs_node_t *home = me->workDir;
    struct cpio_hdr hdr;
    int offset = 0;
    int size = 0;
    root = mountpoint;
    d = dev;
    root->fs = cpio_fs;
    struct cpio *p = kmalloc(sizeof(struct cpio));
    memset(p,0,sizeof(struct cpio));
    root->priv_data = p;
    int dev_size = dev->size;
    me->workDir = mountpoint;
    if (dev->size == 0) {
	    kprintf("cpio: device size is zero! Device: %s\r\n",dev->name);
	    return false;
    }
    int ino_index = 0;
    for (; offset < dev_size; offset +=sizeof(struct cpio_hdr)+(hdr.namesize+1)/2*2 + (size+1)/2*2) {
        int data_offset = offset;
        vfs_read(dev,data_offset,sizeof(struct cpio_hdr),&hdr);
        if (hdr.magic != CPIO_MAGIC) {
            kprintf("cpiofs: Invalid magic(0x%x)\r\n",hdr.magic);
            me->workDir = home;
            return false;
        }
        size = hdr.filesize[0] * 0x10000 + hdr.filesize[1];
        data_offset += sizeof(struct cpio_hdr);
        char path[1024];
        vfs_read(dev,data_offset,hdr.namesize,path);
        if (strcmp(path,".")) continue;
        if (strcmp(path,"TRAILER!!!")) break;
        char *name = NULL;
        char *dir = NULL;
        for (int i = hdr.namesize-1; i >= 0; i--) {
            if (path[i] == '/') {
                path[i] = '\0';
                name = &path[i+1];
                dir = path;
                break;
            }
        }
        if (!name) {
            name = path;
        }
        data_offset += hdr.namesize + (hdr.namesize % 2);
	//kprintf("%s: data offset: %d, size: %d\r\n",__func__,data_offset,size);
        vfs_node_t *node = new_node(name,&hdr,size,data_offset,ino_index);
	// Adjust the file permissions.
	node->mask = hdr.mode;
        vfs_node_t *parent = dir != NULL ? vfs_find(dir) : root;
        parent->flags = VFS_DIRECTORY;
        new_child_node(parent,node);
	ino_index++;
    }
    mountpoint->fs = cpio_fs;
    mountpoint->priv_data = p;
    mountpoint->mount_flags = VFS_MOUNT_RO;
    me->workDir = home;
    return true;
}

static uint64_t cpio_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
   if ((size_t)offset >= (uint64_t)node->size) return 0;
    how = min(how,node->size - offset);
    struct cpio *p = node->priv_data;
    //kprintf("Reading %d bytes, file size: %d\n",how,node->size);
    vfs_read(d,p->data + offset,how,buf);
    return how;
}
static uint64_t cpio_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {return 0;}
struct vfs_node *cpio_finddir(struct vfs_node *di,char *name) {
    char *krnName = strdup(name);
    void *curSpace = arch_mmu_getAspace();
    process_t *prc = thread_getThread(thread_getCurrent());
    void *prcSpace = prc->aspace;
    void *switchTo = arch_mmu_getKernelSpace();
    prc->aspace = switchTo;
    arch_mmu_switch(switchTo);
    struct cpio *p = (struct cpio *)di->priv_data;
    for (; p != NULL; p = p->child) {
	    if ((int)p->node >= KERN_HEAP_END) continue;
	    if ((int)p->child >= KERN_HEAP_END) continue;
	    if (p->node == NULL && p->child != NULL) continue; // looks like root
	    if (strcmp(p->node->name,krnName)) {
		    prc->aspace = prcSpace;
		    arch_mmu_switch(curSpace);
		    kfree(krnName);
		    return p->node;
		}
	}
    prc->aspace = prcSpace;
    arch_mmu_switch(curSpace);
    kfree(krnName);
    return NULL;
}
static struct dirent find_d;
struct dirent *cpio_readdir(struct vfs_node *di,uint32_t index) {
        if (index == 0) {
            strcpy(find_d.name,".");
            return &find_d;
        }
    if (index == 1) {
        strcpy(find_d.name,"..");
        return &find_d;
    }
    index -= 2;
    struct cpio *p = (struct cpio *)di->priv_data;
    // Debug.
    struct cpio *pa = p->child;
    for (int i = 0; i < p->dirSize; i++) {
	    if (pa == NULL) break;
	    if (pa->root != di) {
		    i--;
		    pa = pa->child;
		    continue;
		}
	    if (i == index) {
		    strcpy(find_d.name,pa->node->name);
		    return &find_d;
		}
	    pa = pa->child;
	}
    return NULL;
}

static void cpio_close(vfs_node_t *n) {}

void cpio_load(void *initrd,int dev_size) {
    
}
vfs_node_t *cpio_creat(vfs_node_t *in,char *name,int flags) {
	// Yes.
	if (flags != 3) {
		kprintf("cpio: only UNIX sockets can be created :)\r\n");
		return NULL;
	}
	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node,0,sizeof(vfs_node_t));
	node->name = strdup(name);
	node->fs = cpio_fs;
	struct cpio *cp = kmalloc(sizeof(struct cpio));
	memset(cp,0,sizeof(struct cpio));
	cp->node = node;
	node->priv_data = cp;
	new_child_node(in,node);
	return node;
}
void cpio_init() {
    cpio_fs = kmalloc(sizeof(vfs_fs_t));
    memset(cpio_fs,0,sizeof(vfs_fs_t));
    cpio_fs->fs_name = "cpio";
    cpio_fs->mount = cpio_mount;
    cpio_fs->read = cpio_read;
    cpio_fs->write = cpio_write;
    cpio_fs->finddir = cpio_finddir;
    cpio_fs->readdir = cpio_readdir;
    cpio_fs->close = cpio_close;
    cpio_fs->creat = cpio_creat;
    vfs_addFS(cpio_fs);
    DEBUG_N("CPIO FS added\r\n");
}

