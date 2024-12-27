#include <arch/mmu.h>
#include <output.h>
#include <debug.h>
#include <arch/armv7-a/mmu.h>
#include <arch.h>

// Doesn't work!

pde_t kernel_pd[1024] __attribute__((aligned(4096)));

#define CR_M (1 << 0)    /* Enable MMU */
#define CR_C (1 << 2)    /* Data cache */
#define CR_Z (1 << 11)   /* Branch Predict */
#define CR_I (1 << 12)   /* Instruction cache */
#define CR_TRE (1 << 28) /* TRE: TEX Remap*/
#define CR_AFE (1 << 29) /* AFE: Access Flag enable */

// Not mine code! I use it because i new in ARM!!!
// This code taken from: https://github.com/DavidDiPaola/armv7_bootstrap/blob/master/04_mmu/main.c

void arch_mmu_init() {
    while(1) {}
}
size_t arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {return 0;}
size_t arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {return 0;}
uintptr_t arch_mmu_query(aspace_t *aspace,vaddr_t start,size_t size) {return 0;}
void arch_mmu_switch(aspace_t *aspace) {
    uint32_t bar = (uint32_t)aspace;
    asm volatile("mcr p15, 0, %[bar], c2, c0, 0 @ Write TTBR0\n\t"
                 :
                 : [ bar ] "r"(bar));
}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {

}
void *arch_mmu_newAspace() {return NULL;}
void arch_mmu_destroyAspace(aspace_t *aspace,bool all) {}
aspace_t *arch_mmu_getAspace() {return 0;}
aspace_t *arch_mmu_getKernelSpace() {return 0;}
void *arch_mmu_getPhysical(void *virtual) {
	return NULL;
}
bool arch_mmu_duplicate(aspace_t *parent, aspace_t *child) {
	return false;
}
int arch_mmu_processFlags(int flags) {
	return 0;
}
bool arch_mmu_createTables(vaddr_t from,vaddr_t to) {
	return false;
}

