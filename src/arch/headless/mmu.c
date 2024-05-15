#include <arch/mmu.h>
#include <output.h>
#include <debug.h>
#include <arch.h>
// Currently not implemented
void arch_mmu_init() {
    kprintf("Headless platform doesn't support any kind of MMU\r\n");
    kprintf("Stop, next part of kernel require MMU.\r\n");
    putc('a');
    while(1) {
  }
}
int arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {return 0;}
int arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {return 0;}
int arch_mmu_query(aspace_t *aspace,vaddr_t vaddr,paddr_t *paddr,uint32_t *flags) {return 0;}
void arch_mmu_switch(aspace_t *aspace) {}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {}
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
