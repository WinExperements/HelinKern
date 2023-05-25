/* X86 arch-specific code implementation */
#include <arch.h>
#include <kernel.h>
#include <typedefs.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <mm/alloc.h>
#include <arch/x86/task.h>
#include <arch/x86/io.h>
#include <thread.h>
#include <syscall.h>
#include <arch/elf.h>
#include <output.h>
#include <symbols.h>
#include <arch/x86/acpi.h>
#define PUSH(tos,val) (*(-- tos) = val)
#define ECX_SSE3                        (1 << 0)    // Streaming SIMD Extensions 3
#define ECX_PCLMULQDQ                   (1 << 1)    // PCLMULQDQ Instruction
#define ECX_DTES64                      (1 << 2)    // 64-Bit Debug Store Area
#define ECX_MONITOR                     (1 << 3)    // MONITOR/MWAIT
#define ECX_DS_CPL                      (1 << 4)    // CPL Qualified Debug Store
#define ECX_VMX                         (1 << 5)    // Virtual Machine Extensions
#define ECX_SMX                         (1 << 6)    // Safer Mode Extensions
#define ECX_EST                         (1 << 7)    // Enhanced SpeedStep Technology
#define ECX_TM2                         (1 << 8)    // Thermal Monitor 2
#define ECX_SSSE3                       (1 << 9)    // Supplemental Streaming SIMD Extensions 3
#define ECX_CNXT_ID                     (1 << 10)   // L1 Context ID
#define ECX_FMA                         (1 << 12)   // Fused Multiply Add
#define ECX_CX16                        (1 << 13)   // CMPXCHG16B Instruction
#define ECX_XTPR                        (1 << 14)   // xTPR Update Control
#define ECX_PDCM                        (1 << 15)   // Perf/Debug Capability MSR
#define ECX_PCID                        (1 << 17)   // Process-context Identifiers
#define ECX_DCA                         (1 << 18)   // Direct Cache Access
#define ECX_SSE41                       (1 << 19)   // Streaming SIMD Extensions 4.1
#define ECX_SSE42                       (1 << 20)   // Streaming SIMD Extensions 4.2
#define ECX_X2APIC                      (1 << 21)   // Extended xAPIC Support
#define ECX_MOVBE                       (1 << 22)   // MOVBE Instruction
#define ECX_POPCNT                      (1 << 23)   // POPCNT Instruction
#define ECX_TSC                         (1 << 24)   // Local APIC supports TSC Deadline
#define ECX_AESNI                       (1 << 25)   // AESNI Instruction
#define ECX_XSAVE                       (1 << 26)   // XSAVE/XSTOR States
#define ECX_OSXSAVE                     (1 << 27)   // OS Enabled Extended State Management
#define ECX_AVX                         (1 << 28)   // AVX Instructions
#define ECX_F16C                        (1 << 29)   // 16-bit Floating Point Instructions
#define ECX_RDRAND                      (1 << 30)   // RDRAND Instruction

#define EDX_FPU                         (1 << 0)    // Floating-Point Unit On-Chip
#define EDX_VME                         (1 << 1)    // Virtual 8086 Mode Extensions
#define EDX_DE                          (1 << 2)    // Debugging Extensions
#define EDX_PSE                         (1 << 3)    // Page Size Extension
#define EDX_TSC                         (1 << 4)    // Time Stamp Counter
#define EDX_MSR                         (1 << 5)    // Model Specific Registers
#define EDX_PAE                         (1 << 6)    // Physical Address Extension
#define EDX_MCE                         (1 << 7)    // Machine-Check Exception
#define EDX_CX8                         (1 << 8)    // CMPXCHG8 Instruction
#define EDX_APIC                        (1 << 9)    // APIC On-Chip
#define EDX_SEP                         (1 << 11)   // SYSENTER/SYSEXIT instructions
#define EDX_MTRR                        (1 << 12)   // Memory Type Range Registers
#define EDX_PGE                         (1 << 13)   // Page Global Bit
#define EDX_MCA                         (1 << 14)   // Machine-Check Architecture
#define EDX_CMOV                        (1 << 15)   // Conditional Move Instruction
#define EDX_PAT                         (1 << 16)   // Page Attribute Table
#define EDX_PSE36                       (1 << 17)   // 36-bit Page Size Extension
#define EDX_PSN                         (1 << 18)   // Processor Serial Number
#define EDX_CLFLUSH                     (1 << 19)   // CLFLUSH Instruction
#define EDX_DS                          (1 << 21)   // Debug Store
#define EDX_ACPI                        (1 << 22)   // Thermal Monitor and Software Clock Facilities
#define EDX_MMX                         (1 << 23)   // MMX Technology
#define EDX_FXSR                        (1 << 24)   // FXSAVE and FXSTOR Instructions
#define EDX_SSE                         (1 << 25)   // Streaming SIMD Extensions
#define EDX_SSE2                        (1 << 26)   // Streaming SIMD Extensions 2
#define EDX_SS                          (1 << 27)   // Self Snoop
#define EDX_HTT                         (1 << 28)   // Multi-Threading
#define EDX_TM                          (1 << 29)   // Thermal Monitor
#define EDX_PBE                         (1 << 31)   // Pending Break Enable

// ------------------------------------------------------------------------------------------------
// Extended Function 0x01

#define EDX_SYSCALL                     (1 << 11)   // SYSCALL/SYSRET
#define EDX_XD                          (1 << 20)   // Execute Disable Bit
#define EDX_1GB_PAGE                    (1 << 26)   // 1 GB Pages
#define EDX_RDTSCP                      (1 << 27)   // RDTSCP and IA32_TSC_AUX
#define EDX_64_BIT                      (1 << 29)   // 64-bit Architecture
static multiboot_info_t *info;
void x86_switchContext(void *);
extern int syscall_num;
static int stack_addr = 0;
extern char kernel_end[];
void arch_entry_point(void *arg) {
	// arg is our multiboot structure description
	// Basically, we need just to extract the arguments from the sctructure then pass it to the global entry point
	info = (multiboot_info_t *)arg;
    // Check if we booted from multiboot, otwise just reset the system
    if (!(info->flags >> 6 & 0x1)) {
        arch_reset();
        return;
    }
    kernel_main((char *)info->cmdline);
}
void arch_reset() {
    arch_cli();
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
loop:
    asm volatile ("hlt"); 
	goto loop;
}
void arch_pre_init() {
    // Nothing pre-init currently
}
void arch_init() {
    // Init GDT and IDT
    gdt_init();
    idt_init();
    // Set TSS stack
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    tss_set_stack(0x10,esp);
    outb(0x21, 0x00);
   	outb(0xa1, 0x00);
    // Init timer
    uint32_t divisor = 119180 / 1000;
    outb(0x43,0x36);
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);
    outb(0x40,l);
    outb(0x40,h);
    initAcpi();
}
void arch_sti() {
    asm volatile("sti");
}
void arch_cli() {
    asm volatile("cli");
}
bool arch_getFBInfo(fbinfo_t *fb_info) {
    if (!fb_info) return false;
    fb_info->addr = (void *)(uint32_t)info->framebuffer_addr;
    fb_info->width = info->framebuffer_width;
    fb_info->height = info->framebuffer_height;
    fb_info->pitch = info->framebuffer_pitch;
    return true;
}
int arch_getMemSize() {
    uint32_t size = 0;
	for (unsigned long i = 0; i < info->mmap_length; i+=sizeof(multiboot_memory_map_t)) {
		multiboot_memory_map_t *en = (multiboot_memory_map_t *)(info->mmap_addr+i);
		if (en->type == 1 && en->len_low > size) {
			size = en->len_low;
		}
	}
    return size/1024;
}
void *arch_prepareContext(int entry) {
    int *stack = (int *)(kmalloc(4096)+4096);
    stack_addr = ((int)stack-4096);
    PUSH(stack,0);
    PUSH(stack,entry);
    PUSH(stack,0);
    PUSH(stack,0);
    PUSH(stack,0);
    PUSH(stack,0);
    return (void *)(int)stack;
}
void *arch_preapreArchStack(bool isUser) {
    x86_task_t *t = kmalloc(sizeof(x86_task_t));
    t->kernelESP = (isUser ? (int)kmalloc(4096) : 0);
    t->stack = stack_addr;
    return t;
    //return NULL; // test
}
void arch_switchContext(void *stack) {
    process_t *p = (process_t *)stack;
    x86_task_t *d = (x86_task_t *)p->arch_info;
    // switch TSS stacks
    tss_set_stack(0x10,d->kernelESP);
    // now actual switch
    x86_switchContext(stack);
}
static void *syscall_handler(void *_regs) {
    registers_t *regs = (registers_t *)_regs;
    if (regs->eax > (uint32_t)syscall_num) return _regs;
    int location = syscall_get(regs->eax);
    arch_sti();
    // Call the handler
    int ret;
    asm volatile("push %1; \
    push %2; \
    push %3; \
    push %4; \
    push %5; \
    call *%6; \
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx; \
    " : "=a" (ret) : "r" (regs->esi), "r" (regs->edi), "r" (regs->ebx), "r"(regs->ecx),"r"(regs->edx),"r"(location));
    regs->eax = ret;
    return (void *)regs;
}
void arch_syscall_init() {
    interrupt_add(0x80,syscall_handler);
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
    return hdr->e_machine == 3;
}
int arch_getModuleAddress() {
    if (info->mods_count == 0) return 0;
    multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr;
    return mod->mod_start;
}
int arch_getKernelEnd() {
    if (info->mods_count == 0) return (int)kernel_end;
    multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr+info->mods_count-1;
    return mod->mod_end;
}
void arch_destroyContext(void *context) {
   
}
void arch_destroyArchStack(void *stack) {
    x86_task_t *task = (x86_task_t *)stack;
    if (task->kernelESP != 0) {
        kfree((void *)task->kernelESP);
    }
    kfree((void *)task->stack);
    kfree(stack);
}
void arch_post_init() {
    symbols_init(info);
    // Copy all modules into rootfs
    vfs_node_t *node = vfs_find("/bin");
    if (!node) return;
    for (uint32_t i = 0; i < info->mods_count; i++) {
        multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr+i;
        if (mod->cmdline != 0) {
            vfs_node_t *in = vfs_creat(node,(char *)mod->cmdline,0);
            rootfs_insertModuleData(in,mod->mod_end-mod->mod_start,(char *)mod->mod_start);
        }
    }
}
bool arch_relocSymbols(module_t *mod,void *ehdr) {
	Elf32_Ehdr *e = (Elf32_Ehdr *)ehdr;
	Elf32_Shdr *s;
	Elf32_Word entsize;
	unsigned i;
	// Find symbol table
	for (i = 0,s = (Elf32_Shdr *)((char *) e + e->e_shoff); i < e->e_shnum;
		i++,s = (Elf32_Shdr *)((char *) s + e->e_shentsize)) {
		if (s->sh_type == SHT_SYMTAB) {
			break;
		}
	}
	if (i == e->e_shnum) {
		kprintf("relloc: no symbol table to relocate!\n");
		return false;
	}
	entsize = s->sh_entsize;
	for (i = 0,s = (Elf32_Shdr *)((char *)e + e->e_shoff); i < e->e_shnum;
		i++,s = (Elf32_Shdr *)((char *)s + e->e_shentsize)) {
			if (s->sh_type == SHT_REL) {
				module_segment_t *seg;
				for (seg = mod->seg; seg; seg = seg->next) {
					if (seg->section == s->sh_info) break;
				}
				if (seg) {
					Elf32_Rel *rel,*max;
					for (rel = (Elf32_Rel *)((char *) e + s->sh_offset),
						max = rel + s->sh_size / s->sh_entsize;
						rel < max;
						rel++) {
						Elf32_Word *addr;
						Elf32_Sym *sym;
						if (seg->size < rel->r_offset) {
							kprintf("Relloc offset is out of segment\n");
							return false;
						}
						addr = (Elf32_Word *)((char *)seg->addr + rel->r_offset);
						sym = (Elf32_Sym *)((char *)mod->symtab + entsize * ELF32_R_SYM(rel->r_info));
						switch (ELF32_R_TYPE(rel->r_info)) {
							case R_386_32:
								*addr += sym->st_value;
								break;
							case R_386_PC32:
								*addr += (sym->st_value - (Elf32_Word)seg->addr - rel->r_offset);
								break;
							case R_386_PLT32:
								kprintf("R_386_PLT32 didn't supported currently!\n");
								return false;
							case R_386_GOTOFF:
								kprintf("R_386_GOTOFF didn't supported!\n");
								return false;
							case R_386_GOTPC:
								kprintf("R_386_GOTPC didn't supported!\n");
								return false; // need to be added!
							default:
							kprintf("Unknown relocation type!\n");
							break;
						}
					}
				}
			}
	}
	return true;
}
void arch_poweroff() {
    kprintf("Warrning: Using unstable ACPI shutdown code!\r\n");
    kwait(5000);
    acpiPowerOff();
}
void arch_reschedule() {
    asm volatile("int $32");
}
static inline void cpuid(uint32_t reg,uint32_t *eax,uint32_t *ebx,uint32_t *ecx,uint32_t *edx) {
    asm volatile("cpuid" : "=a" (*eax), "=b" (*ebx), "=c"(*ecx),"=d"(*edx) : "0" (reg));
}
/* Usually called at startup or when user trying to read from /proc/cpuinfo */
/* Original: https://github.com/pdoane/osdev */
void arch_detect() {
    uint32_t eax,ebx,ecx,edx;
    uint32_t largestStandardFunc;
    char vendor[13];
    cpuid(0,&largestStandardFunc,(uint32_t *)(vendor + 0),(uint32_t *)(vendor + 8),(uint32_t *)(vendor + 4));
    vendor[12] = 0;
    kprintf("X86 CPU Detect\r\n");
    kprintf("CPU Vendor: %s\r\n",vendor);
     if (largestStandardFunc >= 0x01)
    {
        cpuid(0x01, &eax, &ebx, &ecx, &edx);

        kprintf("Features:");

        if (edx & EDX_PSE)      kprintf(" PSE");
        if (edx & EDX_PAE)      kprintf(" PAE");
        if (edx & EDX_APIC)     kprintf(" APIC");
        if (edx & EDX_MTRR)     kprintf(" MTRR");

        kprintf("\n");

        kprintf("Instructions:");

        if (edx & EDX_TSC)      kprintf(" TSC");
        if (edx & EDX_MSR)      kprintf(" MSR");
        if (edx & EDX_SSE)      kprintf(" SSE");
        if (edx & EDX_SSE2)     kprintf(" SSE2");
        if (ecx & ECX_SSE3)     kprintf(" SSE3");
        if (ecx & ECX_SSSE3)    kprintf(" SSSE3");
        if (ecx & ECX_SSE41)    kprintf(" SSE41");
        if (ecx & ECX_SSE42)    kprintf(" SSE42");
        if (ecx & ECX_AVX)      kprintf(" AVX");
        if (ecx & ECX_F16C)     kprintf(" F16C");
        if (ecx & ECX_RDRAND)   kprintf(" RDRAND");

        kprintf("\n");
    }

    // Extended Function 0x00 - Largest Extended Function

    uint32_t largestExtendedFunc;
    cpuid(0x80000000, &largestExtendedFunc, &ebx, &ecx, &edx);

    // Extended Function 0x01 - Extended Feature Bits

    if (largestExtendedFunc >= 0x80000001)
    {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (edx & EDX_64_BIT)
        {
            kprintf("Detected 64-bit CPU!\n");
        }
    }

    // Extended Function 0x02-0x04 - Processor Name / Brand String

    if (largestExtendedFunc >= 0x80000004)
    {
        char name[48];
        cpuid(0x80000002, (uint32_t *)(name +  0), (uint32_t *)(name +  4), (uint32_t *)(name +  8), (uint32_t *)(name + 12));
        cpuid(0x80000003, (uint32_t *)(name + 16), (uint32_t *)(name + 20), (uint32_t *)(name + 24), (uint32_t *)(name + 28));
        cpuid(0x80000004, (uint32_t *)(name + 32), (uint32_t *)(name + 36), (uint32_t *)(name + 40), (uint32_t *)(name + 44));

        // Processor name is right justified with leading spaces
        const char *p = name;
        while (*p == ' ')
        {
            ++p;
        }

        kprintf("CPU Name: %s\n", p);
    }
}
