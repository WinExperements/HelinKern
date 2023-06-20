// MMU code implementation taken from LyOS

#ifndef ARMV7_MMU
#define ARMV7_MMU

typedef unsigned int pde_t;
typedef unsigned int pte_t;

#define ARM_VM_DIR_ENTRIES 4096
#define ARM_VM_PT_ENTRIES 256

/* size of page directory */
#define ARM_PGD_SHIFT (22)
#define ARM_PGD_SIZE (1UL << ARM_PGD_SHIFT)
#define ARM_PGD_MASK (~(ARM_PGD_SIZE - 1))

/* size of a virtual page */
#define ARM_PG_SHIFT (12)
#define ARM_PG_SIZE (1UL << ARM_PG_SHIFT)
#define ARM_PG_MASK (~(ARM_PG_SIZE - 1))

/* struct page_directory */
typedef struct {
    /* physical address of page dir */
    pde_t* phys_addr;
    /* virtual address of page dir */
    pde_t* vir_addr;

    /* virtual address of all page tables */
    pte_t* vir_pts[ARM_VM_DIR_ENTRIES];
} pgdir_t;

#define ARM_PG_PRESENT (1 << 1)
#define ARM_PG_C (1 << 3)
#define ARM_PG_SUPER (0x1 << 4)
#define ARM_PG_USER (0x3 << 4)
#define ARM_PG_TEX0 (1 << 6)
#define ARM_PG_TEX1 (1 << 7)
#define ARM_PG_TEX2 (1 << 8)
#define ARM_PG_RO (1 << 9)
#define ARM_PG_RW 0
#define ARM_PG_CACHED (ARM_PG_TEX2 | ARM_PG_TEX0 | ARM_PG_C)

#define ARM_SECTION_SIZE (1024 * 1024) /* 1 MB section */
/* Big page (1MB section) specific flags. */
#define ARM_VM_SECTION (1 << 1)            /* 1MB section */
#define ARM_VM_SECTION_PRESENT (1 << 1)    /* Section is present */
#define ARM_VM_SECTION_B (1 << 2)          /* B Bit */
#define ARM_VM_SECTION_C (1 << 3)          /* C Bit */
#define ARM_VM_SECTION_DOMAIN (0xF << 5)   /* Domain Number */
#define ARM_VM_SECTION_SUPER (0x1 << 10)   /* Super access only AP[1:0] */
#define ARM_VM_SECTION_USER (0x3 << 10)    /* Super/User access AP[1:0] */
#define ARM_VM_SECTION_TEX0 (1 << 12)      /* TEX[0] */
#define ARM_VM_SECTION_TEX1 (1 << 13)      /* TEX[1] */
#define ARM_VM_SECTION_TEX2 (1 << 14)      /* TEX[2] */
#define ARM_VM_SECTION_RO (1 << 15)        /* Read only access AP[2] */
#define ARM_VM_SECTION_SHAREABLE (1 << 16) /* Shareable */
#define ARM_VM_SECTION_NOTGLOBAL (1 << 17) /* Not Global */
#define ARM_VM_SECTION_WTWB \
    (ARM_VM_SECTION_TEX2 | ARM_VM_SECTION_TEX0 | ARM_VM_SECTION_C)
#define ARM_VM_SECTION_CACHED ARM_VM_SECTION_WTWB

#define ARM_VM_SECTION_SHIFT 20
#define ARM_VM_SECTION_MASK (~((1 << ARM_VM_SECTION_SHIFT) - 1))

#define ARM_VM_PAGEDIR (1 << 0)      /* Page directory */
#define ARM_VM_PDE_PRESENT (1 << 0)  /* Page directory is present */
#define ARM_VM_PDE_DOMAIN (0xF << 5) /* Domain Number */
#define ARM_VM_PDE_SHIFT 10
#define ARM_VM_PDE_MASK (~((1 << ARM_VM_PDE_SHIFT) - 1))

#define ARM_VM_OFFSET_MASK_1MB 0x000FFFFF /* physical address */

#define ARCH_VM_DIR_ENTRIES ARM_VM_DIR_ENTRIES
#define ARCH_VM_PT_ENTRIES ARM_VM_PT_ENTRIES

#define ARCH_PG_PRESENT ARM_PG_PRESENT
#define ARCH_PG_USER ARM_PG_USER
#define ARCH_PG_RW ARM_PG_RW
#define ARCH_PG_RO ARM_PG_RO
#define ARCH_PG_BIGPAGE ARM_VM_SECTION

#define ARCH_PGD_SIZE ARM_PGD_SIZE
#define ARCH_PGD_SHIFT ARM_PGD_SHIFT
#define ARCH_PGD_MASK ARM_PGD_MASK

#define ARCH_PG_SIZE ARM_PG_SIZE
#define ARCH_PG_SHIFT ARM_PG_SHIFT
#define ARCH_PG_MASK ARM_PG_MASK

#define ARCH_BIG_PAGE_SIZE ARM_SECTION_SIZE

#define ARCH_PF_PROT(x) ARM_PF_PROT(x)
#define ARCH_PF_NOPAGE(x) ARM_PF_NOPAGE(x)
#define ARCH_PF_WRITE(x) ARM_PF_WRITE(x)

#define ARCH_VM_ADDRESS(pde, pte, offset) \
    ((pde << ARM_PGD_SHIFT) | (pte << ARM_PG_SHIFT) | offset)

#define ARCH_PTE(v) (((unsigned long)(v) >> ARM_PG_SHIFT) & 0xff)
#define ARCH_PDE(x) ((unsigned long)(x) >> ARM_PGD_SHIFT)

#endif