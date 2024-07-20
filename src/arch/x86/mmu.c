#include <arch/mmu.h>
#include <arch/x86/mmu.h>
#include <mm/alloc.h>
#include <arch.h>
#include <output.h>
#if defined(__x86_64__)
#include <limine.h>
#include <lib/string.h>
#endif
// MMU implementation
/*
    This implementation is taken from SOSO OS: https://github.com/ozkl/soso
    I only ported it to my kernel design, check the origins for more information.
*/

/*
 * HelinKern modification.
 * Currently no modification to the init code applied. But as soon as possible we need to change the arch_mmu_init code
 * to add support for any size initrd, and don't limit it to 12MB
 * 29.06.2024: So, we back, and the 12MB limit bug seems to be fixed.
*/

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)
#if !defined(__x86_64__)
#define GET_VIRTUAL_ADDRESS(pd_index, pt_index, offset) \
    (((pd_index) << 22) | ((pt_index) << 12) | (offset))
#else
#define GET_VIRTUAL_ADDRESS(pdpIndex,pdIndex,ptIndex,pageIndex,offset) \
	(((uint64_t)pdpIndex << 39) | ((uint64_t)pdIndex << 30) | ((uint64_t)ptIndex << 21) | ((uint64_t)pageIndex << 12) | offset)
#endif
static int RESERVED_AREA = 0; //16 mb
static int KERN_PD_AREA_BEGIN = 0x0; //12 mb
static int KERN_PD_AREA_END = 0x0; 
#if defined(__x86_64__)
static uint64_t kernel_pdpt[512] __attribute__((aligned(4096))); 
static uint64_t kernel_pml4[512]__attribute__((aligned(4096)));
static uint64_t kernel_pg[512] __attribute__((aligned(4096)));
static uint64_t kernel_pt[512] __attribute__((aligned(4096)));
// Limine extension: 06.07.2024.
extern struct limine_hhdm_request hhdm_req;
extern struct limine_kernel_address_request krnaddr_req;
static void *limineAspace = NULL;
uint64_t krnVirtualOffset = 0;
#else
static uint32_t *kernel_pg = (uint32_t *)KERN_PAGE_DIRECTORY;
#endif
extern void copy_page_physical(uintptr_t src,uintptr_t dst);
static uintptr_t pml4addr;
static uint32_t read_cr3()
{
    uintptr_t value;
    asm volatile("mov %%cr3, %0" : "=r" (value));

    return value & 0xFFFFFFFF;
}

extern char kernel_end[];
#if defined(__x86_64__)
uintptr_t liminePhys(void *ptr) {
	uintptr_t addr = (uintptr_t)ptr;
	addr -= krnaddr_req.response->virtual_base;
	return krnaddr_req.response->physical_base+addr;
}
#endif
void arch_mmu_init() {
    // Reserve specific memory
    KERN_PD_AREA_BEGIN = 0x00F00000;
    KERN_PD_AREA_END = 0x0100000A;
    RESERVED_AREA = arch_getKernelEnd();
    alloc_reserve(0,RESERVED_AREA);
    if (arch_getKernelEnd() > RESERVED_AREA) {
	    PANIC("Initrd is overwriting our page tables!");
	}
#if defined(__x86_64__)
    // Currently map the kernel code(No FB sorry)
    limineAspace = (void *)read_cr3();
    krnVirtualOffset = hhdm_req.response->offset;
    memset(kernel_pdpt,0,4096);
    memset(kernel_pml4,0,4096);
    memset(kernel_pg,0,4096);
    memset(kernel_pt,0,4096);
    uintptr_t we = krnaddr_req.response->virtual_base;
    uintptr_t pdptIndex = (we >> 39) & 0x1ff;
    uintptr_t pdIndex = (we >> 30) & 0x1ff;
    uintptr_t ptIndex = (we >> 21) & 0x1ff;
    uintptr_t pageIndex = (we >> 12) & 0x1ff;
    kernel_pml4[pdptIndex] = (liminePhys(kernel_pdpt) | 7);
    kernel_pml4[510] = (liminePhys(kernel_pml4) | 3);
    kernel_pdpt[pdIndex] = (liminePhys(kernel_pg) | 7);
    kernel_pdpt[511] = (liminePhys(kernel_pdpt) | 3);
    kernel_pg[ptIndex] = (liminePhys(kernel_pt) | 7);
    kernel_pg[511] = (liminePhys(kernel_pg) | 3);
    //kernel_pdpt[511] = ((uint64_t)kernel_pdpt | 3);
    // Now we can potentially switch to the PML4.
    int size = arch_getKernelEnd() - krnaddr_req.response->physical_base;
    uint64_t physStart = krnaddr_req.response->physical_base;
    for (int i = 0; i < PAGE_INDEX_4K(size)+16; i++) {
	    uint64_t addr = physStart + (i*PAGESIZE_4K);
	    kernel_pt[i] = (addr | 7);
	}
    // Kernel PML4 are virtually located at invalid for CR3 address.
    pml4addr = liminePhys(kernel_pml4);
    asm("mov %0, %%cr3" : : "r"(pml4addr));
#else
    int i = 0;
    int end_index = PAGE_INDEX_4M(RESERVED_AREA)+4;
    if (end_index == 0) end_index = 4;
    for (i = 0; i < end_index; ++i)
    {
	    // TODO: Remove PG_USER and fix all #PG after it
        kernel_pg[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB));//add PG_USER for accesing kernel code in user mode
	
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
#endif
}
static void add_page_to_pd(void *v_addr,paddr_t p_addr,int flags) {
#if !defined(__x86_64__)	
    int pd_index = (((uintptr_t) v_addr & 0xFFFFFFFF) >> 22);
    int pt_index = (((uintptr_t) v_addr & 0xFFFFFFFF) >> 12) & 0x03FF;

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
#else
    // get indexes.
    vaddr_t vaddr = (vaddr_t)v_addr;
    vaddr_t indv = (vaddr_t)v_addr;
    indv >>= 12;
    int pageIndex = indv & 0x1ff;
    indv >>= 9;
    int ptIndex = indv & 0x1ff;
    indv >>= 9;
    int pdIndex = indv & 0x1ff;
    indv >>= 9;
    uint64_t pdpIndex = indv & 0x1ff;
    //kprintf("pdp -> %d, pd -> %d, pt -> %d, page -> %d\r\n",pdpIndex,pdIndex,ptIndex,pageIndex);
    // Now actually map.
    // PML4
    uint64_t *pml4 = (uint64_t *) 0xffffff7fbfdfe000; // we use recursive strategy, so this is our active PML4 address.
    // We gonna use recursive mapping tecnique.
    // we gonna use address pointers, because the pointer math is broken.
    // We need to allocate the pdpt too, but not today(okay, fine, today)
    uint64_t pdpAddr = 0xffffff7fbfc00000 + (0x1000 * pdpIndex);
    uint64_t pdAddr = 0xffffff7f80000000 + (0x200000 * pdpIndex) + (0x1000 * pdIndex);
    uint64_t ptAddr = 0xffffff0000000000;
    uint64_t ptOffset = (0x40000000 * pdpIndex);
    ptAddr += ptOffset;
    ptAddr += (0x200000 * pdIndex);
    ptAddr += (0x1000 * ptIndex);
    int orig_flags = flags;
    if (flags > 7) {
	    flags = 7;
	}
    uint8_t prev_flags = 0;
    if ((pml4[pdpIndex] & PG_PRESENT) != PG_PRESENT) {
	    uintptr_t newPdpt = (uintptr_t)alloc_getPage();
	    pml4[pdpIndex] = (newPdpt | flags);
	    INVALIDATE(v_addr);
	    // Now we can modify this shit 😁.
	    memset((void *)pdpAddr,0,4096);
	    uint64_t *pdpt = (uint64_t *)pdpAddr;
	    pdpt[511] = ((uint64_t)newPdpt | 3);
	} else {
		prev_flags = (uint8_t)pml4[pdpIndex] & 0xf;
		if (prev_flags < flags) {
			// Remap this table.
			uint64_t oldPdpPhys = pml4[pdpIndex];
			oldPdpPhys &= ~0xfff; // Remove the 12 bit flags, to get clean physical address.
			pml4[pdpIndex] = (oldPdpPhys | flags);
		}
	}
    uint64_t *pdp = (uint64_t *)pdpAddr;
    uint64_t *pd = (uint64_t *)pdAddr;
    uint64_t *pt = (uint64_t *)ptAddr;
    if ((pdp[pdIndex] & PG_PRESENT) != PG_PRESENT) {
	    // Allocate Page Directory Then.
	    uint64_t *newpd = (uint64_t *)alloc_getPage();
	    pdp[pdIndex] = ((uint64_t)newpd | flags);
	    INVALIDATE(v_addr);
	    memset(pd,0,4096);
	    pd[511] = ((uint64_t)newpd | 3);
	} else {
		// Check if the flags doesn't greater.
		prev_flags = (uint8_t)pdp[pdIndex] & 0xf;
		if (prev_flags < flags) {
			uint64_t oldPdPhys = pdp[pdIndex];
			oldPdPhys &= ~0xfff;
			pdp[pdIndex] = (oldPdPhys | flags);
		}
	}
    if ((pd[ptIndex] & PG_PRESENT) != PG_PRESENT) {
	    uint64_t *newpt = (uint64_t *)alloc_getPage();
	    pd[ptIndex] = ((uint64_t)newpt | flags);
	    pd[511] = ((uint64_t)pd | 3);
	    INVALIDATE(v_addr);
	    memset(pt,0,4096);
	} else {
		prev_flags = (uint8_t)pd[ptIndex] & 0xf;
		if (prev_flags < flags) {
			// Remap.
			uint64_t oldPtPhys = pd[ptIndex];
			oldPtPhys & ~0xfff;
			pd[ptIndex] = (oldPtPhys | flags);
		}
	}
    pt[pageIndex] = (p_addr) | orig_flags;
    asm("" ::: "memory");
    INVALIDATE(v_addr);
    //CHANGE_PD(kernel_pml4);
#endif
}
static void remove_page(void *v_addr) {
#if defined(__x86_64__)
	vaddr_t vaddr = (vaddr_t)v_addr;
    	vaddr_t indv = (vaddr_t)v_addr;
    	indv >>= 12;
    	int pageIndex = indv & 0x1ff;
    	indv >>= 9;
    	int ptIndex = indv & 0x1ff;
    	indv >>= 9;
	int pdIndex = indv & 0x1ff;
    	indv >>= 9;
    	uint64_t pdpIndex = indv & 0x1ff;
    	uint64_t *pml4 = (uint64_t *) 0xffffff7fbfdfe000; // we use recursive strategy, so this is our active PML4 address.
    	uint64_t pdpAddr = 0xffffff7fbfc00000 + (0x1000 * pdpIndex);
    	uint64_t pdAddr = 0xffffff7f80000000 + (0x200000 * pdpIndex) + (0x1000 * pdIndex);
    	uint64_t ptAddr = 0xffffff0000000000;
    	uint64_t ptOffset = (0x40000000 * pdpIndex);
	ptAddr += ptOffset;
    	ptAddr += (0x200000 * pdIndex);
    	ptAddr += (0x1000 * ptIndex);
	uint64_t *pdp = (uint64_t *)pdpAddr;
    	uint64_t *pd = (uint64_t *)pdAddr;
    	uint64_t *pt = (uint64_t *)ptAddr;
	if ((pml4[pdpIndex] & PG_PRESENT) != PG_PRESENT) return;
	if ((pdp[pdIndex] & PG_PRESENT) != PG_PRESENT) return;
	if ((pd[ptIndex] & PG_PRESENT) != PG_PRESENT) return;
	pt[pageIndex] = 0;
	INVALIDATE(v_addr);
#else
	uint32_t vaddr = ((uint32_t)(uintptr_t)v_addr & 0xFFFFFFFF);
	int pd_index =  PAGE_DIRECTORY_INDEX(vaddr);
	int pt_index = PAGE_TABLE_INDEX(vaddr);
	int *pd = (int *)0xFFFFF000;
	uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);
	// Check if we have the address mapped?
	// Sanity checks.
	if (pd_index < 0 || pd_index >= 1024) return;
	if (pt_index < 0 || pt_index >= 1024) return;
	if (pd[pd_index] == 0) {
		// We don't map this area?
		return;
	}
	pt[pt_index] = 0;
	INVALIDATE(v_addr);
#endif
}
size_t arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {
#if defined(__x86_64__)
	return size;
#else
    int s = PAGE_INDEX_4K(base);
    for (size_t i = 0; i < (size/PAGESIZE_4K)+1; i++) {
        // Bro WTF?
            add_page_to_pd((void *)base+(i*4096),alloc_getPage(),flags);
    }
    return 1;
#endif
}
size_t arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t pageCount) {
	// arch_mmu_unmap - just like actual map, but we will just clean the page pointer.
	for (int i = 0; i < pageCount; i++) {
		remove_page((void *)vaddr);
		vaddr+=4096;
	}
	return 1;
}
/*
 * Return virtual address in which required size can fit.
*/
int arch_mmu_query(aspace_t *aspace,vaddr_t start,uintptr_t size) {
	int pd_index = PAGE_DIRECTORY_INDEX(start);
	int pt_index = PAGE_TABLE_INDEX(start);
	int sizeInPages = size / 4096;
	int *pd = (int *)0xFFFFF000;
	int foundedPages = 0;
	uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);
	for (; pd_index <= 1024; pd_index++) {
		if (pd[pd_index] == 0) {
			// Page directory doesn't allocated.
			kprintf("%s: page directory doesn't allocated, so we found it at 0x%x\r\n",__func__,start);
			return start;
		}
		for (; pt_index < 1024; pt_index++) {
			if (pt[pt_index] == 0) {
				foundedPages++;
			} else {
				// Reset.
				foundedPages = 0;
			}
			start+=4096;
			if (foundedPages == sizeInPages) {
				kprintf("FOUND! At 0x%x!\r\n",start);
				return start;
			}
		}
		pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);
		pt_index = 0;
	}
	return -1;
}
void arch_mmu_switch(aspace_t *aspace) {
    CHANGE_PD(aspace);
}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {
    add_page_to_pd((void *)vaddr,paddr,flags);
}
void *arch_mmu_newAspace() {
#if defined(__x86_64__)
	// Well, on x86_64 we need to think different.
	// Allocate PML4, PDPT and PG.
	uint64_t *newPml4 = (uint64_t *)KERN_PD_AREA_BEGIN;
	uint64_t *newPdpt = (uint64_t *)KERN_PD_AREA_BEGIN + 4096;
	uint64_t *newPd = (uint64_t *)KERN_PD_AREA_BEGIN + 8192;
	uint64_t copyAddr = 0xffffff7f80000000 + (0x200000 * 0) + (0x1000 * 0);
	uint64_t *copyPd = (uint64_t *)copyAddr;
	uintptr_t newPml4Addr = alloc_getPage();
	if (newPml4Addr == -1) return NULL;
	uintptr_t newPdptAddr = alloc_getPage();
	if (newPdptAddr == -1) return NULL;
	uintptr_t newPdAddr = alloc_getPage();
	if (newPdAddr == -1) return NULL;
	// Map it, because we don't have access to this area.
	arch_mmu_mapPage(NULL,(uint64_t)newPml4,newPml4Addr,3);
	arch_mmu_mapPage(NULL,(uint64_t)newPdpt,newPdptAddr,3);
	arch_mmu_mapPage(NULL,(uint64_t)newPd,newPdAddr,3);
	// Now by theory we have access to this regions.
	memset(newPml4,0,4096);
	memset(newPdpt,0,4096);
	memset(newPd,0,4096);
	// Recursive strategy.
	newPml4[0] = (newPdptAddr | (PG_PRESENT | PG_WRITE | PG_USER));
	newPml4[510] = (newPml4Addr | 3);
	newPdpt[0] = (newPdAddr | 7);
	newPdpt[511] = (newPdptAddr | 3);
	// Clone the kernels page directories.
	newPml4[511] = kernel_pml4[511];
	for (int i = 0; i < 20; i++) {
		newPd[i] = copyPd[i];
	}
	newPd[511] = (newPdAddr | 3);
	return (void *)newPml4Addr;
#else
    uintptr_t address = KERN_PD_AREA_BEGIN;
    for(; address < KERN_PD_AREA_END; address += PAGESIZE_4K) {
        uintptr_t *pd = (uintptr_t *)address;
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
#endif
}
aspace_t *arch_mmu_getAspace() {
    return (aspace_t *)read_cr3();
}
aspace_t *arch_mmu_getKernelSpace() {
#if defined (__x86_64__)
	return (aspace_t*)pml4addr;
#else
    return kernel_pg;
#endif
}
/* Copy-On-Write modification */
void arch_mmu_destroyAspace(aspace_t *space, bool all) {
#if !defined(__x86_64__)

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
#else
    	uint64_t curCr3 = read_cr3();
	CHANGE_PD(space);
    	uint64_t *pml4 = (uint64_t *) 0xffffff7fbfdfe000;
	uint64_t pdpAddr = 0xffffff7fbfc00000;
    	uint64_t pdAddr = 0xffffff7f80000000;
    	uint64_t ptAddr = 0xffffff0000000000;
	uint64_t *pdpt = (uint64_t *)pdpAddr;
	uint64_t *pd = (uint64_t *)pdAddr;
	uint64_t *pt = (uint64_t *)ptAddr;
	uint64_t pdptPhys = pml4[0];
	pdptPhys &= ~0xfff;
	uint64_t pdPhys = pdpt[0];
	pdPhys &= ~0xfff;
	for (int pdpIndex = 0; pdpIndex < 7; pdpIndex++) {
		pdpAddr = 0xffffff7fbfc00000 + (0x1000 * pdpIndex);
		pdpt = (uint64_t *)pdpAddr;
		if (pml4[pdpIndex] == 0) continue;	// nothing here
		for (int pdIndex = 0; pdIndex < 510; pdIndex++) {
			if ((pdpt[pdIndex] & PG_PRESENT) != PG_PRESENT) continue;
			pdAddr = 0xffffff7f80000000 + (0x200000 * pdpIndex) + (0x1000 * pdIndex);
			pd = (uint64_t*)pdAddr;
			for (int ptIndex = 0; ptIndex < 510; ptIndex++) {
				if ((pd[ptIndex] & PG_PRESENT) != PG_PRESENT) continue;
				uint64_t ptOffset = 0x40000000 * pdpIndex;
				ptAddr = 0xffffff0000000000;
    				ptAddr += ptOffset;
    				ptAddr += (0x200000 * pdIndex);
    				ptAddr += (0x1000 * ptIndex);
				pt = (uint64_t*)ptAddr;
				for (int pgIndex = 0; pgIndex < 511; pgIndex++) {
					if (((pt[pgIndex] & PG_OWNED) == PG_OWNED)) {
						if (!all) {
							if ((pt[pgIndex] & PG_WRITE) != PG_WRITE) {
								continue;
							}
						}
						// Free this page.
						uint64_t phys = pt[pgIndex];
						phys &= ~0xfff;
						alloc_freePage(phys);
						pt[pgIndex] = 0;
					}
				}	
			}
		}
	}
	// Destroy the pdpt and pd at index 0.
	alloc_freePage(pdptPhys);
	alloc_freePage(pdPhys);
	CHANGE_PD(curCr3);
#endif
}

void *arch_mmu_getPhysical(void *virtual) {
#if defined(__x86_64__)
	uintptr_t vaddr = (uintptr_t)virtual;
	uintptr_t offset = vaddr % 4096;
	// Retrive indexes.
	vaddr_t indv = (vaddr_t)virtual;
    	indv >>= 12;
    	int pageIndex = indv & 0x1ff;
    	indv >>= 9;
    	int ptIndex = indv & 0x1ff;
    	indv >>= 9;
    	int pdIndex = indv & 0x1ff;
    	indv >>= 9;
    	uint64_t pdpIndex = indv & 0x1ff;
	// Now define the addresses of this elements.
	// It simular to arch_mmu_mapPage
	uint64_t *pml4 = (uint64_t *) 0xffffff7fbfdfe000;
	uint64_t pdpAddr = 0xffffff7fbfc00000 + (0x1000 * pdpIndex);
    	uint64_t pdAddr = 0xffffff7f80000000 + (0x200000 * pdpIndex) + (0x1000 * pdIndex);
    	uint64_t ptAddr = 0xffffff0000000000;
    	uint64_t ptOffset = (0x40000000 * pdpIndex);
    	ptAddr += ptOffset;
    	ptAddr += (0x200000 * pdIndex);
    	ptAddr += (0x1000 * ptIndex);
	uint64_t *pdp = (uint64_t *)pdpAddr;
    	uint64_t *pd = (uint64_t *)pdAddr;
    	uint64_t *pt = (uint64_t *)ptAddr;
	// Check if the elements even exist.
	if ((pml4[pdpIndex] & PG_PRESENT) != PG_PRESENT) {
		return NULL;
	}
	if ((pdp[pdIndex] & PG_PRESENT) != PG_PRESENT) {
		return NULL;
	}
	if ((pd[ptIndex] & PG_PRESENT) != PG_PRESENT) {
		return NULL;
	}
	if (pt[pageIndex] == 0) {
		return NULL;
	}
	uint64_t phys = pt[pageIndex];
	phys &= ~0xfff;
	phys += offset;
	return (void *)phys;
#else
    int vaddr = ((int)(uintptr_t)virtual & 0xFFFFFFFF);
    uint32_t cr3 = 0;

    /*if (vaddr < (int)KERN_HEAP_END) {
        cr3 = read_cr3();
        CHANGE_PD(kernel_pg);
    }*/

    int *ptable = (int *)0xFFFFF000;
    int index = PAGE_DIRECTORY_INDEX(vaddr);
    int page_index = PAGE_TABLE_INDEX(vaddr);

    if (index >= 1024 || page_index >= 1024)
        return NULL; // Invalid virtual address

    if (ptable[index] == 0) {
	    return NULL;
	}
    int *table = ((uint32_t*)0xFFC00000) + (0x400 * index);
    int physAddr = table[page_index] & ~0xfff;
    int align = vaddr & 0xfff;

    if (cr3 != 0) {
        CHANGE_PD(cr3);
    }

    return (void *)(physAddr + align);
#endif
}

/*
 * Actually required by fork
 * Copy-On-Write implementation
 *
 * Return true if success or zero if failed
*/

bool arch_mmu_duplicate(aspace_t *parent, aspace_t *child) {
#if !defined(__x86_64__)
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
#else
	// x86_64 modification.
	uintptr_t orig = read_cr3();
	// Now go throuth the tables.
	CHANGE_PD(parent);
	uint64_t *pml4 = (uint64_t *) 0xffffff7fbfdfe000;
	uint64_t pdpAddr = 0xffffff7fbfc00000;
    	uint64_t pdAddr = 0xffffff7f80000000;
    	uint64_t ptAddr = 0xffffff0000000000;
	uint64_t *pdpt = (uint64_t *)pdpAddr;
	uint64_t *pd = (uint64_t *)pdAddr;
	uint64_t *pt = (uint64_t *)ptAddr;
	for (int pdpIndex = 0; pdpIndex < 7; pdpIndex++) {
		pdpAddr = 0xffffff7fbfc00000 + (0x1000 * pdpIndex);
		pdpt = (uint64_t *)pdpAddr;
		if (pml4[pdpIndex] == 0) continue;	// nothing here
		for (int pdIndex = 0; pdIndex < 510; pdIndex++) {
			if ((pdpt[pdIndex] & PG_PRESENT) != PG_PRESENT) continue;
			pdAddr = 0xffffff7f80000000 + (0x200000 * pdpIndex) + (0x1000 * pdIndex);
			pd = (uint64_t*)pdAddr;
			for (int ptIndex = 0; ptIndex < 510; ptIndex++) {
				if ((pd[ptIndex] & PG_PRESENT) != PG_PRESENT) continue;
				uint64_t ptOffset = 0x40000000 * pdpIndex;
				ptAddr = 0xffffff0000000000;
    				ptAddr += ptOffset;
    				ptAddr += (0x200000 * pdIndex);
    				ptAddr += (0x1000 * ptIndex);
				pt = (uint64_t*)ptAddr;
				for (int pgIndex = 0; pgIndex < 511; pgIndex++) {
					if ((pt[pgIndex] & PG_OWNED) == PG_OWNED) {
						if ((pt[pgIndex] & PG_USER) != PG_USER) continue;
						// remove write permission in owner table.
						uint64_t phys = pt[pgIndex];
						phys &= ~0xfff;
						pt[pgIndex] = (phys | (PG_PRESENT | PG_USER | PG_OWNED));
						CHANGE_PD(child);
						uint64_t virtualAddress = (uint64_t)GET_VIRTUAL_ADDRESS(pdpIndex,pdIndex,ptIndex,pgIndex,0);
						arch_mmu_mapPage(NULL,virtualAddress,phys,(PG_PRESENT | PG_USER | PG_OWNED));
						CHANGE_PD(parent);
						//kprintf("Cloned 0x%x\r\n",virtualAddress);
						//PANIC("AAAAAA");
					}
				}	
			}
		}
}
	if (orig != NULL) {
		CHANGE_PD(orig);
	}
	return true;
#endif
}
#if defined(__x86_64__)
extern char stack_top;
extern void copy_page_physical_asm(uintptr_t src,uintptr_t dst);
void copy_page_physical(uintptr_t src,uintptr_t dest) {
	// Map this shit.
	// read current RSP?
	uint64_t offset = krnVirtualOffset;
	uintptr_t srcOf = src + offset;
	uintptr_t destOf = dest + offset;
	arch_mmu_mapPage(NULL,srcOf,src,3);
	arch_mmu_mapPage(NULL,destOf,dest,3);
//	copy_page_physical_asm((uintptr_t)srcPtr,(uintptr_t)dstPtr);
	memcpy((void *)destOf,(void *)srcOf,4096);
	remove_page((void *)destOf);
	remove_page((void *)srcOf);
}
#endif
