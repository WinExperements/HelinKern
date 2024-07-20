#ifndef ALLOC_H
#define ALLOC_H
#include <typedefs.h>
#include <thread.h>
#define PAGE_INDEX_4K(addr)		((addr) >> 12)
#define PAGE_INDEX_4M(addr)		((addr) >> 22)
#define SET_PAGEFRAME_USED(bitmap, page_index)	bitmap[((uint32_t) page_index)/8] |= (1 << (((uint32_t) page_index)%8))
#define SET_PAGEFRAME_UNUSED(bitmap, p_addr)	bitmap[((uint32_t) p_addr/PAGESIZE_4K)/8] &= ~(1 << (((uint32_t) p_addr/PAGESIZE_4K)%8))
#define IS_PAGEFRAME_USED(bitmap, page_index)	(bitmap[((uint32_t) page_index)/8] & (1 << (((uint32_t) page_index)%8)))
#define	PAGESIZE_4K 		0x00001000
#define	PAGESIZE_4M			0x00400000
extern int KERN_HEAP_BEGIN;
extern int KERN_HEAP_END; // must be defined somewhere
extern int USER_OFFSET;
extern int USER_MMAP_START;
extern int MEMORY_END;

#define KMALLOC_MINSIZE		16
void alloc_init(uintptr_t kernel_end,uintptr_t high_mem);
paddr_t alloc_getPage();
// Kernel heap

struct MallocHeader
{
    unsigned long size:31;
    unsigned long used:1;
} __attribute__ ((packed));

typedef struct MallocHeader MallocHeader;
void kheap_init();
void *ksbrk_page(int n);
void *kmalloc(int size);
void kfree(void *addr);
void *sbrk(process_t* process, int n_bytes);
void alloc_initProcess(process_t *prc,int size);
int alloc_getUsedSize();
// Required by FAT32!
void *krealloc(void *p,int size);
void alloc_freePage(int addr);
int alloc_getBitmapSize();
void alloc_mapItself();
int alloc_getEnd(); // needed for FB :(
void alloc_reserve(int start,int end); // reserve there address space
unsigned long alloc_getUsedPhysMem(); // get used physical memory
unsigned long alloc_getAllMemory(); // get all available amount of memory
int alloc_alignAddress(int address,int aligment);
#endif
