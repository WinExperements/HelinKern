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
#include <dev/x86/serialdev.h>
// The firmware can boot the kernel using HelinBoot Protocol.
#include <helinboot.h>
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
extern char kernel_start[];
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
static kernelInfo *helinboot;
void arch_entry_point(void *arg) {
	// arg is our multiboot structure description
	// Basically, we need just to extract the arguments from the sctructure then pass it to the global entry point
	info = (multiboot_info_t *)arg;
    helinboot = NULL;
    // Check if we booted from multiboot, otwise just reset the system
    if (info->flags == 0x454c494e) {
		output_uart_init();
		dontFB = dontVGA = true;
		kprintf("helinboot, magic: 0x%x!\r\n",HELINBOOT_MAGIC);
		helinboot = (kernelInfo *)info;
	} else if (!(info->flags >> 6 & 0x1)) {
		kprintf("Nor grub nor helinboot?\r\n");
		PANIC("Cannot process anything other");
	}
     kernel_main(helinboot != NULL ? helinboot->cmdline : (char *)info->cmdline);
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
    //initAcpi(info);
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
    kprintf("Skipped some work of arch_getFBInfo\r\n");
    if (helinboot != NULL) {
	    // Okay, our custom boot protocol, that i developed when tryed to boot headless version of kernel on ARM EFI device(real HW)
	    if (helinboot->framebufferAddress == 0) return false;
	    fb_info->addr = (void *)helinboot->framebufferAddress;
	    fb_info->width = (uint32_t)helinboot->framebufferWidth;
	    fb_info->height = (uint32_t)helinboot->framebufferHeight;
	    fb_info->pitch = (uint32_t)helinboot->framebufferPitch;
	    return true;
    }
    fb_info->addr = (void *)(uint32_t)info->framebuffer_addr;
    fb_info->width = info->framebuffer_width;
    fb_info->height = info->framebuffer_height;
    fb_info->pitch = info->framebuffer_pitch;
    return true;
}
int arch_getMemSize() {
    uint32_t size = 0;
    // Check whatever the HelinBoot protocol is used or no.
    if (helinboot != NULL) {
	    // Okay, used.
	    for (int i = 0; i < helinboot->memoryMapCount; i++) {
		    MemoryMapEntry ent = helinboot->mmap_start[i];
		    if (ent.magic != HELINBOOT_MMAP_MAGIC) {
			    PANIC("Corrupted HelinBoot descirptors.");
		    }
		    if (ent.type == 7 && ent.size > size) {
			    size = ent.size;
		    }
	   }
	   return size / 1024;
    	}
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
    return (void *)regs;
}
void arch_syscall_init() {
    interrupt_add(0x80,syscall_handler);
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
    return hdr->e_machine == 3;
}
int arch_getModuleAddress() {
    if (helinboot != NULL) {
	    // Yes, X86 now support HelinBoot boot protocol.
	    if (helinboot->moduleCount == 0) return 0;
	    return (int)helinboot->mod[0].begin;
    }
    if (info->mods_count == 0) return 0;
    multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr;
    return mod->mod_start;
}
int arch_getKernelEnd() {
    if (helinboot != NULL) {
	    if (helinboot->moduleCount == 0) return (int)kernel_end;
	    return (int)helinboot->mod[helinboot->moduleCount-1].end;
    }
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
	// HelinBoot typically fit into one page.
	if (helinboot != NULL) {
		arch_mmu_mapPage(NULL,(int)helinboot,(int)helinboot,3);
	}
	symbols_init(info);
	apic_init();
	smp_init();
	smp_post_init();
    	acpiPostInit();
    	serialdev_init();
	// Create initrd
	void *initrdAddr = NULL;
	int initrdSize = 0;
	if (helinboot != NULL) {
		// Different boot protocol!
		if (helinboot->moduleCount == 0) return;
		ModuleInfo *mod = helinboot->mod;
		initrdAddr = (void *)mod->begin;
		initrdSize = mod->size;
		// Reserve the memory where the structures are located.
		//alloc_reserve((int)helinboot,(int)helinboot+4096);
	} else {
		multiboot_module_t *initrdData = (multiboot_module_t *)info->mods_addr;
        	int size = initrdData->mod_end - initrdData->mod_start;
		initrdAddr = (void *)initrdData->mod_start;
		initrdSize = size;
	}
	vfs_node_t *initrd = vfs_creat(vfs_getRoot(),"initrdram",0);
	rootfs_insertModuleData(initrd,initrdSize,(char *)initrdAddr);
	// After data insertion process, map the data itself.
	arch_mmu_map(NULL,(int)initrdAddr,initrdSize,7);
	if (helinboot == NULL) {
		kprintf("Multiboot located at 0x%x, mmap: 0x%x, module: 0x%x, kernel start: 0x%x, initrd 0x%x, kernel_end: 0x%x\r\n",info,info->mmap_addr,info->mods_addr,(int)kernel_start,initrdAddr,kernel_end);
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
    uint32_t largestExtendedFunc;
    cpuid(0x80000000, &largestExtendedFunc, &ebx, &ecx, &edx);
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

void userModeEntry() {
    return; // nothing here
}

void arch_switchToUserMode() {
	// We can use x86_jumpToUser
    int esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    x86_jumpToUser((int)userModeEntry,esp);
}

/* Oh.....the fucking signals..... */
/* first ever long name of function */
void arch_processSignal(process_t *nextTask,int address,...) {
    // Extract the signal number
    va_list list;
    va_start(list,address);
    int num = va_arg(list,int);
    va_end(list);
    x86_task_t *task = (x86_task_t *)nextTask->arch_info;
    if (task->signalReturn == NULL) {
	    task->signalReturn = kmalloc(sizeof(registers_t));
            memcpy(task->signalReturn,task->userTaskRegisters,sizeof(registers_t));
	} else {
		memcpy(task->signalReturn,task->userTaskRegisters,sizeof(registers_t));
	}
    task->userTaskRegisters->eip = address;
    task->userTaskRegisters->eax = num; // to be handled by newlib or other libc.
}
void arch_exitSignal(process_t *prc) {
	x86_task_t *task = (x86_task_t *)prc->arch_info;
	if (task->signalReturn == NULL) {
		kprintf("W??\r\n");
		return;
	}
	memcpy(task->userTaskRegisters,task->signalReturn,sizeof(registers_t));
	x86_switch(task->userTaskRegisters);
}
