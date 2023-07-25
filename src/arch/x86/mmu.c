#include <arch/mmu.h>
#include <arch/x86/mmu.h>
#include <mm/alloc.h>
#include <arch.h>

// MMU implementation
/*
    This implementation is taken from SOSO OS: https://github.com/ozkl/soso
    I only ported it to my kernel design, check the origins for more information.
*/
static uint32_t *kernel_pg = (uint32_t *)KERN_PAGE_DIRECTORY;

static uint32_t read_cr3()
{
    uint32_t value;
    asm volatile("mov %%cr3, %0" : "=r" (value));

    return value;
}

extern char kernel_end[];

void arch_mmu_init() {
    int i = 0;
    for (i = 0; i < 4; ++i)
    {
	    // TODO: Remove PG_USER and fix all #PG after it
        kernel_pg[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB | PG_USER));//add PG_USER for accesing kernel code in user mode
	
    }
    for (i = 4; i < 1024; ++i)
    {
        kernel_pg[i] = 0;
    }
    //Recursive page directory strategy
    kernel_pg[1023] = (uint32_t)kernel_pg | PG_PRESENT | PG_WRITE;
    //zero out PD area
    memset((uint8_t*)KERN_PD_AREA_BEGIN, 0, KERN_PD_AREA_END - KERN_PD_AREA_BEGIN);
    asm("	mov %0, %%eax \n \
        mov %%eax, %%cr3 \n \
        mov %%cr4, %%eax \n \
        or %2, %%eax \n \
        mov %%eax, %%cr4 \n \
        mov %%cr0, %%eax \n \
        or %1, %%eax \n \
        mov %%eax, %%cr0"::"m"(kernel_pg), "i"(PAGING_FLAG), "i"(PSE_FLAG));
}


static void add_page_to_pd(void *v_addr,uint32_t p_addr,int flags) {
    int pd_index = (((uint32_t) v_addr) >> 22);
    int pt_index = (((uint32_t) v_addr) >> 12) & 0x03FF;

    uint32_t* pd = (uint32_t*)0xFFFFF000;

    uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);

    uint32_t cr3 = 0;

    if (v_addr < (void *)KERN_HEAP_END)
    {
        cr3 = read_cr3();

        CHANGE_PD(kernel_pg);
    }
    if ((pd[pd_index] & PG_PRESENT) != PG_PRESENT)
    {
        uint32_t tablePhysical = alloc_getPage();
        pd[pd_index] = (tablePhysical) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);
        INVALIDATE(v_addr);
        for (int i = 0; i < 1024; ++i)
        {
            pt[i] = 0;
        }
    }

    if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
    {
        if (0 != cr3)
        {
            CHANGE_PD(cr3);
        }

        return;
    }

    pt[pt_index] = (p_addr) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);
    INVALIDATE(v_addr);
    if (0 != cr3)
    {
        //restore
        CHANGE_PD(cr3);
    }
}

int arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {
    int s = PAGE_INDEX_4K(base);
    for (size_t i = 0; i < (size/PAGESIZE_4K)+1; i++) {
        uint32_t page_addr = (i+s)*PAGESIZE_4K;
        add_page_to_pd((void *)page_addr,page_addr,7);
    }
    return 0;
}
int arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {
    return 0;
}
int arch_mmu_query(aspace_t *aspace,vaddr_t vaddr,paddr_t *paddr,uint32_t *flags) {
    return 0;
}
void arch_mmu_switch(aspace_t *aspace) {
    CHANGE_PD(aspace);
}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {
    add_page_to_pd((void *)vaddr,paddr,flags);
}
void *arch_mmu_newAspace() {
    uint32_t address = KERN_PD_AREA_BEGIN;
    for(; address < KERN_PD_AREA_END; address += PAGESIZE_4K) {
        uint32_t *pd = (uint32_t *)address;
        if (*pd == NULL) {
            // We found new unused address space!
            for (int i = 0; i < 256; i++) {
                pd[i] = kernel_pg[i] & ~PG_OWNED;
            }
            for (int i = 256; i < 1024; i++) {
                pd[i] = 0;
            }
            pd[1023] = address | PG_PRESENT | PG_WRITE;
            return pd;
        }
    }
    return NULL;
}
aspace_t *arch_mmu_getAspace() {
    return (aspace_t *)read_cr3();
}
aspace_t *arch_mmu_getKernelSpace() {
    return kernel_pg;
}
void arch_mmu_destroyAspace(aspace_t *space) {

    uint32_t* pd = (uint32_t*)0xFFFFF000;

    uint32_t cr3 = read_cr3();

    CHANGE_PD(space);

    //this 1023 is very important
    //we must not touch pd[1023] since PD is mapped to itself. Otherwise we corrupt the whole system's memory.
    for (int pd_index = 256; pd_index < 1023; ++pd_index)
    {
        if ((pd[pd_index] & PG_PRESENT) == PG_PRESENT)
        {
            uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);

            for (int pt_index = 0; pt_index < 1024; ++pt_index)
            {
                if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
                {
                    if ((pt[pt_index] & PG_OWNED) == PG_OWNED)
                    {
                        uint32_t physicalFrame = pt[pt_index] & ~0xFFF;

                        alloc_freePage(physicalFrame);
                    }
                }
                pt[pt_index] = 0;
            }

            if ((pd[pd_index] & PG_OWNED) == PG_OWNED)
            {
                uint32_t physicalFramePT = pd[pd_index] & ~0xFFF;
                alloc_freePage(physicalFramePT);
            }
        }

        pd[pd_index] = 0;
    }
    //return to caller's Page Directory
    CHANGE_PD(cr3);
}
