#ifndef X86_MMU_H
#define X86_MMU_H
#include <typedefs.h>

#define CHANGE_PD(pd) asm("mov %0, %%eax ;mov %%eax, %%cr3":: "m"(pd))
#define INVALIDATE(v_addr) asm("invlpg %0"::"m"(v_addr))

#define	KERN_PAGE_DIRECTORY			0x00001000

//16M is identity mapped as below.
//First 12M we don't touch. Kernel code and runtime initrd are there.
//4M is reserved for 4K page directories.
#define RESERVED_AREA           0x01000000 //16 mb
#define KERN_PD_AREA_BEGIN      0x00F00000 //12 mb
#define KERN_PD_AREA_END        0x0100000A //16 mb

#define	PAGING_FLAG 		0x80000000	// CR0 - bit 31
#define PSE_FLAG			0x00000010	// CR4 - bit 4 //For 4M page support.
#define PG_PRESENT			0x00000001	// page directory / table
#define PG_WRITE			0x00000002
#define PG_USER				0x00000004
#define PG_4MB				0x00000080
#define PG_OWNED			0x00000200  // We use 9th bit for bookkeeping of owned pages (9-11th bits are available for OS)
#define	RAM_AS_4K_PAGES		0x100000
#define	RAM_AS_4M_PAGES		1024
#define PAGE_COUNT(bytes)       (((bytes-1) / PAGESIZE_4K) + 1)

#endif
