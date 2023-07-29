#include <mm/alloc.h>
#include <arch/mmu.h>
#include <arch.h>
#include <output.h>
#include <lib/string.h>
#include <debug.h>
/*
    Ported version of https://github.com/ozkl/soso/blob/master/kernel/alloc.c
*/
// Ядро дуже не стабільне!
static uint8_t *phys_map;
static int total_pages = 0;
static char *m_kheap = NULL;
static int heap_used;
static bool mapped = false;
static int pagesUsedBeforeMapping = 0;
static int kernel_endAddress;
static int end_address;
static int numAllocs = 0; // debug only!
static int lastAlloc = 0;
void alloc_init(int kernel_end,int high_mem) {
    // Place map at the end of kernel address
    phys_map = (uint8_t *)kernel_end;
    // Clean the map
    total_pages = (high_mem*1024)/4096;
    int pg = 0;
    kernel_endAddress = (int)kernel_end;
    for (pg = 0; pg < total_pages / 8; ++pg) {
        phys_map[pg] = 0x0;
    }
    int bitmapSize = total_pages*sizeof(uint16_t);
    for (pg = 0; pg < (int)(PAGE_INDEX_4K(kernel_end+bitmapSize)); ++pg) {
        SET_PAGEFRAME_USED(phys_map,pg);
    }
    end_address = kernel_end+(total_pages*8);
    kprintf("Kernel end address: 0x%x, allocation start address: 0x%x\n",kernel_end,end_address);
}
int alloc_getPage() {
    if (!mapped) {
        pagesUsedBeforeMapping++;
        return end_address+(pagesUsedBeforeMapping*PAGESIZE_4K);
    }
    int byte, bit;
    uint32_t page = -1;
    for (byte = 0; byte < total_pages / 8; byte++)
    {
        if (phys_map[byte] != 0xFF)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (!(phys_map[byte] & (1 << bit)))
                {
                    page = 8 * byte + bit;
                    SET_PAGEFRAME_USED(phys_map, page);
                    return (page * PAGESIZE_4K);
                }
            }
        }
    }
    return -1;
}
void kheap_init() {
	m_kheap = (char *)KERN_HEAP_BEGIN;
	// Fix of major bug
	// Map the phys_map even if it is already mapped(double check)
	DEBUG_N("Mapping phys_map...");
	/*int addr = (int)phys_map;
	aspace_t *space = arch_mmu_getAspace();
	for (int i = 0; i < total_pages; i++) {
		arch_mmu_mapPage(space,(void *)addr,addr,7);
		addr+=4096;
	}
	kprintf("done\r\n");*/
	ksbrk_page(1);
}
void *ksbrk_page(int n) {
	struct MallocHeader *chunk;
	uint32_t p_addr;
	if ((m_kheap + (n * PAGESIZE_4K)) > (char *)KERN_HEAP_END) {
		return (void *)-1;
	}
	chunk = (struct MallocHeader *)m_kheap;
	for (int i = 0; i < n ; i++) {
		p_addr = alloc_getPage();
		if (p_addr == (uint32_t)-1) {
			PANIC("ksbrk_page: No free space left");
			return (void *)-1;
		}
		arch_mmu_mapPage(NULL,(int)m_kheap,p_addr,7);
		m_kheap+= PAGESIZE_4K;
	}
	chunk->size = PAGESIZE_4K * n;
	chunk->used = 0;
	return chunk;
}
void *kmalloc(int size) {
	if (size == 0) {
		return NULL;
	}
	numAllocs++;
	unsigned long realsize;
	struct MallocHeader *chunk,*other;
	if ((realsize = sizeof(MallocHeader) + size) < KMALLOC_MINSIZE) {
		realsize = KMALLOC_MINSIZE;
	}
    	chunk = (struct MallocHeader *)KERN_HEAP_BEGIN;
	while(chunk->used || chunk->size < realsize) {
		if (chunk->size == 0) {
			kprintf("Num allocs: %d\n",numAllocs);
			kprintf("Last allocation size: %d\r\n",lastAlloc);
			kprintf("\nPANIC: kmalloc(): corrupted chunk on 0x%x with null size (heap 0x%x) !\nSystem halted\n", chunk, m_kheap);
			PANIC("Kernel heap corruption");
			return NULL;
		}
		chunk = (MallocHeader *)((char *)chunk + chunk->size);
		if (chunk == (struct MallocHeader *)m_kheap) {
			if ((int)(ksbrk_page(realsize/PAGESIZE_4K + 1)) < 0) {
               			 kprintf("Attempt to allocate %d bytes\r\n",size);
				//PANIC("Out of kernel memory!");
				return NULL;
			}
		} else if (chunk > (struct MallocHeader *)m_kheap) {
			kprintf("kmalloc: chunk is out of limit!\r\n");
			PANIC("Kernel heap limit");
			return NULL;
		}
	}
	if (chunk->size - realsize < KMALLOC_MINSIZE) {
		chunk->used = 1;
	} else {
		other = (struct MallocHeader *)((char *)chunk+realsize);
		other->size = chunk->size-realsize;
		other->used = 0;
		chunk->size = realsize;
		chunk->used = 1;
	}
	heap_used+=realsize;
	lastAlloc = size;
	return (void *) chunk + sizeof(struct MallocHeader);
}
void kfree(void *addr) {
    if (addr == (void *)-1) {
        return;
    }
    struct MallocHeader *chunk,*other;
    chunk = (struct MallocHeader *)((uint32_t)addr-sizeof(struct MallocHeader));
    //kprintf("kfree: address: 0x%x, chunk address: 0x%x\r\n",addr,chunk);    
    if (!chunk->used) {
        DEBUG("0x%x isn't allocated\r\n",addr);
        return;
    }
    chunk->used = 0;
    heap_used-=chunk->size;
    while ((other = (struct MallocHeader *)((char *)chunk + chunk->size))
           && other < (struct MallocHeader *)m_kheap
           && other->used == 0)
    {
        if (other->size == 0) {
            kprintf("kfree: null chunk while freeing! Chunk address 0x%x, heap: %d!\r\n",other,chunk->size);
            PANIC("Heap corruption!");
        }
        chunk->size += other->size;
    }
}
static void sbrk_page(process_t* process, int page_count)
{
    if (page_count > 0)
    {
        for (int i = 0; i < page_count; ++i)
        {
            if ((process->brk_next_unallocated_page_begin + PAGESIZE_4K) > (MEMORY_END - PAGESIZE_4K))
            {
                return;
            }

            uint32_t p_addr = alloc_getPage();

            if ((int)(p_addr) < 0)
            {
                //PANIC("sbrk_page(): no free page frame available !");
                return;
            }

            arch_mmu_mapPage(process->aspace,process->brk_next_unallocated_page_begin, p_addr, 0x00000004 | 0x00000200);

            process->brk_next_unallocated_page_begin += PAGESIZE_4K;
        }
    }
}

void *sbrk(process_t* process, int n_bytes)
{
    char* previous_break = (char *)process->brk_end;

    if (n_bytes > 0)
    {
        int remainingInThePage = process->brk_next_unallocated_page_begin - process->brk_end;

        if (n_bytes > remainingInThePage)
        {
            int bytesNeededInNewPages = n_bytes - remainingInThePage;
            int neededNewPageCount = ((bytesNeededInNewPages-1) / PAGESIZE_4K) + 1;

            sbrk_page(process, neededNewPageCount);
        }
    }
    process->brk_end += n_bytes;

    return previous_break;
}

void alloc_initProcess(process_t *process,int size) {
    process->brk_begin =  USER_OFFSET;
    process->brk_end = process->brk_begin;
    process->brk_next_unallocated_page_begin = process->brk_begin;
    sbrk(process, size);
}
int alloc_getUsedSize() {
    return heap_used;
}
void *krealloc(void *p,int size) {
    if(p == NULL) {
        return kmalloc(size);
    }
    struct MallocHeader *header = (struct MallocHeader *)(((char *)p) - sizeof (struct MallocHeader));
    void *newchunk = kmalloc(size);
    if(newchunk == NULL) return NULL; // WTF?

    memcpy(newchunk, p, header->size);
    kfree(p);
    return newchunk;
}
void alloc_freePage(int addr) {
	SET_PAGEFRAME_UNUSED(phys_map,addr);
}
int alloc_getBitmapSize() {
    return total_pages*sizeof(uint16_t);
}
void alloc_mapItself() {
    arch_mmu_map(arch_mmu_getAspace(),(int)phys_map,alloc_getBitmapSize(),7);
    mapped = true;
    int page = PAGE_INDEX_4K(kernel_endAddress+alloc_getBitmapSize());
    for (int i = page; i < pagesUsedBeforeMapping; i++) {
        SET_PAGEFRAME_USED(phys_map,i);
    }
}
int alloc_getEnd() {
    return kernel_endAddress+alloc_getBitmapSize();
}

void alloc_reserve(int start,int end) {
	// Generally used by MMU code
	for (int i = PAGE_INDEX_4K(start); i <  PAGE_INDEX_4K(end); i++) {
		SET_PAGEFRAME_USED(phys_map,i);
	}
}
