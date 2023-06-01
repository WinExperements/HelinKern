#include <arch/mmu.h>
#include <output.h>
#include <debug.h>

void arch_mmu_init() {
    DEBUG_N("NOT IMPLEMENTED, HALT!\r\n");
    DEBUG("ARM kernel end address: 0x%x\r\n",arch_getKernelEnd());
    while(1) {}
}
int arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {return 0;}
int arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {return 0;}
int arch_mmu_query(aspace_t *aspace,vaddr_t vaddr,paddr_t *paddr,uint32_t *flags) {return 0;}
void arch_mmu_switch(aspace_t *aspace) {}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {}
void *arch_mmu_newAspace() {return NULL;}
void arch_mmu_destroyAspace(aspace_t *aspace) {}
aspace_t *arch_mmu_getAspace() {return 0;}
aspace_t *arch_mmu_getKernelSpace() {return 0;}
