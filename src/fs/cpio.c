#include <fs/cpio.h>
#include <output.h>
#include <debug.h>
#include <lib/string.h>
#include <mm/alloc.h>

static vfs_fs_t *cpio_fs;
static vfs_node_t *root;
static vfs_node_t *d;

static vfs_node_t *new_node(const char *name,struct cpio_hdr *hdr,size_t sz,size_t data) {
    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    memset(node,0,sizeof(vfs_node_t));
    struct cpio *p = kmalloc(sizeof(struct cpio));
    memset(p,0,sizeof(struct cpio));
    node->size = sz;
    node->uid = node->guid = 0;
    node->priv_data = p;
    node->fs = cpio_fs;
    p->name = strdup(name);
    p->data = data;
    node->name = p->name;
    kprintf("Pasted node size: %d\n",sz);
    return node;
}

static void new_child_node(vfs_node_t *parent,vfs_node_t *child) {
    if (!parent || !child) return;
    if (parent->flags != VFS_DIRECTORY) return;
    struct cpio *pp = (struct cpio *)parent->priv_data;
    struct cpio *cp = (struct cpio *)child->priv_data;
    vfs_node_t *tmp = pp->dir;
    cp->next = tmp;
    pp->dir = child;
    pp->count++;
    cp->parent = parent;
}

static bool cpio_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *params) {
    DEBUG("Loading CPIO initrd from %s\r\n",dev->name);
    // HACK!
    process_t *me = thread_getThread(thread_getCurrent());
    vfs_node_t *home = me->workDir;
    struct cpio_hdr hdr;
    int offset = 0;
    int size = 0;
    // FIXME: Add the header checks at mount!
    root = mountpoint;
    d = dev;
    root->fs = cpio_fs;
    struct cpio *p = kmalloc(sizeof(struct cpio));
    memset(p,0,sizeof(struct cpio));
    root->priv_data = p;
    int dev_size = dev->size;
    me->workDir = mountpoint;
    for (; offset < dev_size; offset +=sizeof(struct cpio_hdr)+(hdr.namesize+1)/2*2 + (size+1)/2*2) {
        int data_offset = offset;
        vfs_read(dev,data_offset,sizeof(struct cpio_hdr),&hdr);
        if (hdr.magic != CPIO_MAGIC) {
            DEBUG_N("Invalid magic\r\n");
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
	DEBUG("Processing file: %s\r\n",path);
        data_offset += hdr.namesize + (hdr.namesize % 2);
        vfs_node_t *node = new_node(name,&hdr,size,data_offset);
        vfs_node_t *parent = dir != NULL ? vfs_find(dir) : root;
        parent->flags = VFS_DIRECTORY;
        new_child_node(parent,node);
    }
    me->workDir = home;
    return true;
}

static int cpio_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
   if ((size_t)offset >= node->size) return 0;
    how = min(how,node->size - offset);
    struct cpio *p = node->priv_data;
    //kprintf("Reading %d bytes, file size: %d\n",how,node->size);
    vfs_read(d,p->data + offset,how,buf);
    return how;
}
static int cpio_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {return 0;}

struct vfs_node *cpio_finddir(struct vfs_node *di,char *name) {
    struct cpio *p = (struct cpio *)di->priv_data;
    size_t i = 0;
    vfs_node_t *dir = p->dir;
    for (vfs_node_t *e = dir; e != NULL; e = ((struct cpio *) e->priv_data)->next) {
        char *n = (((struct cpio *)e->priv_data)->name);
        if (strcmp(n,name)) {
            return e;
        }
        ++i;
        if (i == p->count) break;
    }
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
    if (index == p->count) return NULL;
    size_t i = 0;
    vfs_node_t *dir = p->dir;
    for (vfs_node_t *e = dir; e != NULL; e = ((struct cpio *) e->priv_data)->next) {
        if (i == index) {
            
            strcpy(find_d.name,((struct cpio *)e->priv_data)->name);
            return &find_d;
        }
        ++i;
    }
    return NULL;
}

static void cpio_close(vfs_node_t *n) {}

void cpio_load(void *initrd,int dev_size) {
    
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
    vfs_addFS(cpio_fs);
    DEBUG_N("CPIO FS added\r\n");
}

