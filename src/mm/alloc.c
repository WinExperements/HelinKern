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
static unsigned long total_pages = 0;
static char *m_kheap = NULL;
static int heap_used;
static bool mapped = false;
static int pagesUsedBeforeMapping = 0;
static int kernel_endAddress;
static int end_address;
static int numAllocs = 0; // debug only!
static int lastAlloc = 0;
static unsigned long usedPhysMem = 0;
static uintptr_t kernelEndForBitmap = 0;
static uintptr_t notMappedPageIndx = 0;
static uintptr_t allocStart = 0;
void alloc_init(uintptr_t kernel_end,uintptr_t high_mem) {
    if (high_mem == 0) {
      kprintf("Headless platform detected, %s not possible\r\n",__func__);
      return;
    }
    kernelEndForBitmap = kernel_end;
    output_write(__func__);
    output_write(" kernel ");
    output_printHex(high_mem);
    output_write("\r\n");
    // Place map at the end of kernel address
    phys_map = (uint8_t *)arch_getMemStart();
    allocStart = arch_getMemStart();
    // Clean the map
    total_pages = (high_mem)/4096;
    total_pages *= 1024;
    //total_pages *= 1024;
    if (total_pages < 0) {
	    kprintf("Something went really WRONG. HIGH MEM(%d,0x0%x). MN: %d, DEL: %d\r\n",high_mem,high_mem,high_mem*1024,(high_mem*1024)/4096);
	    PANIC("WHAAHAHAHAH");
	}
    kernel_endAddress = (int)kernel_end;
    end_address = kernel_end+(total_pages*8);
    notMappedPageIndx = PAGE_INDEX_4K((uintptr_t)phys_map+(total_pages*8))+10;
    kprintf("Kernel end address: 0x%x, allocation start address: 0x%x\n",kernel_end,end_address);
}
paddr_t alloc_getPage() {
    if (!mapped) {
        pagesUsedBeforeMapping++;
        return (notMappedPageIndx+pagesUsedBeforeMapping) * PAGESIZE_4K; 
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
		    usedPhysMem+=4096;
                    return allocStart+(page * PAGESIZE_4K);
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
			kprintf("ksbrk_page: No free space left. alloc_getPage returns(new call): 0x%x",alloc_getPage());
			PANIC("A");
			return (void *)-1;
		}
		arch_mmu_mapPage(NULL,(size_t)m_kheap,p_addr,3);
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
	struct MallocHeader *chunk,*other,*prev;
	if ((realsize = sizeof(MallocHeader) + size) < KMALLOC_MINSIZE) {
		realsize = KMALLOC_MINSIZE;
	}
    	chunk = (struct MallocHeader *)KERN_HEAP_BEGIN;
	while(chunk->used || chunk->size < realsize) {
		if (chunk->size == 0) {
			kprintf("Num allocs: %d\n",numAllocs);
			kprintf("Last allocation size: %d\r\n",lastAlloc);
			kprintf("Kernel heap corruption when trying to allocate %d bytes, previous chunk size: %d\n",size,prev->size);
			kprintf("\nPANIC: kmalloc(): corrupted chunk on 0x%x with null size (heap 0x%x) !\nSystem halted\n", chunk, m_kheap);
			PANIC("Kernel heap corruption");
			return NULL;
		}
		prev = chunk;
		chunk = (MallocHeader *)((char *)chunk + chunk->size);
		if (chunk == (struct MallocHeader *)m_kheap) {
			if ((size_t)(ksbrk_page((realsize/PAGESIZE_4K) + 2)) < 0) {
               			 kprintf("Attempt to allocate %d bytes\r\n",size);
				//PANIC("Out of kernel memory!");
				return NULL;
			}
		} else if (chunk > (struct MallocHeader *)m_kheap) {
			kprintf("kmalloc: chunk is out of limit!(chunk at 0x%x, m_kheap at 0x%x)\r\n",chunk,m_kheap);
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
        	//DEBUG("Using other chunk, size: %d, allocating %d\n",other->size,size);
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
    chunk = (struct MallocHeader *)((size_t)addr-sizeof(struct MallocHeader));
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
            if ((uint32_t)(process->brk_next_unallocated_page_begin + PAGESIZE_4K) > (uint32_t)(MEMORY_END - PAGESIZE_4K))
            {
                kprintf("%s: warrning!! process->brk_next_unallocated_page_begin -> 0x%x, memory end + pg -> 0x%x!\r\n",__func__,process->brk_next_unallocated_page_begin + PAGESIZE_4K,MEMORY_END - PAGESIZE_4K);
                return;
            }
            uint32_t p_addr = alloc_getPage();

            if ((int)(p_addr) < 0)
            {
                //PANIC("sbrk_page(): no free page frame available !");
                return;
            }

            arch_mmu_mapPage(process->aspace,process->brk_next_unallocated_page_begin, p_addr, 7 | 0x00000200);

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
	addr-=allocStart;
	SET_PAGEFRAME_UNUSED(phys_map,addr);
	usedPhysMem-=4096;
}
int alloc_getBitmapSize() {
    return total_pages*sizeof(uint16_t);
}
void alloc_mapItself() {
	int pages = alloc_getBitmapSize() / 4096;
    for (int i = 0; i < (pages)+10; i++) {
	    vaddr_t pgAd = (vaddr_t)phys_map + (i*4096);
	    arch_mmu_mapPage(NULL,pgAd,pgAd,3);
	}
    /*
     * Why? Well, some bootloaders and protocols doesn't really give us access to the memory map
     * after boot, because it's isn't mapped whitin the bootloaders TLB buffers.
     */
    int pg = 0;
     for (pg = 0; pg < total_pages / 8; ++pg) {
        phys_map[pg] = 0x0;
    }
    int bitmapSize = total_pages*sizeof(uint16_t);
    for (pg = 0; pg < (int)(PAGE_INDEX_4K(bitmapSize)+10); ++pg) {
        SET_PAGEFRAME_USED(phys_map,pg);
    }
    mapped = true;
    int page = PAGE_INDEX_4K(kernel_endAddress+alloc_getBitmapSize());
    for (int i = notMappedPageIndx; i < pagesUsedBeforeMapping; i++) {
	    if (i >= total_pages) break;
        SET_PAGEFRAME_USED(phys_map,i);
    }
}
int alloc_getEnd() {
    return kernel_endAddress+alloc_getBitmapSize();
}

void alloc_reserve(int start,int end) {
	if (!mapped) return;
	// Generally used by MMU code
	for (int i = PAGE_INDEX_4K(start); i <  PAGE_INDEX_4K(end); i++) {
		SET_PAGEFRAME_USED(phys_map,i);
	}
}

unsigned long alloc_getUsedPhysMem() {
	return usedPhysMem;
}
unsigned long alloc_getAllMemory() {
	return total_pages*4096;
}
int alloc_alignAddress(int addr,int al) {
    al--;
    uintptr_t aligned_addr = (addr + al) & ~al;
    return aligned_addr;
}
