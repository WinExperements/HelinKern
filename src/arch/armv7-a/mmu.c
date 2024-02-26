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
extern char kernel_start[];
extern char kernel_end[];
void enable_paging()
{
    asm volatile("mcr p15, 0, %[bcr], c2, c0, 2 @ Write TTBCR\n\t"
                 :
                 : [ bcr ] "r"(0)); /* TTBCR = 0: always use TTBR0 */
    asm volatile("mcr p15, 0, %[dacr], c3, c0, 0 @ Write DACR\n\t"
                 :
                 : [ dacr ] "r"(0x55555555)); /* all client */

    uint32_t sctlr;
    asm volatile("mrc p15, 0, %[ctl], c1, c0, 0 @ Read SCTLR\n\t"
                 : [ ctl ] "=r"(sctlr));
    sctlr |= CR_M;
    sctlr |= CR_C;
    sctlr |= CR_Z;
    sctlr |= CR_I;
    sctlr &= ~CR_TRE;
    sctlr &= ~CR_AFE;
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 0 @ Write SCTLR\n\t"
                 :
                 : [ ctl ] "r"(sctlr));
}

// Currently not implemented
void arch_mmu_init() {
    // We need to map the kernel code then enable MMU chip using mcr
    // There code parts taken from LyOS: https://github.com/jimx-/lyos
    // Halt system currently
    // Just map whole kernel and enable MMU chip
    // Calculate kernel size
    int size = kernel_end-kernel_start;
    kprintf("ARMV7-A MMU: Kernel size %d bytes\r\n",size);
    kprintf("I protect you, my device from this hell\r\n");
    PANIC("Self protection");
}
int arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {return 0;}
int arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {return 0;}
int arch_mmu_query(aspace_t *aspace,vaddr_t vaddr,paddr_t *paddr,uint32_t *flags) {return 0;}
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
