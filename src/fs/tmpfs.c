/*
 * Temple file system driver for HelinOS(kernel is HelinKern)
 *
 * Yes, primarialy used for program testing where the data can't be written to something other that RAM
 *
 * We use this FS to test the ports.
 *
*/
#define TMPFS_BLOCKSIZE 2048
#include <vfs.h>
#include <lib/string.h>
#include <mm/alloc.h>
#include <typedefs.h>
#include <fs/tmpfs.h>
#include <lib/queue.h>
typedef struct {
	vfs_node_t *node;
	queue_t *dataQueue;
	vfs_node_t *next;
} TmpFSFile;
static vfs_fs_t *tmpfs;

void tmpfs_readBlock(vfs_node_t *from,int block,void *to) {
	TmpFSFile *dat = (TmpFSFile *)from->priv_data;
	int i = 0;
	queue_for(element,dat->dataQueue) {
		if (i == block) {
			memcpy(to,element->value,TMPFS_BLOCKSIZE);
			return;
		}
		i++;
	}
}
uint64_t tmpfs_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	return 0;
}
vfs_node_t *tmpfs_creat(vfs_node_t *where,char *name,int flags) {
	vfs_node_t *newNode = kmalloc(sizeof(vfs_node_t));
	memset(newNode,0,sizeof(vfs_node_t));
	newNode->name = strdup(name);
	newNode->flags = flags;
	newNode->mask =  (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
	newNode->fs = tmpfs;
	TmpFSFile *fl = kmalloc(sizeof(TmpFSFile));
	memset(fl,0,sizeof(TmpFSFile));
	fl->node = newNode;
	fl->dataQueue = queue_new();
	newNode->priv_data = fl;
	TmpFSFile *wheref = (TmpFSFile *)where->priv_data;
	if (wheref->next == NULL) {
		wheref->next = newNode;
	} else {
		while(wheref->next != NULL) {
			wheref = (TmpFSFile *)wheref->next->priv_data;
		}
	}
	// Put ourself here.
	wheref->next = newNode;
	return newNode;
}

vfs_node_t *tmpfs_finddir(vfs_node_t *where,char *who) {
	if (where->fs != tmpfs) return NULL;
	TmpFSFile *start = (TmpFSFile *)where->priv_data;
	while(start != NULL) {
		if (start->node == NULL) {
			return NULL;
		}
		if (strcmp(start->node->name,who)) {
			return start->node;
		}
		if (start->next == NULL) break;
		start = (TmpFSFile *)start->next->priv_data;
	}
	return NULL;
}
static struct dirent staticDirent;
struct dirent *tmpfs_readdir(vfs_node_t *where,unsigned int index) {
	if (index == 0) {
		strcpy(staticDirent.name,".");
		return &staticDirent;
	} else if (index == 1) {
		strcpy(staticDirent.name,"..");
		return &staticDirent;
	}
	index-=2;
	int i = 0;
	TmpFSFile *start = (TmpFSFile *)where->priv_data;
	// Skip ourself.
	if (start->next == NULL) return NULL;
	start = start->next->priv_data;
	for (; start != NULL; 
			start = (TmpFSFile *)start->next->priv_data) {
		if (start->node == NULL) break;
		if (i == index) {
			strcpy(staticDirent.name,start->node->name);
			return &staticDirent;
		}
		if (start->next == NULL) break;
		start = (TmpFSFile *)start->next->priv_data;
		i++;
	}
	return NULL;
}
bool tmpfs_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *unused) {
	TmpFSFile *root = (TmpFSFile *)kmalloc(sizeof(TmpFSFile));
	memset(root,0,sizeof(TmpFSFile));
	mountpoint->priv_data = root;
	mountpoint->fs = tmpfs;
	return true;
}
void tmpfs_init() {
	tmpfs = kmalloc(sizeof(vfs_fs_t));
	memset(tmpfs,0,sizeof(vfs_fs_t));
	tmpfs->fs_name = "tmpfs";
	tmpfs->read = tmpfs_read;
	tmpfs->creat = tmpfs_creat;
	tmpfs->finddir = tmpfs_finddir;
	tmpfs->readdir = tmpfs_readdir;
	tmpfs->mount = tmpfs_mount;
	vfs_addFS(tmpfs);
}


