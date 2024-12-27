/*
 * Temporary file system driver for HelinOS(kernel is HelinKern)
 *
 * Yes, primarialy used for program testing where the data can't be written to something other that RAM
 *
 * We use this FS to test the ports.
 *
*/
#define TMPFS_BLOCKSIZE 4096
#include <vfs.h>
#include <lib/string.h>
#include <mm/alloc.h>
#include <typedefs.h>
#include <fs/tmpfs.h>
#include <lib/queue.h>
#include <dev.h>
#include <output.h>
#include <arch/mmu.h>
#include <arch.h>
// Linked list to track the file blocks(can be random!)
typedef struct blockinfo {
	int id;
	void *data;
	struct blockinfo *next;
} BlockInfo;
typedef struct {
	vfs_node_t *node;
        vfs_node_t *parent;
        bool deleted;
        int fd_count;
	BlockInfo *dataQueue;
        queue_t *dirQueue; // queue of files. For directory only.
} TmpFSFile;
static int glInode = 1;
static vfs_fs_t *tmpfs;
static int map_flags = 0;
static char tmp_buff[4096] __attribute__((aligned(0x1000)));
void *tmpfs_getBlockPtr(TmpFSFile *file,int blockID);
uint64_t tmpfs_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	TmpFSFile *f = (TmpFSFile *)node->priv_data;
	uint64_t endptr = offset+size;
  	if (endptr > node->size) {
    		if (offset > 0 && offset > node->size) {
      			endptr = offset - node->size;
    		} else {
      			endptr = node->size;
    		}
  	}
 	uint64_t startBlock = offset / TMPFS_BLOCKSIZE;
	uint64_t end = endptr / TMPFS_BLOCKSIZE;
	uint64_t oper_size = (offset > node->size) ? endptr : endptr - offset;
	uint64_t remOff = endptr - end * TMPFS_BLOCKSIZE;
	uint64_t written = 0;
	if (startBlock == end) {
		void *blockptr = tmpfs_getBlockPtr(f,startBlock);
		memcpy(tmp_buff,(uint8_t*)(((uintptr_t)blockptr) + ((uintptr_t)offset % TMPFS_BLOCKSIZE)),oper_size);
		memcpy(buff,tmp_buff,oper_size);
		return oper_size;
	} else {
		for (int blkoff = startBlock; blkoff < end; blkoff++,written++) {
			if (blkoff == startBlock) {
				void *blockptr = tmpfs_getBlockPtr(f,blkoff);
				memcpy(tmp_buff,(uint8_t*)(((uintptr_t)blockptr) + ((uintptr_t)offset % TMPFS_BLOCKSIZE)),TMPFS_BLOCKSIZE - (offset % TMPFS_BLOCKSIZE));
				memcpy(buff,tmp_buff,TMPFS_BLOCKSIZE - (offset % TMPFS_BLOCKSIZE));
			} else {
				void *blockptr = tmpfs_getBlockPtr(f,blkoff);
				memcpy(tmp_buff,blockptr,TMPFS_BLOCKSIZE);
				memcpy(buff + TMPFS_BLOCKSIZE * written - (offset % TMPFS_BLOCKSIZE),tmp_buff,TMPFS_BLOCKSIZE);
			}
		}
	}
	if (remOff) {
		void *last = tmpfs_getBlockPtr(f,end);
		memcpy(buff + TMPFS_BLOCKSIZE * written - (offset % TMPFS_BLOCKSIZE),last,remOff);
	}
	return oper_size;
}
vfs_node_t *tmpfs_creat(vfs_node_t *where,char *name,int flags) {
	vfs_node_t *newNode = kmalloc(sizeof(vfs_node_t));
	memset(newNode,0,sizeof(vfs_node_t));
	newNode->name = strdup(name);
	newNode->flags = flags;
	newNode->mask =  (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH | S_IXOTH);
        newNode->inode = glInode;
        glInode++;
	newNode->fs = tmpfs;
	newNode->size = 0;
	TmpFSFile *fl = kmalloc(sizeof(TmpFSFile));
	memset(fl,0,sizeof(TmpFSFile));
	fl->node = newNode;
        fl->parent = where;
        if ((flags & VFS_DIRECTORY) == VFS_DIRECTORY) {
                // Create directory node queue.
                fl->dirQueue = queue_new();
        }
        //	memset(fl->dataQueue,0,sizeof(BlockInfo));
        newNode->priv_data = fl;
	TmpFSFile *wheref = (TmpFSFile *)where->priv_data;
        enqueue(wheref->dirQueue,newNode);
	// Put ourself here.
	return newNode;
}

vfs_node_t *tmpfs_finddir(vfs_node_t *where,char *who) {
	if (where->fs != tmpfs) return NULL;
	TmpFSFile *start = (TmpFSFile *)where->priv_data;
        if (start->dirQueue == NULL) {
                return NULL;
        }
        queue_for(el,start->dirQueue) {
                vfs_node_t *node = (vfs_node_t *)el->value;
                if (strcmp(node->name,who)) {
                        return node;
                }
        }
	return NULL;
}
static struct dirent staticDirent;
static int tmpfs_readdir(vfs_node_t *where,unsigned int index,struct dirent *to) {
	if (index == 0) {
		strcpy(to->name,".");
		return 1;
	} else if (index == 1) {
		strcpy(to->name,"..");
		return 1;
	}
	index-=2;
	int i = 0;
	TmpFSFile *start = (TmpFSFile *)where->priv_data;
        // Check!
        if (start->dirQueue == NULL) {
                return ENOTDIR; // no dir queue -> not a dir!
        }
        queue_for(el,start->dirQueue) {
                vfs_node_t *node = (vfs_node_t *)el->value;
        if (i == index) {
                strcpy(to->name,node->name);
                return 1;
        }
        i++;
        }
	return 0;
}
bool tmpfs_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *unused) {
	TmpFSFile *root = (TmpFSFile *)kmalloc(sizeof(TmpFSFile));
	memset(root,0,sizeof(TmpFSFile));
        root->dirQueue = queue_new();
	mountpoint->priv_data = root;
	mountpoint->fs = tmpfs;
	return true;
}
void *tmpfs_getBlockPtr(TmpFSFile *file,int blockID) {
	// Just go throuth the list.
	BlockInfo *inf = file->dataQueue;
	while(inf != NULL) {
		if (inf->id == blockID) {
      //arch_mmu_mapPage(NULL,(uintptr_t)buff,(uintptr_t)inf->data,3);
			return inf->data;
		}
		inf = inf->next;
	}
	return NULL;
}
void *tmpfs_getOrCreateBlock(TmpFSFile *file,int blockID) {
	void *to = tmpfs_getBlockPtr(file,blockID);
	if (to != NULL) {
		return to;
	}
	BlockInfo *inf = (BlockInfo *)kmalloc(sizeof(BlockInfo));
	memset(inf,0,sizeof(BlockInfo));
	inf->id = blockID;
	uintptr_t page = alloc_getPage();
        uintptr_t toAddr = arch_mmu_get_io_addr(4096);
        arch_mmu_mapPage(NULL,toAddr,page,3);
        inf->data = (void *)toAddr;
  	memset(inf->data,0,TMPFS_BLOCKSIZE);
	BlockInfo *putTo = file->dataQueue;
  	if (blockID == 0) {
    		file->dataQueue = inf;
    		goto m;
  	}
	while(putTo->next != NULL) {putTo = putTo->next;}
	putTo->next = inf;
	to = inf->data;
m:
  //arch_mmu_mapPage(NULL,(uintptr_t)buff,(uintptr_t)inf->data,3);
	return inf->data;
}

static vfs_node_t *serial = NULL;
void dumptoserial(void *ptr) {
	if (serial == NULL) {
		dev_t *ser = dev_find("serial");
		if (ser == NULL) return;
		serial = ser->devNode;
	}
}
uint64_t tmpfs_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *ptr) {
	arch_cli();
	uint64_t endptr = offset+size;
	if (endptr > node->size) {
		node->size = endptr;
	}
	uint64_t startBlock = offset / TMPFS_BLOCKSIZE;
	uint64_t end = endptr / TMPFS_BLOCKSIZE;
	uint64_t oper_size = endptr - offset;
	uint64_t remOff = endptr - end * TMPFS_BLOCKSIZE;
	uint64_t written = 0;
	TmpFSFile *f = (TmpFSFile *)node->priv_data;
	process_t *caller = thread_getThread(thread_getCurrent());
	void *aspace = caller->aspace;
	caller->aspace = arch_mmu_getKernelSpace();
	if (startBlock == end) {
		memcpy(tmp_buff,ptr,oper_size);
		arch_mmu_switch(caller->aspace);
		void *blockptr = tmpfs_getOrCreateBlock(f,startBlock);
		memcpy((uint8_t*)(((uintptr_t)blockptr) + ((uintptr_t)offset % TMPFS_BLOCKSIZE)),tmp_buff,oper_size);
		caller->aspace = aspace;
		arch_mmu_switch(aspace);
		return oper_size;
	} else {
		for (int blkoff = startBlock; blkoff < end; blkoff++,written++) {
			if (blkoff == startBlock) {
				memcpy(tmp_buff,ptr,TMPFS_BLOCKSIZE - (offset % TMPFS_BLOCKSIZE));
				arch_mmu_switch(caller->aspace);
				void *blockptr = tmpfs_getOrCreateBlock(f,blkoff);
				memcpy((uint8_t*)(((uintptr_t)blockptr) + ((uintptr_t)offset % TMPFS_BLOCKSIZE)),tmp_buff,TMPFS_BLOCKSIZE - (offset % TMPFS_BLOCKSIZE));
				arch_mmu_switch(aspace);
			} else {
				memcpy(tmp_buff,ptr + TMPFS_BLOCKSIZE * written - (offset % TMPFS_BLOCKSIZE),TMPFS_BLOCKSIZE);
				arch_mmu_switch(caller->aspace);
				void *blockptr = tmpfs_getOrCreateBlock(f,blkoff);
				memcpy(blockptr,tmp_buff,TMPFS_BLOCKSIZE);
				arch_mmu_switch(aspace);
			}
		}
	}
	if (remOff) {
                memcpy(tmp_buff,ptr + TMPFS_BLOCKSIZE * written - (offset % TMPFS_BLOCKSIZE),remOff);
                arch_mmu_switch(caller->aspace);
		void *last = tmpfs_getOrCreateBlock(f,end);
                memcpy(last,tmp_buff,remOff);
                arch_mmu_switch(aspace);
	//	memcpy(last,ptr + TMPFS_BLOCKSIZE * written - (offset & TMPFS_BLOCKSIZE),remOff);
	}
	caller->aspace = aspace;
	arch_mmu_switch(aspace);
	return size;
}
bool tmpfs_rm(vfs_node_t *node) {
  TmpFSFile *file = (TmpFSFile *)node->priv_data;
  // Remove the file from directory.
  vfs_node_t *parent = file->parent;
  TmpFSFile *parentN = (TmpFSFile *)parent->priv_data;
  if (parentN->dirQueue == NULL) {
    return false;
  }
  queue_remove(parentN->dirQueue,node);
  // Destroy node properties.
  BlockInfo *blocks = file->dataQueue;
  BlockInfo *nextBlock = NULL;
  while(blocks != NULL) {
          nextBlock = blocks->next;
          // Free page and unmap it.
          uintptr_t phys = (uintptr_t)arch_mmu_getPhysical(blocks->data);
          alloc_freePage(phys);
          arch_mmu_unmap(NULL,(vaddr_t)blocks->data,1);
          kfree(blocks);
          blocks = nextBlock;
  }
  if (file->fd_count <= 0) {
          // Delete file.
          kfree(file);
          kfree(node->name);
          kfree(file);
  }
  return true;
}
void tmpfs_close(vfs_node_t *node) {
        TmpFSFile *file = (TmpFSFile *)node->priv_data;
        file->fd_count--;
        if (file->deleted && file->fd_count <= 0) {
                kfree(file);
                kfree(node->name);
                kfree(node);
        }
}
void tmpfs_init() {
	map_flags = arch_mmu_processFlags(PROT_READ | PROT_WRITE);
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
        tmpfs->rm = tmpfs_rm;
        tmpfs->close = tmpfs_close;
	vfs_addFS(tmpfs);
}


