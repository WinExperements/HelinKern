#ifndef MMU_H
#define MMU_H
// Arch specific MMU declaration
typedef void aspace_t;
#define PROT_NONE	0
#define PROT_READ	1
#define PROT_WRITE	2
#define PROT_USER	4
#define PROT_EXEC	8
#define PROT_OWNED	12
#include <typedefs.h>
void arch_mmu_init();
size_t arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags);
size_t arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count);
// Find usable virtual memory region.
uintptr_t arch_mmu_query(aspace_t *aspace,vaddr_t start,size_t size);
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
// Convert XSI mmap prot flags to arch specific MMU flags. Used to maybe optimize some syscalls?
int arch_mmu_processFlags(int flags);
bool arch_mmu_createTables(vaddr_t from,vaddr_t to);
/*
 * Get arch-specific mm i/o address.
 * Used because the drivers data like, mem i/o must be shared within all address spaces
 * so this function will help the driver map their HW data in correct virtual address.
*/
uintptr_t arch_mmu_get_io_addr(uintptr_t size);
uintptr_t arch_mmu_getPageSize();
#endif
