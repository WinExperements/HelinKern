#include <arch/mmu.h>
#include <arch/x86/mmu.h>
#include <mm/alloc.h>
#include <arch.h>
#include <output.h>

// MMU implementation
/*
    This implementation is taken from SOSO OS: https://github.com/ozkl/soso
    I only ported it to my kernel design, check the origins for more information.
*/

/*
 * HelinKern modification.
 * Currently no modification to the init code applied. But as soon as possible we need to change the arch_mmu_init code
 * to add support for any size initrd, and don't limit it to 12MB
*/

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

#define GET_VIRTUAL_ADDRESS(pd_index, pt_index, offset) \
    (((pd_index) << 22) | ((pt_index) << 12) | (offset))

static int RESERVED_AREA = 0; //16 mb
static int KERN_PD_AREA_BEGIN = 0x0; //12 mb
static int KERN_PD_AREA_END = 0x0; 
static uint32_t *kernel_pg = (uint32_t *)KERN_PAGE_DIRECTORY;
extern void copy_page_physical(int src,int dst);

static uint32_t read_cr3()
{
    uint32_t value;
    asm volatile("mov %%cr3, %0" : "=r" (value));

    return value;
}

extern char kernel_end[];

void arch_mmu_init() {
    // Reserve specific memory
    KERN_PD_AREA_BEGIN = 0x00F00000;
    KERN_PD_AREA_END = 0x0100000A;
    RESERVED_AREA = arch_getKernelEnd();
    alloc_reserve(0,RESERVED_AREA);
    kprintf("Reserved area: 0x%x, KERN_PD_AREA_BEGIN: 0x%x, KERN_PD_AREA_END: 0x%x\n",RESERVED_AREA,KERN_PD_AREA_BEGIN,KERN_PD_AREA_END);
    if (arch_getKernelEnd() > RESERVED_AREA) {
	    PANIC("Initrd is overwriting our page tables!");
	}
    int i = 0;
    int end_index = PAGE_INDEX_4M(RESERVED_AREA);
    if (end_index == 0) end_index = 4;
    for (i = 0; i < end_index; ++i)
    {
	    // TODO: Remove PG_USER and fix all #PG after it
        kernel_pg[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB | PG_USER));//add PG_USER for accesing kernel code in user mode
	
    }
    for (i = end_index; i < 1024; ++i)
    {
        kernel_pg[i] = 0;
    }
    //Recursive page directory strategy
    kernel_pg[1023] = (uint32_t)kernel_pg | PG_PRESENT | PG_WRITE;
    //zero out PD area
    kprintf("Zeroing our PD area...");
    memset((uint8_t*)KERN_PD_AREA_BEGIN, 0, KERN_PD_AREA_END - KERN_PD_AREA_BEGIN);
    kprintf("finished\r\n");
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

    /*if (v_addr < (void *)KERN_HEAP_END)
    {
        cr3 = read_cr3();

        CHANGE_PD(kernel_pg);
    }*/
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

    /*if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
    {
        if (0 != cr3)
        {
            CHANGE_PD(cr3);
        }

        return;
    }*/

    pt[pt_index] = (p_addr) | (flags & 0xFFF);
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
        // Bro WTF?
            add_page_to_pd((void *)base+(i*4096),alloc_getPage(),flags);
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
/* Copy-On-Write modification */
void arch_mmu_destroyAspace(aspace_t *space, bool all) {

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
                    bool allowed = (all ? true : ((pt[pt_index] & PG_WRITE) == PG_WRITE));
                    if ((pt[pt_index] & PG_OWNED) == PG_OWNED && allowed)
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

void *arch_mmu_getPhysical(void *virtual) {
    int vaddr = (int)virtual;
    uint32_t cr3 = 0;

    if (vaddr < (int)KERN_HEAP_END) {
        cr3 = read_cr3();
        CHANGE_PD(kernel_pg);
    }

    int *ptable = (int *)0xFFFFF000;
    int index = PAGE_DIRECTORY_INDEX(vaddr);
    int page_index = PAGE_TABLE_INDEX(vaddr);

    if (index >= 1024 || page_index >= 1024)
        return NULL; // Invalid virtual address

    if (ptable[index] == 0)
        return NULL; // Unmapped

    int *table = ((uint32_t*)0xFFC00000) + (0x400 * index);
    int physAddr = table[page_index] & ~0xfff;
    int align = vaddr & 0xfff;

    if (cr3 != 0) {
        CHANGE_PD(cr3);
    }

    return (void *)(physAddr + align);
}

/*
 * Actually required by fork
 * Copy-On-Write implementation
 *
 * Return true if success or zero if failed
*/

bool arch_mmu_duplicate(aspace_t *parent, aspace_t *child) {
        // Actually like arch_mmu_destroyAspace
        uint32_t* pd = (uint32_t*)0xFFFFF000;
        uint32_t cr3 = read_cr3();
        CHANGE_PD(parent);
	    int index = 256;
        for (int pd_index = index; pd_index < 1023; ++pd_index)
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
						                        uint32_t virtual = GET_VIRTUAL_ADDRESS(pd_index,pt_index,0);
						                        //uint32_t child_phys = alloc_getPage();
									            //copy_page_physical(physicalFrame,child_phys);
						                        //kprintf("%s: copy address 0x%x, 0x%x\n",__func__,virtual,physicalFrame);
						                        /* Copy-On-Write! */
						                        CHANGE_PD(child);
						                        arch_mmu_mapPage(NULL,virtual,physicalFrame,PG_PRESENT | PG_USER | PG_OWNED);
						                        CHANGE_PD(parent);
									            arch_mmu_mapPage(NULL,virtual,physicalFrame,PG_PRESENT | PG_USER | PG_OWNED);
                                        }
                                }
                        }
                }
        }
        // For any case, if we called from different address space
        CHANGE_PD(cr3);
        return true;
}
