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

#define arm_cp15_sync __asm("isb")

/* put CPU to sleep until an interrupt occurs */
#define arm_sleep __asm("wfi")

/* SCTLR (System Control Register) definitions (see: DDI0406C.D B4.1.130) */
#define __asm_mrc_SCTLR(var) __asm("mrc p15, 0, %[__reg], c1, c0, 0" : [__reg] "=r" (var))
#define __asm_mcr_SCTLR(var) __asm("mcr p15, 0, %[__reg], c1, c0, 0" : : [__reg] "r" (var))

/* TTBCR (Translation Table Base Control Register) definitions (see: DDI0406C.D B4.1.153) */
#define __asm_mrc_TTBCR(var) __asm("mrc p15, 0, %[__reg], c2, c0, 2" : [__reg] "=r" (var))
#define __asm_mcr_TTBCR(var) __asm("mcr p15, 0, %[__reg], c2, c0, 2" : : [__reg] "r" (var))

/* TTBR0 (Translation Table Base Register 0) 32-bit definitions (see: DDI0406C.D B4.1.154) */
#define __asm_mrc_TTBR0(var) __asm("mrc p15, 0, %[__reg], c2, c0, 0" : [__reg] "=r" (var))
#define __asm_mcr_TTBR0(var) __asm("mcr p15, 0, %[__reg], c2, c0, 0" : : [__reg] "r" (var))

/* DACR (Domain Access Control Register) definitions (see: DDI0406C.D B4.1.43) */
#define __asm_mrc_DACR(var) __asm("mrc p15, 0, %[__reg], c3, c0, 0" : [__reg] "=r" (var))
#define __asm_mcr_DACR(var) __asm("mcr p15, 0, %[__reg], c3, c0, 0" : : [__reg] "r" (var))

/* bit fields:
	ns (not secure):
	ng (not global):
	s (shareable):
	tex ():
	ap (access permissions):
		value    PL1 access    unprivileged access
		0b011    read,write    read,write
		(see DDI0406C.D table B3-8)
	domain ():
	xn (execute never): 0=can execute, 1=execution causes permission fault (see DDI0406C.D B3.7.2)
	c ():
	b ():
	pxn (privelaged execute never): 0=PL1 can execute, 1=execution causes permission fault (see DDI0406C.D B3.7.2)
*/

/* translation table level 1 entry macros (see: DDI0406C.D figure B3-4) */
#define _tt_l1_INVALID ((uint32_t)( 0b00 ))
#define _tt_l1_PAGETABLE(address, domain, sbz, ns, pxn) ((uint32_t)( \
	((address) & 0xFFFFFC00) | \
	(((domain)  &      0xF) <<  5) | \
	(((sbz)     &        1) <<  4) | \
	(((ns)      &        1) <<  3) | \
	(((pxn)     &        1) <<  2) | \
	(0b10                   <<  0)   \
))
#define _tt_l1_SECTION(address, ns, ng, s, tex, ap, domain, xn, c, b, pxn) ((uint32_t)( \
	((address) & 0xFFF00000)    | \
	(((ns)      &     1) << 19) | \
	(((ng)      &     1) << 17) | \
	(((s)       &     1) << 16) | \
	((((ap)>>2) &     1) << 15) | \
	(((tex)     &   0x7) << 12) | \
	(((ap)      &   0x3) << 10) | \
	(((domain)  &   0xF) <<  5) | \
	(((xn)      &     1) <<  4) | \
	(((c)       &     1) <<  3) | \
	(((b)       &     1) <<  2) | \
	(1                   <<  1) | \
	(((pxn)     &     1) <<  0)   \
))

/* translation table level 2 entry macros (see: DDI0406C.D figure B3-5) */
#define _tt_l2_entry_INVALID ((uint32_t)( 0b00 ))
#define _tt_l2_entry_PAGE_4K(address, ng, s, ap, tex, c, b, xn) ((uint32_t)( \
	(((address)   & 0xFFFFF) << 12) | \
	(((ng)        &       1) << 11) | \
	(((s)         &       1) << 10) | \
	((((ap) >> 2) &       1) <<  9) | \
	(((tex)       &     0x7) <<  6) | \
	(((ap)        &     0x3) <<  4) | \
	(((c)         &       1) <<  3) | \
	(((b)         &       1) <<  2) | \
	(1                       <<  1) | \
	(((xn)        &       1) <<  0)   \
))

#define _tt_l1_type uint32_t
#define _tt_l1_size (16384)
#define _tt_l1_length (_tt_l1_size / sizeof(_tt_l1_type))
static _tt_l1_type __attribute__((aligned(_tt_l1_size))) _tt_l1[_tt_l1_length];

static void
mmu_tt_l1_clear(void) {
	for (uint32_t i=0; i<_tt_l1_length; i++) {
		_tt_l1[i] = _tt_l1_INVALID;
	}
}

static uint32_t
mmu_tt_l1_mapsection(uint32_t physaddr, uint32_t virtaddr) {
	/* TODO add args to set mapping flags */
	virtaddr &= 0xFFF00000;
	uint32_t entry = _tt_l1_SECTION(
		/*address*/ physaddr,
		/*ns*/ 0,  /* use the secure state's address map */
		/*ng*/ 0,  /* global */
		/*s*/ 1,  /* ignored because this mapping is strongly-ordered */
		/*tex*/ 0b000,  /* strongly-ordered & shareable (see: DDI0406C.D table B3-10) */
		/*ap*/ 0b011,  /* read & write access (see: DDI0406C.D table B3-8) */
		/*domain*/ 0b0000,  /* which DACR bits to check */
		/*xn*/ 0,  /* execution allowed */
		/*c*/ 0,  /* strongly-ordered & shareable (see: DDI0406C.D table B3-10) */
		/*b*/ 0,  /* strongly-ordered & shareable (see: DDI0406C.D table B3-10) */
		/*pxn*/ 0  /* privelaged execution allowed */
	);
	uint32_t index = (virtaddr >> 20) & 0xFFF;
	_tt_l1[index] = entry;

	return virtaddr;
}

__attribute__ ((interrupt ("ABORT")))
void
_handler_abort(void) {
	kprintf("abort exception occurred!" "\r\n");
	for (;;) { arm_sleep; }
}

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
    uint32_t sctlr;
    int size = kernel_end-kernel_start;
    kprintf("ARMV7-A MMU: Kernel size %d bytes\r\n",size);
    kprintf("TT1 placed at: 0x%x\r\n",_tt_l1);
    kprintf("Disabling ARM MMU\r\n");
    __asm_mrc_SCTLR(sctlr);
    sctlr &= ~(1 << 0);
    __asm_mcr_SCTLR(sctlr);
    uint32_t ttbcr;
    __asm_mrc_TTBCR(ttbcr);
    ttbcr &= ~((1<<31) | 0b111);
    ttbcr |=   (0<<31  | 0b000);
    __asm_mcr_TTBCR(ttbcr);
    kprintf("Setting up TTBR0 address\r\n");
    uint32_t ttbr0;
__asm_mrc_TTBR0(ttbr0);
ttbr0 &= ~(0xFFFFC000);
ttbr0 |=  (((uint32_t)_tt_l1) & 0xFFFFC000);
__asm_mcr_TTBR0(ttbr0);

/* set all MMU domains as "client" (translation table permission bits are checked) (see DDI0406C.D B3.12.3) */
uint32_t dacr = 0b01010101010101010101010101010101;
__asm_mcr_DACR(dacr);

arm_cp15_sync;

/* clear level 1 translation table */
mmu_tt_l1_clear();
mmu_tt_l1_mapsection(0x80010000, 0x80010000);
//mmu_tt_l1_mapsection(0x1c090000,0x1c090000);
kprintf("Enabling MMU! This is very scary!\r\n");
enable_paging();
    kprintf("Did we still alive and doesn't get any of interrupts?\r\n");
    kprintf("Causing #PF, hihiha\r\n");
   // *(volatile uint32_t *)(0xF00FF00F) = 'c';
    while(1) {}
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
