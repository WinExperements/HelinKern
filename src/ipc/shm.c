// Shared memory IPC.
#include <ipc/shm.h>
#include <mm/alloc.h>
#include <ipc/ipc_manager.h>
#include <output.h>
#include <lib/string.h>
#include <lib/queue.h>
#include <stdarg.h>
#include <thread.h>
#include <vfs.h>
#include <arch/mmu.h>
#include <arch.h>
#include <lib/queue.h>
struct shm_obj {
	process_t *owner;
	int uid;
	int gid;
	int mode;
	int perms;
	int id;
	int key;
	int size;
	queue_t *pageQueue;	// Queue to track the page numbers per SHM, used because in some cases we can corrupt the whole
				// system memory if we don't do it.
	int atCnt;	// Track count of attaches. If it's value will be zero and the shmdt will be called, then the kernel
			// will free the resources.
	bool destroyed;
};
struct shm_cmd {
	int key;
	void *mapAddr;
	int size;
	int flags;
};
static int shm_id; // Next shared-memory ID.
static queue_t *shmQueue; // Queue of SHM objects.
static vfs_fs_t *shmFs;
static void shm_fs_close(vfs_node_t *node);
// SHM functions.
struct shm_obj *shm_find(int id);
struct shm_obj *shm_findByKey(int key);
int shm_create(process_t *caller,void *args) {
	// doesn't supported.
	struct shm_cmd *msg = (struct shm_cmd *)args;
	vfs_node_t *shmNode = NULL;
	int key = msg->key;
	size_t size = msg->size;
	int shmflags = msg->flags;
	// Now we need to generate the ID somehow...
	// Check if we need to create this IPC SHM object or no.
	struct shm_obj *st = NULL;
	if (msg->flags == 0) {
		// Try to find the object.
		st = shm_findByKey(msg->key);
		if (st == NULL) {
			return -1;
		}
		goto createNode;
	}
	st = kmalloc(sizeof(struct shm_obj));
	memset(st,0,sizeof(struct shm_obj));
	st->owner = caller;
	st->id = shm_id;
	st->key = msg->key;
	if (key == 0) {
		shm_id++;
	}
	st->size = msg->size;
	if (st->size > 0) {
		arch_cli();
		// Allocate this chunk of memory.
		st->pageQueue = queue_new();
		for (int i = 0; i < (st->size/4096)+1; i++) {
			uintptr_t page = alloc_getPage();
			if (page == -1) {
				PANIC("Please clean our up");
			}
			enqueue(st->pageQueue,(void *)page);
		}
		arch_sti();
	}
	enqueue(shmQueue,st);
createNode:
	// Create and open SPECIAL SHM file(to allow this resource auto clean after process exit)
	shmNode = kmalloc(sizeof(vfs_node_t));
	memset(shmNode,0,sizeof(vfs_node_t));
	shmNode->fs = shmFs;
	shmNode->priv_data = st;
	shmNode->inode = shm_id;
	thread_openFor(caller,shmNode,FD_RDWR);
	return st->id;
}
struct shm_obj *shm_find(int id) {
	queue_for(obj,shmQueue) {
		struct shm_obj *b = (struct shm_obj *)obj->value;
		if (b->id == id) {
			return b;
		}
	}
	return NULL;
}
struct shm_obj *shm_findByKey(int key) {
	queue_for(obj,shmQueue) {
		struct shm_obj *b = (struct shm_obj *)obj->value;
		if (b->key == key) {
			return b;
		}
	}
	return NULL;
}
int shm_command(process_t *caller,int cmd,void *args) {
	struct shm_cmd *msg = (struct shm_cmd *)args;
	struct shm_obj *obj = NULL;
	switch(cmd) {
		case 0x2: {
				  // shmat.
				  obj = shm_find(msg->key);
				  if (obj == NULL) {
					  return -1;
				  }
				  // Find memory hole where we can fit.
				  // 32-bit integer due to some extreme address given if don't use it.
				  uint32_t where = arch_mmu_query(NULL,(uintptr_t)(uint32_t)USER_MMAP_START,obj->size+4096);
				  uint32_t off = 0;
				  int map_flags = arch_mmu_processFlags(PROT_WRITE | PROT_READ | PROT_USER);
				  queue_for(sh,obj->pageQueue) {
					  paddr_t pagePtr = (paddr_t)sh->value;
					  arch_mmu_mapPage(NULL,where+off,pagePtr,7);
					  off+=4096;
				  }
				  obj->atCnt++;
				  return where;
			  } break;
		case 0x3: {
				  // Args is actually virtual address of attached shm object....
				  // on 64 bit systems we process this address differenctly.
				  uintptr_t argsAddr = (uintptr_t)args;
				  void *phys_addr = arch_mmu_getPhysical((void *)(uintptr_t)(uint32_t)argsAddr);
				  // Now find the SHM object.
				  queue_for(sh,shmQueue) {
					  struct shm_obj *shobj = (struct shm_obj *)sh->value;
					  if (shobj->pageQueue == NULL) continue;
					  paddr_t valuePtr = (paddr_t)shobj->pageQueue->head->value;
					  if (valuePtr == (paddr_t)phys_addr) {
						  obj = shobj;
						  break;
					  }
				   }
				  if (obj == NULL) return -1;
				  obj->atCnt--;
				  arch_mmu_unmap(NULL,(vaddr_t)(uint32_t)argsAddr,(obj->size/4096)+1);
				  if (obj->atCnt <= 0) {
					  kprintf("[shm]: Freeing resources used by this SHM segment\r\n");
					  queue_for(sh,obj->pageQueue) {
						  paddr_t pagePtr = (paddr_t)sh->value;
						  alloc_freePage(pagePtr);
					  }
					  while(dequeue(obj->pageQueue));
					  kfree(obj->pageQueue);
					  queue_remove(shmQueue,obj);
					  // Notify the object that it has been already destroyed(see shm_fs_close)
					  obj->destroyed = true;
					  kfree(obj);
				 }
			  } break;
		case 99: {
				 // Destroy this object.
				 obj = shm_find(msg->key);
				 if (obj == NULL) return -1;
				 // Check if we don't need to clean this thing.
				 if (obj->atCnt <= 0) {
					 queue_for(sh,obj->pageQueue) {                  
                                                  paddr_t pagePtr = (paddr_t)sh->value;
                                                  alloc_freePage(pagePtr);
                                          }       
                                          while(dequeue(obj->pageQueue));
                                          kfree(obj->pageQueue);
					 queue_remove(shmQueue,obj);
					 obj->destroyed = true;
				 }
			 } break;
		default:
			 return -1;
	}
	return 0;
}
int shm_destroy(process_t *prc) {
	return 0;
}
void shm_init() {
	Ipc *shm_ipc = kmalloc(sizeof(Ipc));
	memset(shm_ipc,0,sizeof(Ipc));
	shm_ipc->name = "shared-memory";
	shm_ipc->magicID = 'S';
	shm_ipc->createIPC = shm_create;
	shm_ipc->command = shm_command;
	shm_ipc->destroyIPC = shm_destroy;
	ipc_add(shm_ipc);
	shmQueue = queue_new();
	shmFs = kmalloc(sizeof(vfs_fs_t));
	memset(shmFs,0,sizeof(vfs_fs_t));
	shmFs->fs_name = "shm-fs";
	shmFs->close = shm_fs_close;
	kprintf("Shared-Memory IPC added\r\n");
}
void shm_fs_close(vfs_node_t *node) {
	struct shm_obj *obj = (struct shm_obj *)node->priv_data;
	// Unmap this area from process virtual memory(first actual use of arch_mmu_unmap)
//	arch_mmu_unmap(NULL,USER_MMAP_START,(obj->size/4096)+1);
	if (obj->destroyed) return;
	arch_cli();
	if (thread_getCurrent() == obj->owner->pid) {
		// Free memory used by this object.
		queue_for(sh,obj->pageQueue) {                 
			paddr_t pagePtr = (paddr_t)sh->value;
                	alloc_freePage(pagePtr);
                }       
                while(dequeue(obj->pageQueue));
                kfree(obj->pageQueue);
		queue_remove(shmQueue,obj);
		kfree(obj);
	}
	kfree(node);
}
