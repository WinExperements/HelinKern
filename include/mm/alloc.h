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
#define KERN_HEAP_BEGIN 		0x02000000 //32 mb
#define KERN_HEAP_END    		0x40000000 // 1 gb
#define	USER_OFFSET         	0x40000000
#define	USER_MMAP_START     	0x80000000 //This is just for mapping starts searching vmem from here not to conflict with sbrk. It can start from USER_OFFSET if sbrk not used!
#define	MEMORY_END              0xFFC00000 //After this address is not usable. Because Page Tables sit there!

#define KMALLOC_MINSIZE		16
void alloc_init(int kernel_end,int high_mem);
int alloc_getPage();
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
#endif
