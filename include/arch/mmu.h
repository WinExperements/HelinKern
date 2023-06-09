#ifndef MMU_H
#define MMU_H
// Arch specific MMU declaration
typedef void aspace_t;
#include <typedefs.h>
void arch_mmu_init();
int arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags);
int arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count);
int arch_mmu_query(aspace_t *aspace,vaddr_t vaddr,paddr_t *paddr,uint32_t *flags);
void arch_mmu_switch(aspace_t *aspace);
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags);
void *arch_mmu_newAspace();
void arch_mmu_destroyAspace(aspace_t *aspace);
aspace_t *arch_mmu_getAspace();
aspace_t *arch_mmu_getKernelSpace();
#endif
