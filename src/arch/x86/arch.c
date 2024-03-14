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
#include <arch/mmu.h>
#include <arch/x86/mmu.h>
/* Define some defines that needed for kernel allocator */
int KERN_HEAP_BEGIN=0x02000000; //32 mb
int KERN_HEAP_END=0x40000000; // 1 gb
int USER_OFFSET=0x40000000;
int USER_MMAP_START=0x80000000; //This is just for mapping starts searching vmem from here not to conflict with sbrk. It can start from USER_OFFSET if sbrk not used!
int MEMORY_END=0xFFC00000; //After this address is not usable. Because Page Tables sit there!
// Why my previous define is very starnge, like the stack is increments to 200!
// Now all works fine as planed.
#define	USER_STACK 			0xF0000000
#define PUSH(stack, type, value)\
    do {\
        (stack) -= sizeof(type);\
        *((type *) (stack)) = (type) (value);\
    } while (0)
static multiboot_info_t *info;
void x86_switchContext(void *);
extern int syscall_num;
static int stack_addr = 0;
extern char kernel_end[];
extern void x86_switchToNew(int esp);
extern void vga_change();
static bool dontFB = false;
static bool dontVGA = false;
static void thread_main(int entryPoint,int esp,bool isUser);
extern void x86_switch(registers_t *to);
extern void x86_jumpToUser(int entryPoint,int userstack);
static char cpuName[48];
static void setup_fpu();
static registers_t *syscall_regs; // required by arch_syscall_getCallerRet
extern bool showByte;
extern void dump_registers(const registers_t *);
extern bool disableOutput;
void arch_entry_point(void *arg) {
	// arg is our multiboot structure description
	// Basically, we need just to extract the arguments from the sctructure then pass it to the global entry point
	info = (multiboot_info_t *)arg;
    // Check if we booted from multiboot, otwise just reset the system
    if (!(info->flags >> 6 & 0x1)) {
    	// Init serial, and print that we loaded by wrong bootloader
    	
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
    uint32_t divisor = 1193182 / 1000;
    outb(0x43,0x00 | 0x06 | 0x30 | 0x00);
    outb(0x40,divisor);
    outb(0x40,divisor >> 8);
    initAcpi(info);
    if (dontFB && !dontVGA) vga_change();
    //apic_init();
    //smp_init();
    setup_fpu();
    // detect and copy the initrd if the address is bigger that the reserved area of MMU
    
    
}
void arch_sti() {
    asm volatile("sti");
}
void arch_cli() {
    asm volatile("cli");
}
bool arch_getFBInfo(fbinfo_t *fb_info) {
    if (!fb_info) return false;
    if (dontFB) return false;
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
void *arch_prepareContext(int entry,bool isUser) {
    int frame = (int)(kmalloc(4096)+4096);
    stack_addr = ((int)frame-4096);
    memset((void *)stack_addr,0,4096);
    if (!isUser) {
        PUSH(frame,int,0);
        PUSH(frame,int,entry);
    } else {
        PUSH(frame,int,isUser);
        PUSH(frame,int,0);
        PUSH(frame,int,entry);
        PUSH(frame,int,0);
        PUSH(frame,int, (int)thread_main);
    }
    PUSH(frame,int, 0);           // EBP
    PUSH(frame,int, 0);           // EDI
    PUSH(frame,int, 0);           // ESI
    PUSH(frame,int, 0);           // EBX
    return (void *)frame;
}
void *arch_preapreArchStack(bool isUser) {
    x86_task_t *t = kmalloc(sizeof(x86_task_t));
    memset(t,0,sizeof(x86_task_t));
    t->kesp_start = (isUser ? (int)kmalloc(4096) : 0);
    t->kernelESP = t->kesp_start+4096;
    t->stack = stack_addr;
    return t;
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
    arch_cli();
    // Call the handler
    syscall_regs = regs;
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
    arch_sti();
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
    if (task->kesp_start != 0) {
        kfree((void *)task->kesp_start);
    }
    
    kfree((void *)task->stack);
    /*
     * User ESP pointer is automatically freed in arch_mmu_destroyAspace
    */
    if (task->argc != 0) {
        kfree((void *)task->argv);
    }
    kfree(stack);
}
void arch_post_init() {
    /*if (dontFB) {
	vga_change();
    }*/
    kprintf("%s: begin of post init\n",__func__);
    kprintf("X86 official port post init process begin. Not all X86 platforms are supported. Build date of this support module: %s\r\n",__DATE__);
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
	    // Map all modules into memory :)
	    int module_size = mod->mod_end-mod->mod_start;
	    arch_mmu_map(NULL,(int)mod->mod_start,module_size,7);
        }
    }
	apic_init();
	smp_init();
	smp_post_init();
	if (!acpiIsOn()) {
		kprintf("WARRNING: No ACPI available, the system can't be shutted down\r\n");
        
	}
	unsigned int cr0;

        // Read the CR0 register
        asm("mov %%cr0, %0" : "=r"(cr0));
        if ((cr0 & 0x2)) {
                kprintf("WARRNING: MSDOS compatibility of FPU is enabled!\n");
        }  else {
		kprintf("FPU isn't enabled with MSDOS compatibility mode\n");
	}
    acpiPostInit();
	kprintf("HelinOS X86 part is successfully ended in arch_post_init\r\n");
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

        kprintf("\r\n");

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
            kprintf("Detected 64-bit CPU!\r\n");
            #ifndef AMD64_SUPPORT
            kprintf("Kernel booted in 32-bit mode, only 4GB is usable\r\n");
            #endif
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
	strcpy(cpuName,p);
        kprintf("CPU Name: %s\r\n", p);
    }
}
void arch_sysinfo() {
	kprintf("This OS is builded for X86 PC\r\n");
	kprintf("Is ACPI Shutdown Available: %s\r\n",(acpiIsOn() ? "Yes" : "No"));
	kprintf("Detected cores: %d\r\n",smp_getCPUCount());
	kprintf("Machine Name: %s\r\n",cpuName);
}
static void thread_main(int entryPoint,int esp,bool isUser) {
    /* Через те, що наш метод переключення контексту процесів працює лише з задачами ядра, ми будемо перемикатися в режим користувача.
    */
    process_t *prc = thread_getThread(thread_getCurrent());
    x86_task_t *task = (x86_task_t *)prc->arch_info;
    if (task->forkESP != 0) {
	    // We after fork, jump to user mode
	    //kprintf("After fork! EIP: 0x%x\n",entryPoint);
	    //x86_jumpToUser(entryPoint,task->forkESP);
        kfree((void *)task->forkESP);
        x86_switch((registers_t *)task->forkESP);
	    PANIC("Failed to switch to user mode!"); // never happening

    }
    // На данний момент часу ми працюємо в кільці ядра!
    //int stackSize = 50*4096;
    int _esp = (int)sbrk(prc,4096);
    //if (isUser) arch_mmu_map(NULL,_esp,stackSize,7 | 0x00000200);
    //int _esp = (int)kmalloc(4096);
    x86_task_t *archStack = (x86_task_t *)prc->arch_info;
    archStack->userESP = _esp;
    // clean memory
    memset((void *)_esp,0,4096);
    uint32_t stack = (uint32_t )_esp+4096;
    archStack->userESP_top = stack;
    PUSH(stack,char **,(char **)(uintptr_t)archStack->argv);
    PUSH(stack,int,archStack->argc);
    x86_jumpToUser(entryPoint,(int)stack);
}
void arch_putArgs(process_t *prc,int argc,char **argv) {
    DEBUG("%s: put %s arguments with pointer to 0x%x\r\n",__func__,argc,argv);
    if (prc == NULL || argv == 0) return;
    x86_task_t *s = (x86_task_t *)prc->arch_info;
    s->argc = argc;
    s->argv = (int)argv;
}
struct stackframe {
  struct stackframe* ebp;
  uint32_t eip;
};
void arch_trace() {
    struct stackframe *stk;
    asm ("movl %%ebp,%0" : "=r"(stk) ::);    
    for(unsigned int frame = 0; stk && frame < 10; ++frame) {
        kprintf("0x%x, %s\r\n",stk->eip,symbols_findName(stk->eip));
        stk = stk->ebp;
    }
}
void arch_prepareProcess(process_t *prc) {
	// Allocate the FPU context
	// Save current FPU context, before initializing new
	void *current_fpu_state = 0;
    	arch_fpu_save(current_fpu_state);
	prc->fpu_state = kmalloc(512 + 15); // Default 32-bit FPU context size
	uintptr_t addr = (uintptr_t)prc->fpu_state;
    	uintptr_t aligned_addr = (addr + 15) & ~0xF;
	prc->fpu_state = (void *)aligned_addr;
	memset(prc->fpu_state,0,512);
	arch_fpu_restore(prc->fpu_state);
	asm volatile("finit");
	arch_fpu_save(prc->fpu_state);
	arch_fpu_restore(current_fpu_state);
}

static void set_fpu_cw(const uint16_t cw) {
	asm volatile("fldcw %0" :: "m"(cw));
}

static void setup_fpu() {
    // Source code taken from ToaruOS 1.0x
    int cr4;
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    kprintf("CR4 before: 0x%x\n", cr4);
    cr4 |= (1 << 10);
    asm volatile ("mov %0, %%cr4" :: "r"(cr4));
    kprintf("CR4 set, current value: 0x%x\n", cr4);

    set_fpu_cw(895); // Set the FPU control word to 0x037F in decimal format

    // Enable FPU exceptions
    unsigned int cr0;

    // Read the current value of CR0
    asm volatile("mov %%cr0, %0" : "=r" (cr0));

    // Set the NE flag (bit 5) to 1
    cr0 |= (1 << 5);

    // Write the modified value back to CR0
    asm volatile("mov %0, %%cr0" :: "r" (cr0));

    asm volatile("finit");
}

int arch_syscall_getCallerRet() {
	if (syscall_regs != NULL) {
        //kprintf("%s: caller userESP: 0x%x, ESP: 0x%x\n",__func__,syscall_regs->useresp,0);
		return syscall_regs->eip;
	}
	return NULL;
}
void arch_forkProcess(process_t *parent,process_t *child) {
	/*
	 * X86 specifc code for fork system call
	 * We set the arch specifc structure to indicate for thread_main that process is after fork, so it jump to the copied entry point and saved stack here
	 * We use useresp because it's is a pointer to user space ESP value that automatically loaded for user space programs by iret
	 * Also we need to copy parent's FPU state for our child, so it is able to continue using FPU commands
	*/
        x86_task_t *t = (x86_task_t *)child->arch_info;
        //t->forkESP = syscall_regs->useresp;
     registers_t *regs = (registers_t *)kmalloc(sizeof(registers_t));
     memcpy(regs,syscall_regs,sizeof(registers_t));
    regs->eax = 0; // we a child so clean EAX!
    t->forkESP = (int)regs;
	//kprintf("FORK ESP: 0x%x\n",t->forkESP);
        // Copy FPU state
	memcpy(child->fpu_state,parent->fpu_state,512);
	//dump_registers(syscall_regs);
}

char *arch_getName() {
    return "x86";
}
