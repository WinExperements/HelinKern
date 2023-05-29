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
#include <lib/string.h>
#include <debug.h>
#include <module.h>
#include <arch/x86/cpuid.h>
#include <arch/x86/smp.h>
#define PUSH(tos,val) (*(-- tos) = val)
static multiboot_info_t *info;
void x86_switchContext(void *);
extern int syscall_num;
static int stack_addr = 0;
extern char kernel_end[];
extern void x86_switchToNew(int esp);
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
    apic_init();
    smp_init();
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
    int *frame = (kmalloc(4096)+4096);
    stack_addr = ((int)frame-4096);
    PUSH(frame, 0);        // entry point
    PUSH(frame, entry);  // startup function
    PUSH(frame, 0);           // EBP
    PUSH(frame, 0);           // EDI
    PUSH(frame, 0);           // ESI
    PUSH(frame, 0);           // EBX
    return (void *)(int)frame;
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
	    // check extension then install it as module
	    int name_size = strlen((char *)mod->cmdline)-4;
	    if (strcmp((char *)mod->cmdline+name_size,".mod")) {
			DEBUG("Loading module %s\r\n",mod->cmdline);
			module_t *m = load_module((void *)mod->mod_start);
			if (m) {
				m->init(m);
			} else {
				DEBUG_N("Failed\r\n");
			}
		}
        }
    }
    smp_post_init();
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
    DEBUG_N("Warrning: Using unstable ACPI shutdown code!\r\n");
    kwait(5000);
    acpiPowerOff();
}
void arch_reschedule() {
    asm volatile("int $32");
}
/* Usually called at startup or when user trying to read from /proc/cpuinfo */
/* Original: https://github.com/pdoane/osdev */
void cpuid(uint32_t reg,uint32_t *eax,uint32_t *ebx,uint32_t *ecx,uint32_t *edx) {
    asm volatile("cpuid" : "=a" (*eax), "=b" (*ebx), "=c"(*ecx),"=d"(*edx) : "0" (reg));
}
void arch_detect() {
    uint32_t eax,ebx,ecx,edx;
    uint32_t largestStandardFunc;
    char vendor[13];
    cpuid(0,&largestStandardFunc,(uint32_t *)(vendor + 0),(uint32_t *)(vendor + 8),(uint32_t *)(vendor + 4));
    vendor[12] = 0;
    DEBUG_N("X86 CPU Detect\r\n");
    DEBUG("CPU Vendor: %s\r\n",vendor);
     if (largestStandardFunc >= 0x01)
    {
        cpuid(0x01, &eax, &ebx, &ecx, &edx);

        DEBUG_N("Features:");

        if (edx & EDX_PSE)      kprintf(" PSE");
        if (edx & EDX_PAE)      kprintf(" PAE");
        if (edx & EDX_APIC)     kprintf(" APIC");
        if (edx & EDX_MTRR)     kprintf(" MTRR");

        kprintf("\r\n");

        DEBUG_N("Instructions:");

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

        kprintf("\r\n");
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
            DEBUG_N("Detected 64-bit CPU!\r\n");
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

        DEBUG("CPU Name: %s\r\n", p);
    }
}
