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
// Linked list to track the file blocks(can be random!)
typedef struct blockinfo {
	int id;
	void *data;
	struct blockinfo *next;
} BlockInfo;
typedef struct {
	vfs_node_t *node;
	BlockInfo *dataQueue;
	vfs_node_t *next;
} TmpFSFile;
static vfs_fs_t *tmpfs;
void *tmpfs_getBlockPtr(TmpFSFile *file,int blockID);
uint64_t tmpfs_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	int dataBlock = offset / TMPFS_BLOCKSIZE;
	int blocks = size / TMPFS_BLOCKSIZE;
	int offsetIndex = offset / TMPFS_BLOCKSIZE;
	int remainOffset = offset % TMPFS_BLOCKSIZE;
	TmpFSFile *f = (TmpFSFile *)node->priv_data;
	if (f == NULL || !f->dataQueue) {
		return 0;
	}
	void *ptr = NULL;
	uintptr_t buffAddr = (uintptr_t)buff;
	if (blocks == 0) {
		ptr = tmpfs_getBlockPtr(f,dataBlock);
		if (ptr == NULL) return 0;
		uintptr_t blockAddr = (uintptr_t)ptr;
		buffAddr+=remainOffset;
		blockAddr+=remainOffset;
		memcpy((void *)buffAddr,(void *)blockAddr,size);
		return size;
	}
	return 0;
}
vfs_node_t *tmpfs_creat(vfs_node_t *where,char *name,int flags) {
	vfs_node_t *newNode = kmalloc(sizeof(vfs_node_t));
	memset(newNode,0,sizeof(vfs_node_t));
	newNode->name = strdup(name);
	newNode->flags = flags;
	newNode->mask =  (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
	newNode->fs = tmpfs;
	newNode->size = TMPFS_BLOCKSIZE;
	TmpFSFile *fl = kmalloc(sizeof(TmpFSFile));
	memset(fl,0,sizeof(TmpFSFile));
	fl->node = newNode;
	fl->dataQueue = kmalloc(sizeof(BlockInfo));
	memset(fl->dataQueue,0,sizeof(BlockInfo));
	fl->dataQueue->data = kmalloc(TMPFS_BLOCKSIZE);
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
	if (start->node == NULL) {
		// Root?
		if (start->next == NULL) {
			return NULL; // broken node?
		}
		start = start->next->priv_data;
	}
	for (; start != NULL; start = (TmpFSFile*)start->next->priv_data) {
		if (start->node == NULL) break;
		if (strcmp(start->node->name,who)) {
			return start->node;
		}
		if (start->next == NULL) break;
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
void *tmpfs_getBlockPtr(TmpFSFile *file,int blockID) {
	// Just go throuth the list.
	BlockInfo *inf = file->dataQueue;
	while(inf != NULL) {
		if (inf->id == blockID) {
			return inf->data;
		}
		inf = inf->next;
	}
	return NULL;
}
void tmpfs_writeBlock(TmpFSFile *file,int blockID,int size,void *ptr,int blockOffset) {
	// Write the data if block is already allocated or allocate new block.
	void *to = tmpfs_getBlockPtr(file,blockID);
	uintptr_t blockAddr = 0;
	if (to != NULL) {
		// Block exist, write to it.
		blockAddr = (uintptr_t)to;
		blockAddr +=blockOffset;
		memcpy((void *)blockAddr,ptr,size);
	} else {
		// Allocate if not found previously.
		BlockInfo *inf = kmalloc(sizeof(BlockInfo));
		memset(inf,0,sizeof(BlockInfo));
		inf->id = blockID;
		// Allocate whole block.
		inf->data = kmalloc(TMPFS_BLOCKSIZE);
		// Put the block into block queue.
		BlockInfo *putTo = file->dataQueue;
		while(putTo->next != NULL) {putTo = putTo->next;}
		putTo->next = inf;
		to = inf->data;
		blockAddr = (uintptr_t)to;
		blockAddr+=blockOffset;
		memcpy((void *)blockAddr,ptr,size);
		// Increse the file size.
		file->node->size += size;
	}
}
uint64_t tmpfs_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *ptr) {
	TmpFSFile *start = (TmpFSFile *)node->priv_data;
	int offsetIndex = offset / TMPFS_BLOCKSIZE;
	int remainOffset = offset % TMPFS_BLOCKSIZE;
	int remain = size % TMPFS_BLOCKSIZE;
	int dataBlock = offsetIndex;
	int sectors = size / TMPFS_BLOCKSIZE;
	void *sector = kmalloc(TMPFS_BLOCKSIZE);
	uintptr_t blockaddr = 0;
	if (sectors == 0) {
		// x86_64 doesn't like adding some values to pointers....
		blockaddr = (uintptr_t)ptr;
		blockaddr+=remainOffset;
		tmpfs_writeBlock(start,dataBlock,size,(void *)blockaddr,remainOffset);
		kfree(sector);
		return size;
	}
	if (remainOffset != 0) {
		int scopy = remainOffset;
		if (scopy == 0) {
			scopy = TMPFS_BLOCKSIZE;
		}
		if (scopy+remainOffset >= TMPFS_BLOCKSIZE) {
			scopy = TMPFS_BLOCKSIZE - remainOffset;
		}
		memcpy(sector,ptr+remainOffset,scopy);
		tmpfs_writeBlock(start,dataBlock,TMPFS_BLOCKSIZE,sector,remainOffset);
		size-=scopy;
		dataBlock++;
		ptr+=scopy;
		if (scopy+remainOffset < TMPFS_BLOCKSIZE) {
			int how = TMPFS_BLOCKSIZE - (scopy+remainOffset);
			if (how < size) {
				memcpy(sector,ptr+(scopy+remainOffset),how);
				tmpfs_writeBlock(start,dataBlock-1,TMPFS_BLOCKSIZE,sector,(scopy+remainOffset));
				ptr+=how;
				size-=how;
				remain+=remainOffset;
				remainOffset = 0;
			}
		}
		sectors--;
		if (remainOffset*2 >= TMPFS_BLOCKSIZE) {
			remainOffset = 0;
		}
	}
	while(sectors != 0) {
		tmpfs_writeBlock(start,dataBlock,TMPFS_BLOCKSIZE,ptr,0);
		dataBlock++;
		sectors--;
		size-=TMPFS_BLOCKSIZE;
		ptr+=TMPFS_BLOCKSIZE;
	}
	if (remain != 0) {
		tmpfs_writeBlock(start,dataBlock,size,ptr+remainOffset,remainOffset);
		size-=remain;
		ptr+=remain;
		if (size != 0) {
			if (size+remain > TMPFS_BLOCKSIZE) {
				int how = TMPFS_BLOCKSIZE - remain;
				dataBlock++;
				ptr+=how;
				size-=how;
				tmpfs_writeBlock(start,dataBlock,size,ptr,0);
			}
		}
	}
	kfree(sector);
	return size;
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
	tmpfs->write = tmpfs_write;
	tmpfs->read = tmpfs_read;
	vfs_addFS(tmpfs);
}


