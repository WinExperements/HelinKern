#include <arch/mmu.h>
#include <output.h>
#include <debug.h>
#include <arch.h>
#include <mm/alloc.h>
uint64_t vpn0[512] __attribute__((aligned(4096)));
uint64_t vpn1[512] __attribute__((aligned(4096)));
uint64_t vpn2[512] __attribute__((aligned(4096)));
#define PTE_V 0x1
#define PTE_R 0x2
#define PTE_W 0x4
#define PTE_X 0x8
#define PTE_U 0x10
#define PTE_G 0x20
#define PTE_A 0x40
#define PTE_D 0x80
extern char kernel_start[];
extern char kernel_end[];
static uint64_t kernel_vpn2,kernel_vpn1,kernel_vpn0;
static uint64_t kernel_satp;
uint64_t create_pte(uint64_t phys,bool read,bool write,bool exec,bool user) {
	/*uint64_t ppn2 = (phys >> 30) & 0xFFFFFFF;
	uint64_t ppn1 = (phys >> 21) & 0x1FF;
	uint64_t ppn0 = (phys >> 12) & 0x1FF;
	return create_pte_entry(ppn2,ppn1,ppn0,read,write,exec,user);*/
	uint64_t b = phys >> 12;
	uint64_t a = b << 10;
	a |= PTE_V;
	a |= (read ? PTE_R : 0);
	a |= (write ? PTE_W : 0);
	a |= (exec ? PTE_X : 0);
	a |= (user ? PTE_U : 0);
	return a;
}
uint64_t create_pte_with_flags(uint64_t phys,int flags) {
	uint64_t b = phys >> 12;
	uint64_t a = b << 10;
	return (a | flags);
}
void arch_mmu_init() {
	kprintf("rv64: mapping kernel from 0x%x to 0x%x\n",&kernel_start,&kernel_end);
	kprintf("rv64: recursive mapping enabled\n");
	//vpn0 = (uint64_t *)0x80228000000;
	uint64_t vaddr = (uint64_t)&kernel_start;
	uint64_t vpn2_indx = (vaddr >> 30) & 0x1FF;
	uint64_t vpn1_indx = (vaddr >> 21) & 0x1FF;
	uint64_t vpn0_indx = (vaddr >> 12) & 0x1FF;
	kernel_vpn0 = vpn0_indx;
	kernel_vpn1 = vpn1_indx;
	kernel_vpn2 = vpn2_indx;
	vpn2[vpn2_indx] = create_pte((uintptr_t)vpn1,false,false,false,false);
	vpn1[vpn1_indx] = create_pte((uintptr_t)vpn0,false,false,false,false);
	uint64_t endTo = (uint64_t)&kernel_end;
	for (; vaddr < endTo; vaddr += 4096) {
		vpn0_indx = (vaddr >> 12) & 0x1FF;
		vpn0[vpn0_indx] = create_pte(vaddr,true,true,true,false);
	}
	vpn0[508] = create_pte((uintptr_t)vpn0,true,true,false,false); // REAL.
	vpn0[509] = create_pte((uintptr_t)vpn2,true,true,false,false);
	vpn0[510] = create_pte((uintptr_t)vpn1,true,true,false,false);
	vpn0[511] = create_pte((uintptr_t)vpn0,true,true,false,false);
	kprintf("rv64: enabling MMU!\n");
	uint64_t ppn = ((uintptr_t)vpn2) >> 12;
	uint64_t satp = ((8ULL << 60) | ppn);
	kernel_satp = satp;
	asm volatile("csrw satp, %0" :: "r"(satp));
	asm volatile("sfence.vma");
	arch_mmu_mapPage(NULL, 0x10000000, 0x10000000,7);
}
size_t arch_mmu_map(aspace_t *aspace,vaddr_t base,size_t size,uint32_t flags) {return 0;}
size_t arch_mmu_unmap(aspace_t *aspace,vaddr_t vaddr,uint32_t count) {return 0;}
uintptr_t arch_mmu_query(aspace_t *aspace,vaddr_t start,size_t size) {return 0;}
void arch_mmu_switch(aspace_t *aspace) {
	asm volatile("csrw satp, %0" :: "r"((uintptr_t)aspace));
}
uint64_t sv39_convert(uint64_t val) {
	uint64_t ret = (val >> 10) << 12;
	return ret;
}
void arch_mmu_mapPage(aspace_t *aspace,vaddr_t vaddr,paddr_t paddr,uint32_t flags) {
	// Convert virtual address to the indexes.
	uint64_t vpn2_indx = (vaddr >> 30) & 0x1FF;
	uint64_t vpn1_indx = (vaddr >> 21) & 0x1FF;
	uint64_t vpn0_indx = (vaddr >> 12) & 0x1FF;
	uint64_t *tmp_vpn0 = (uint64_t *)0x803fc000;
	uint64_t *cur_vpn2 = (uint64_t *)0x803fd000;
	uint64_t *cur_vpn1 = (uint64_t *)0x803fe000;
	uint64_t *cur_vpn0 = (uint64_t *)0x803ff000;
	// Check if required page exists in each mapping level.
	if (!(cur_vpn2[vpn2_indx] & PTE_V)) {
		uint64_t newVpn1 = (uint64_t)alloc_getPage();
		cur_vpn2[vpn2_indx] = create_pte(newVpn1,false,false,false,false);
		// Map!
		tmp_vpn0[510] = create_pte(newVpn1,true,true,false,false);
		asm("sfence.vma");
		memset(cur_vpn1,0,4096);
	}
	tmp_vpn0[510] = create_pte(sv39_convert(cur_vpn2[vpn2_indx]),true,true,false,false);
	asm("sfence.vma");
	if (!(cur_vpn1[vpn1_indx] & PTE_V)) {
		uint64_t newVpn0 = (uint64_t)alloc_getPage();
		cur_vpn1[vpn1_indx] = create_pte(newVpn0,false,false,false,false);
		tmp_vpn0[511] = create_pte(newVpn0,true,true,false,false);
		asm("sfence.vma");
		memset(cur_vpn0,0,4096);
	}
	tmp_vpn0[511] = create_pte(sv39_convert(cur_vpn1[vpn1_indx]),true,true,false,false);
	asm("sfence.vma");
	// Map!
	cur_vpn0[vpn0_indx] = create_pte_with_flags(paddr,flags);
	asm volatile("sfence.vma"); // TLB flush
}
void *arch_mmu_newAspace() {return NULL;}
void arch_mmu_destroyAspace(aspace_t *aspace,bool all) {}
aspace_t *arch_mmu_getAspace() {
	uint64_t v = 0;
	asm("csrr %0, satp" : "=r"(v));
	return (aspace_t *)v;
}
aspace_t *arch_mmu_getKernelSpace() {return (aspace_t*)kernel_satp;}
void *arch_mmu_getPhysical(void *virtual) {
	return NULL;
}
bool arch_mmu_duplicate(aspace_t *parent, aspace_t *child) {
	return false;
}
int arch_mmu_processFlags(int flags) {
	int f = PTE_V;
	if ((flags & PROT_WRITE) == PROT_WRITE) {
		f |= PTE_W;
	}
	if ((flags & PROT_READ) == PROT_READ) {
		f |= PTE_R;
	}
	if ((flags & PROT_USER) == PROT_USER) {
		f |= PTE_U;
	}
	return f;
}
bool arch_mmu_createTables(vaddr_t from,vaddr_t to) {
	return false;
}

