#ifndef MMU_H
#define MMU_H
// Arch specific MMU declaration
typedef void aspace_t;
#include <typedefs.h>
void arch_mmu_init();
size_t arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags);
size_t arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count);
// Find usable virtual memory region.
int arch_mmu_query(aspace_t *aspace,vaddr_t start,size_t size);
void arch_mmu_switch(aspace_t *aspace);
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags);
void *arch_mmu_newAspace();
void arch_mmu_destroyAspace(aspace_t *aspace,bool all);
aspace_t *arch_mmu_getAspace();
aspace_t *arch_mmu_getKernelSpace();
/*
 * Get physical address of virtual address by it's pointer
*/
void *arch_mmu_getPhysical(void *virtual);
bool arch_mmu_duplicate(aspace_t *parent, aspace_t *child);
#endif
