/* X86 and X86_64 arch-specific code implementation */
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
#include <arch/x86/firmware.h>
#include <dev/keyboard.h>
#include <dev/ps2mouse.h>
#include <atapi/atapi.h>
#include <ahci/ahci.h>
#include <ext2/ext2.h>
#include <pci/pci.h>
#include <arch/x86/idt.h>
#include <net/amd_am79c973/amd_am79c973.h>
#include <gpu/amdgpu/amdgpu.h>
#include <usb/usb.h>
#include <audio/ac97/ac97.h>
#include <fat32/fat32.h>
#include <mbr/mbr.h>
#include <iso9660/iso9660.h>
// The firmware can boot the kernel using HelinBoot Protocol.
#include <helinboot.h>
#if defined(__x86_64__)
/* Test the Lemine protocol */
#include <limine.h>
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);
__attribute__((used,section(".requests")))
volatile struct limine_hhdm_request hhdm_req = {
	.id = LIMINE_HHDM_REQUEST,
	.revision = 2
};
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};
/* Request the memory map */
__attribute__((used,section(".requests")))
static volatile struct limine_memmap_request memmap_req = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0
};
__attribute__((used,section(".requests")))
static volatile struct limine_module_request modreq = {
	.id = LIMINE_MODULE_REQUEST,
	.revision = 0
};
__attribute__((used,section(".requests")))
volatile struct limine_kernel_address_request krnaddr_req = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 0
};
__attribute__((used,section(".requests")))
volatile struct limine_smp_request smpreq = {
	.id = LIMINE_SMP_REQUEST,
	.revision = 0
};
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;
static int smp_cores = 0;
static uintptr_t virtualOffset;
static uintptr_t physicalOffset;
static uintptr_t kernelVirtualOffset;
#endif
extern process_t *runningTask;
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
static void thread_main(uintptr_t entryPoint,int esp,bool isUser);
extern void x86_switch(registers_t *to);
extern void x86_jumpToUser(int entryPoint,int userstack);
static char cpuName[48];
static void setup_fpu();
static registers_t *syscall_regs; // required by arch_syscall_getCallerRet
extern bool showByte;
extern void dump_registers(const registers_t *);
extern bool disableOutput;
static kernelInfo *helinboot;
#define FB_TEST_COLOR(a, r, g, b) ((a << 24) | (r << 16) | (g << 8) | b)
void arch_entry_point(void *arg) {
	// arg is our multiboot structure description
	// Basically, we need just to extract the arguments from the sctructure then pass it to the global entry point
#if defined(__x86_64__)
	if (LIMINE_BASE_REVISION_SUPPORTED != 0) {
		kernel_main("-v");
	}
#endif
	info = (multiboot_info_t *)arg;
    helinboot = NULL;
    // Check if we booted from multiboot, otwise just reset the system
    if (info->flags == 0x454c494e) {
		output_uart_init();
		disableOutput = false;
		kprintf("helinboot, magic: 0x%x!\r\n",HELINBOOT_MAGIC);
		helinboot = (kernelInfo *)info;
	} else if (!(info->flags >> 6 & 0x1)) {
		kprintf("Nor grub nor helinboot?\r\n");
		PANIC("Cannot process anything other");
	}
    	char *cmdline = "";
	if (helinboot != NULL) {
		cmdline = helinboot->cmdline;
	} else if (info != NULL) {
		cmdline = (char *)info->cmdline;
	}
	kernel_main(cmdline);
}
void arch_reset() {
    arch_cli();
    kprintf("causing triple fault\n");
    arch_mmu_switch((void *)0x10);
}
void arch_pre_init() {
    // Nothing pre-init currently
}
extern void x86_serial();
static uint32_t fbaddr;
static uint32_t fbpitch;
void arch_init() {
    // Init GDT and IDT
#if defined(__x86_64__)
	gdt_init();
	idt_init();
	outb(0x21, 0x00);
    	outb(0xa1, 0x00);
    	// Init timer
    	uint32_t divisor = 1193180 / 1000;
    	outb(0x43,0x34);
    	outb(0x40,divisor);
    	outb(0x40,(divisor >> 8) & 0xFF);
	if (smpreq.response != NULL) {
		smp_cores = smpreq.response->cpu_count;
	}
	virtualOffset = hhdm_req.response->offset;
	physicalOffset = krnaddr_req.response->physical_base;
	kernelVirtualOffset = krnaddr_req.response->virtual_base;
	return;
#else
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
    setup_fpu();
#endif
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
    if (helinboot != NULL) {
	    // Okay, our custom boot protocol, that i developed when tryed to boot headless version of kernel on ARM EFI device(real HW)
	    if (helinboot->framebufferAddress == 0) return false;
	    fb_info->paddr = (void *)(paddr_t)helinboot->framebufferAddress;
	    fb_info->vaddr = fb_info->paddr;	// HelinBoot passes physical address.
	    fb_info->width = (uint32_t)helinboot->framebufferWidth;
	    fb_info->height = (uint32_t)helinboot->framebufferHeight;
	    fb_info->pitch = (uint32_t)helinboot->framebufferPitch;
	    return true;
    }
#if defined(__x86_64__)
    // X86_64 boots only using limine.
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
	    return false;
	}
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    fb_info->vaddr = fb->address;
    fb_info->paddr = (void *)(uint64_t)fb->address - hhdm_req.response->offset;
    fb_info->width = fb->width;
    fb_info->height = fb->height;
    fb_info->pitch = fb->pitch;
    fbpitch = fb->pitch;
    fbaddr = (uint32_t)(uint64_t)fb_info->vaddr;
    return true;
#else
    fb_info->vaddr = (void *)(uint32_t)info->framebuffer_addr;
    fb_info->paddr = fb_info->vaddr;
    fb_info->width = info->framebuffer_width;
    fb_info->height = info->framebuffer_height;
    fb_info->pitch = info->framebuffer_pitch;
    return true;
#endif
}
uintptr_t arch_getMemSize() {
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
	   return size;
    	}
#if defined(__x86_64__)
    // Limine.
    if (memmap_req.response == NULL) {
	    return 0;
	}
    struct limine_memmap_response *mmap = (struct limine_memmap_response *)memmap_req.response;
    uint64_t lsize = 0;
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
	    struct limine_memmap_entry *entry = (struct limine_memmap_entry *)mmap->entries[i];
	    if (entry->type != LIMINE_MEMMAP_USABLE) {
		    continue;
		}
	    if (entry->length > lsize) {
		    lsize = entry->length;
		}
	}
    return lsize;
#else
	for (unsigned long i = 0; i < info->mmap_length; i+=sizeof(multiboot_memory_map_t)) {
		multiboot_memory_map_t *en = (multiboot_memory_map_t *)(info->mmap_addr+i);
		if (en->type == 1 && en->len_low > size) {
			size = en->len_low;
		}
	}
    return size;
#endif
}
void *arch_prepareContext(void *entry,bool isUser) {
#if defined(__x86_64__)
	uint64_t stack = ((uint64_t)kmalloc(4096));
	memset((void *)stack,0,4096);
	stack+=4095;
	PUSH(stack,uint64_t,entry);
	/*PUSH(stack,uint64_t,0); // rdi.
	PUSH(stack,uint64_t,0); // r15
	PUSH(stack,uint64_t,0); // r14
	PUSH(stack,uint64_t,0); // r13
	PUSH(stack,uint64_t,0); // r12.
	PUSH(stack,uint64_t,0); // rbp.
	PUSH(stack,uint64_t,0); // rbx*/
	PUSH(stack,uint64_t,(isUser ? thread_main : entry));
	x64TaskRegisters *regs = (x64TaskRegisters *)kmalloc(sizeof(x64TaskRegisters));
	memset(regs,0,sizeof(x64TaskRegisters));
	regs->rsp = stack;
	if (isUser) {
		regs->rdi = (uint64_t)entry;
	}
	return regs;
#else
    int frame = (int)(kmalloc(4096)+4096);
    stack_addr = ((int)frame-4096);
    memset((void *)stack_addr,0,4096);
    if (!isUser) {
        PUSH(frame,int,0);
        PUSH(frame,int,(int)entry);
    } else {
        PUSH(frame,int,isUser);
        PUSH(frame,int,0);
        PUSH(frame,int,(int)entry);
        PUSH(frame,int,0);
        PUSH(frame,int, (int)thread_main);
    }
    PUSH(frame,int, 0);           // EBP
    PUSH(frame,int, 0);           // EDI
    PUSH(frame,int, 0);           // ESI
    PUSH(frame,int, 0);           // EBX
    return (void *)frame;
#endif
}
extern void x86_test_switch(void *to);
void *arch_preapreArchStack(bool isUser) {
#if defined(__x86_64__)
	x64_task_t *tsk = (x64_task_t *)kmalloc(sizeof(x64_task_t));
	memset(tsk,0,sizeof(x64_task_t));
	// allocate stacks.
	tsk->rsp0_top = (uintptr_t)kmalloc(4096);
	memset((void *)tsk->rsp0_top,0,4096);
	tsk->rsp0 = tsk->rsp0_top + 4096;
	tsk->softIRQ_top = (uintptr_t)kmalloc(4096);
	memset((void *)tsk->softIRQ_top,0,4096);
	tsk->softIRQ = tsk->softIRQ_top + 4096;
	if (isUser) {
		// Prepare stack for user processes.
		tsk->userStackTop = (uintptr_t)kmalloc(4096);
		tsk->userStack = tsk->userStackTop + 4095;
	}
	return tsk;
#else
    x86_task_t *t = kmalloc(sizeof(x86_task_t));
    memset(t,0,sizeof(x86_task_t));
    t->kesp_start = (isUser ? (int)kmalloc(4096) : 0);
    t->kernelESP = t->kesp_start+4096;
    t->stack = stack_addr;
    return t;
#endif
}
void arch_switchContext(void *stack) {
	process_t *p = (process_t *)stack;
#if defined(__x86_64__)
	x64_task_t *d = (x64_task_t *)p->arch_info;
	tss_set_stack64(d->rsp0,d->softIRQ);
//	runningTask = p;
	x86_switchContext(stack);
#else
    x86_task_t *d = (x86_task_t *)p->arch_info;
    // switch TSS stacks
    tss_set_stack(0x10,d->kernelESP);
    // now actual switch
    x86_switchContext(stack);
    //x86_test_switch(p->stack);
#endif
}
static uintptr_t syscall_handler(uintptr_t _regs) {
#if defined(__x86_64__)
	registers_t *regs = (registers_t*)_regs;
	syscall_regs = regs;
	if (regs->rax > syscall_num) {
		// invalid syscall.
		return _regs;
	}
	x64_task_t *tsk = (x64_task_t *)runningTask->arch_info;
	if (tsk->syscallRegs == NULL) {
		//kprintf("Updating task syscall regs(for %s)...",runningTask->name);
		tsk->syscallRegs = kmalloc(sizeof(registers_t));
		//kprintf("done\n");
	}
	memcpy(tsk->syscallRegs,regs,sizeof(registers_t));
	uintptr_t ret = 0;
	uintptr_t location = syscall_get(regs->rax);
	asm volatile("  push %6;	\
			push %5;	\
			push %4;	\
			push %3;	\
			push %2;	\
			push %1;	\
			sti;		\
			pop %%rdi; 	\
			pop %%rsi; 	\
			pop %%rdx; 	\
			pop %%rcx; 	\
			pop %%r8; 	\
			pop %%r9; 	\
			call *%%r9;	\
			cli;" : "=a"(ret) : "r" (regs->rdx),"r"(regs->rcx),"r"(regs->r8),"r"(regs->rdi),"r"(regs->rsi),"r"(location));
	regs->rax = ret;
	return (uintptr_t)regs;
#else
    registers_t *regs = (registers_t *)_regs;
    if (runningTask != NULL) {
	    x86_task_t *t = (x86_task_t *)runningTask->arch_info;
	    if (t->syscallRegs == NULL) {
		    t->syscallRegs = kmalloc(sizeof(registers_t));
		}
	    memcpy(t->syscallRegs,regs,sizeof(registers_t));
    }
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
    sti;	\
    call *%6; \
    cli;	\
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx; \
    pop %%ebx;" : "=a" (ret) : "r" (regs->esi), "r" (regs->edi), "r" (regs->ebx), "r"(regs->ecx),"r"(regs->edx),"r"(location));
    regs->eax = ret;
    return (uintptr_t)regs;
#endif
}
void arch_syscall_init() {
    interrupt_add(0x80,syscall_handler);
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
	// no legacy 32-bit support currently.
#if defined(__x86_64__)
	return hdr->e_machine == 62;
#else
    return hdr->e_machine == 3;
#endif
}
// Not used anymore.
uint64_t arch_getModuleAddress() {
    if (helinboot != NULL) {
	    // Yes, X86 now support HelinBoot boot protocol.
	    if (helinboot->moduleCount == 0) return 0;
	    return (int)helinboot->mod[0].begin;
    }
    if (info->mods_count == 0) return 0;
    multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr;
    return mod->mod_start;
}
uint64_t arch_getKernelEnd() {
    if (helinboot != NULL) {
	    if (helinboot->moduleCount == 0) {
		    return (uintptr_t)helinboot + (sizeof(MemoryMapEntry) * (helinboot->memoryMapCount + 1));
	    }
	    return (uintptr_t)helinboot->mod[helinboot->moduleCount-1].end;
    }
#if defined(__x86_64__)
    // Limine, use pretty hard calculations(no).
    // Get the virtual offset.
    uintptr_t virtualOffset = (uintptr_t)kernel_end;
    virtualOffset -= krnaddr_req.response->virtual_base;
    return krnaddr_req.response->physical_base + virtualOffset;
#else
    if (info->mods_count == 0) return (paddr_t)kernel_end;
    multiboot_module_t *mod = (multiboot_module_t *)info->mods_addr+info->mods_count-1;
    return mod->mod_end;
#endif
}
void arch_destroyContext(void *context) {
   kfree(context);
}
void arch_destroyArchStack(void *stack) {
#if defined(__x86_64__)
	x64_task_t *tsk = (x64_task_t *)stack;
	kfree((void *)tsk->rsp0_top);
	kfree((void *)tsk->userStackTop);
	kfree((void *)tsk->softIRQ_top);
	if (tsk->userModeRegs != NULL) {
		kfree(tsk->userModeRegs);
	}
	if (tsk->sigReturn != NULL) {
		kfree(tsk->sigReturn);
	}
	if (tsk->syscallRegs != NULL) {
		kfree(tsk->syscallRegs);
	}
	kfree(stack);
#else
    x86_task_t *task = (x86_task_t *)stack;
    if (task->kesp_start != 0) {
        if ((uintptr_t)task->kesp_start < KERN_HEAP_END) {
          kfree((void *)task->kesp_start);
        }
    }

    kfree((void *)task->stack);
    /*
     * User ESP pointer is automatically freed in arch_mmu_destroyAspace
    */
    if (task->argc != 0) {
	if ((uintptr_t)task->argv > KERN_HEAP_BEGIN && task->argv > KERN_HEAP_END) {
		kfree(stack);
		return;
	}
        kfree((void *)task->argv);
    }
    kfree(stack);
#endif
}
void arch_post_init() {
	// HelinBoot typically fit into one page.
	if (helinboot != NULL) {
		arch_mmu_mapPage(NULL,(vaddr_t)helinboot,(vaddr_t)helinboot,3);
	}
  // Check if CPU supports SSE and SSE2.
  uint32_t eax,ebx,ecx,edx;
  cpuid(0x1,&eax,&ebx,&ecx,&edx);
  if ((edx & EDX_SSE) == EDX_SSE) {
    // CPU supports SSE. Enable.
    uintptr_t cr0;
    uintptr_t cr4;
    // Read required registers.
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    // Enable SSE.
    cr0 &= ~(1<<2);
    cr0 |= (1<<1);
    cr4 |= (1<<9);
    cr4 |= (1<<10);
    // Write values back to make changes working.
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
  }
	symbols_init(info);
#if defined(__x86_64__)
	uintptr_t physAddr = (uintptr_t)&framebuffer_request;
	physAddr -= kernelVirtualOffset;
	physAddr = physicalOffset + physAddr;
	// set TSS64.
	arch_mmu_mapPage(NULL,(uintptr_t)&framebuffer_request,physAddr,3);
	physAddr = (uintptr_t)framebuffer_request.response;
	physAddr -= virtualOffset;
	arch_mmu_mapPage(NULL,(uintptr_t)framebuffer_request.response,physAddr,3);
	// Map framebuffer_request.response->framebuffers.
	physAddr = (uintptr_t)framebuffer_request.response->framebuffers - virtualOffset;
	arch_mmu_mapPage(NULL,(uintptr_t)framebuffer_request.response->framebuffers,physAddr,3);
	// Now map each element.
	for (uint64_t i = 0; i < framebuffer_request.response->framebuffer_count; i++) {
		physAddr = (uintptr_t)framebuffer_request.response->framebuffers[i] - virtualOffset;
		arch_mmu_mapPage(NULL,(uintptr_t)framebuffer_request.response->framebuffers[i],physAddr,3);
	}
	// Map the HHDM request
	physAddr = (uintptr_t)&hhdm_req;
	physAddr -= kernelVirtualOffset;
	physAddr = physicalOffset + physAddr;
	arch_mmu_mapPage(NULL,(uintptr_t)&hhdm_req,physAddr,3);
	// Map the response.
	physAddr = (uintptr_t)hhdm_req.response - virtualOffset;
	arch_mmu_mapPage(NULL,(uintptr_t)hhdm_req.response,physAddr,3);
	asm volatile("finit"); // initialize FPU.
#else
        // Initialize mem i/o region.
        arch_mmu_createTables(0xf0000000,0xff000000);
#endif
	//apic_init();
	//smp_init();
	//smp_post_init();
  //	acpiPostInit();
	kprintf("Initializing serial device\r\n");
  	serialdev_init();
	// Create initrd
	void *initrdAddr = NULL;
	int initrdSize = 0;
//	x86_serial();
	if (helinboot != NULL) {
		// Different boot protocol!
		if (helinboot->moduleCount == 0) return;
		ModuleInfo *mod = helinboot->mod;
		initrdAddr = (void *)mod->begin;
		initrdSize = mod->size;
		// Reserve the space;
		alloc_reserve((int)mod->begin,(int)mod->end+4096);
		// Reserve the memory where the structures are located.
		//alloc_reserve((int)helinboot,(int)helinboot+4096);
	} else {
#if defined(__x86_64__)
		if (modreq.response != NULL) {
			physAddr = (uintptr_t)&modreq;
			physAddr -= kernelVirtualOffset;
			physAddr = physicalOffset + physAddr;
			arch_mmu_mapPage(NULL,(uintptr_t)&modreq,physAddr,3);
			physAddr = (uintptr_t)modreq.response - virtualOffset;
			arch_mmu_mapPage(NULL,(uintptr_t)modreq.response,physAddr,3);
			struct limine_module_response *res = (struct limine_module_response *)modreq.response;
			if (res->module_count != 0) {
				// map First module then.
				physAddr = (uintptr_t)res->modules - virtualOffset;
				arch_mmu_mapPage(NULL,(uintptr_t)res->modules,physAddr,3);
				physAddr = (uintptr_t)res->modules[0];
				physAddr -= virtualOffset;
				arch_mmu_mapPage(NULL,(uintptr_t)res->modules[0],physAddr,3);
				kprintf("Module start address: 0x%x, kernel end: 0x%x\r\n",res->modules[0]->address-virtualOffset,&kernel_end);
				// Map the module.
				initrdSize = (int)res->modules[0]->size;
				initrdAddr = (void *)alloc_getPage();
				for (int i = 0; i < (initrdSize/4096)+1; i++) {
					alloc_getPage();
				}
				// Align it to 4KiB.
//				initrdAddr = (void *)((uintptr_t)((uintptr_t)initrdAddr + 4095) & ~4095);
				int pages = (initrdSize / 4096)+1;
				physAddr = (uintptr_t)res->modules[0]->address - virtualOffset;
				uintptr_t initrdVirtual = (uintptr_t)initrdAddr;
				for (int page = 0; page < pages; page++) {
					arch_mmu_mapPage(NULL,initrdVirtual,physAddr,3);
					physAddr+=4096;
					initrdVirtual+=4096;
				}
			}
		}
#else
		if (info->mods_count == 0) return;
		multiboot_module_t *initrdData = (multiboot_module_t *)info->mods_addr;
        	int size = initrdData->mod_end - initrdData->mod_start;
		initrdAddr = (void *)initrdData->mod_start;
		initrdSize = size;
#endif
	}
	if (initrdSize == 0 || initrdAddr == NULL) return;
	vfs_node_t *initrd = vfs_creat(vfs_getRoot(),"initrdram",0);
	rootfs_insertModuleData(initrd,initrdSize,(char *)initrdAddr);
	// After data insertion process, map the data itself.
	arch_mmu_map(NULL,(vaddr_t)initrdAddr,initrdSize,7);
#if !defined(__x86_64__)
	if (helinboot == NULL) {
		kprintf("Multiboot located at 0x%x, mmap: 0x%x, module: 0x%x, kernel start: 0x%x, initrd 0x%x, kernel_end: 0x%x\r\n",info,info->mmap_addr,info->mods_addr,(paddr_t)kernel_start,initrdAddr,kernel_end);
	}
#endif
  x86_smbios_init();
  x86_smbios_printInfo(0);
  //x86_smbios_printInfo(4);
}
bool arch_relocSymbols(module_t *mod,void *ehdr) {
#if defined(__x86_64__)
	return false;
#else
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
        s = (Elf32_Shdr *)((char *)e + e->e_shoff + e->e_shentsize * s->sh_link);
        const char *str = (char *) e + s->sh_offset;
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
							case R_386_GOTPC: {
                                                                // Check if this symbol isn't GOT.
                                                                char *aa = (char *)str + s->sh_name;
                                                                if (aa[0] == '_') {
                                                                        kprintf("Hm. Got Global Offset table. skip\n");
                                                                        continue;
                                                                }
								kprintf("R_386_GOTPC didn't supported!\n");
								return false;  // need to be added!
							} break;
							default:
							kprintf("Unknown relocation type!\n");
							break;
						}
					}
				}
			}
	}
	return true;
#endif
}
void arch_poweroff() {
    DEBUG_N("Warrning: Using unstable ACPI shutdown code!\r\n");
#if defined(__x86_64__)
    return;
#else
    acpiPowerOff();
#endif
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
#if defined(__x86_64__)
	kprintf("X86_64 Experemental Version Of HelinKern\r\n");
	kprintf("Used bootloader: Limine\r\n");
	kprintf("Detected CPU cores(taken from limine): %d cores\r\n",smp_cores);
#else
	kprintf("This OS is builded for X86 PC\r\n");
	kprintf("Is ACPI Shutdown Available: %s\r\n",(acpiIsOn() ? "Yes" : "No"));
	kprintf("Detected cores: %d\r\n",smp_getCPUCount());
	kprintf("Machine Name: %s\r\n",cpuName);
#endif
}
static void thread_main(uintptr_t entryPoint,int esp,bool isUser) {
    /* Через те, що наш метод переключення контексту процесів працює лише з задачами ядра, ми будемо перемикатися в режим користувача.
    */
#if defined(__x86_64__)
	process_t *prc = thread_getThread(thread_getCurrent());
	x64_task_t *tsk = (x64_task_t *)prc->arch_info;
	if (tsk->userModeRegs != NULL) {
		goto jump;
	}
	tsk->userModeRegs = kmalloc(sizeof(registers_t));
	memset(tsk->userModeRegs,0,sizeof(registers_t));
	tsk->userModeRegs->ss = 0x23;
	tsk->userModeRegs->cs = 0x1b;
	tsk->userModeRegs->ds = 0x23;
	tsk->userModeRegs->rip = entryPoint;
	tsk->userModeRegs->eflags = 0x200;
	if (tsk->argc > 0) {
		tsk->userModeRegs->rdi = tsk->argc;
		tsk->userModeRegs->rsi = (uint64_t)tsk->argv;
		tsk->userModeRegs->rdx = (uint64_t)tsk->environ;
	}
	uintptr_t rsp = (uintptr_t)sbrk(prc,70*4096);
	memset((void *)rsp,0,70*4096);
	uintptr_t aligned = rsp+(50*4096)-1;
	// Required by some software that USE SSE optimizations.
	aligned &= ~0x0F;
	aligned += 8;
	tsk->userModeRegs->useresp = aligned;
jump:
	x86_switch(tsk->userModeRegs);
#else
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
    int stackSize = 70*4096;
    int _esp = (int)sbrk(prc,stackSize);
    //if (isUser) arch_mmu_map(NULL,_esp,stackSize,7 | 0x00000200);
    //int _esp = (int)kmalloc(4096);
    x86_task_t *archStack = (x86_task_t *)prc->arch_info;
    archStack->userESP = _esp;
    // clean memory
    memset((void *)_esp,0,stackSize);
    uint32_t stack = (uint32_t )_esp+stackSize;
    stack-=1;
    stack &= ~0x0F;
    stack += 8;
    archStack->userESP_top = stack;
    PUSH(stack,char **,(char **)(uintptr_t)archStack->environ);
    PUSH(stack,char **,(char **)(uintptr_t)archStack->argv);
    PUSH(stack,int,archStack->argc);
    x86_jumpToUser(entryPoint,(int)stack);
#endif
}
void arch_putArgs(process_t *prc,int argc,char **argv,char **environ) {
#if defined(__x86_64__)
	// Just pass it as standard arguments.
	x64_task_t *tsk = (x64_task_t *)prc->arch_info;
	tsk->argc = argc;
	tsk->argv = argv;
	tsk->environ = environ;
#else
    if (prc == NULL || argv == 0) return;
    x86_task_t *s = (x86_task_t *)prc->arch_info;
    s->argc = argc;
    s->argv = (int)argv;
    s->environ = (int)environ;
#endif
}
struct stackframe {
#if !defined(__x86_64__)
  struct stackframe* ebp;
  uint32_t eip;
#else
  struct stackframe *rbp;
  uint64_t rip;
#endif
};
void arch_trace(uintptr_t stackPtr) {
#if defined(__x86_64__)
	if (syscall_regs != NULL) {
		// Dump CPU state before the panic.
		kprintf("CPU State Before panic:\r\n");
		kprintf("RIP: 0x%x   RAX: 0x%x\r\n" \
				"RBX: 0x%x   RCX: 0x%x\r\n" \
				"RDX: 0x%x   RSI: 0x%x\r\n" \
				"RDI: 0x%x   RBP: 0x%x\r\n" \
				"R8: 0x%x    R9: 0x%x\r\n" \
				"R10: 0x%x   R11: 0x%x\r\n" \
				"R12: 0x%x   R13: 0x%x\r\n" \
				"R14: 0x%x   R15: 0x%x\r\n",
				syscall_regs->rip,syscall_regs->rax,
				syscall_regs->rbx,syscall_regs->rcx,
				syscall_regs->rdx,syscall_regs->rsi,
				syscall_regs->rdi,syscall_regs->rbp,
				syscall_regs->r8,syscall_regs->r9,
				syscall_regs->r10,syscall_regs->r11,
				syscall_regs->r12,syscall_regs->r13,
				syscall_regs->r14,syscall_regs->r15);
	}
	struct stackframe *stk;
	asm("mov %%rbp, %0" : "=r"(stk) ::);
	if (stackPtr != NULL) {
		stk = (struct stackframe *)stackPtr;
	}
	for (int fr = 0; fr < 10; fr++) {
		kprintf("RIP at 0x%x in %s\r\n",stk->rip,symbols_findName(stk->rip));
		if ((uintptr_t)stk->rbp < 0x100000) break;
		stk = stk->rbp;
	}
#else
    if (runningTask != NULL) {
    	kprintf("Last exextued task was %s(%d)\n",runningTask->name,runningTask->pid);
    }
    struct stackframe *stk;
    asm ("movl %%ebp,%0" : "=r"(stk) ::);
    for(unsigned int frame = 0; stk && frame < 10; ++frame) {
	    if ((uintptr_t)stk > KERN_HEAP_END) break;
        kprintf("0x%x in %s\r\n",stk->eip,symbols_findName(stk->eip));
        stk = stk->ebp;
    }
#endif
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
    uintptr_t cr4;
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    kprintf("CR4 before: 0x%x\n", cr4);
    cr4 |= (1 << 10);
    asm volatile ("mov %0, %%cr4" :: "r"(cr4));
    kprintf("CR4 set, current value: 0x%x\n", cr4);

    set_fpu_cw(895); // Set the FPU control word to 0x037F in decimal format

    // Enable FPU exceptions
    uintptr_t cr0;

    // Read the current value of CR0
    asm volatile("mov %%cr0, %0" : "=r" (cr0));

    // Set the NE flag (bit 5) to 1
    cr0 |= (1 << 5);

    // Write the modified value back to CR0
    asm volatile("mov %0, %%cr0" :: "r" (cr0));

    asm volatile("finit");
}

uintptr_t arch_syscall_getCallerRet() {
	if (syscall_regs != NULL) {
        //kprintf("%s: caller userESP: 0x%x, ESP: 0x%x\n",__func__,syscall_regs->useresp,0);
#if defined(__x86_64__)
		return syscall_regs->rip;
#else
		return syscall_regs->eip;
#endif
	}
	return NULL;
}
void arch_forkProcess(process_t *parent,process_t *child) {
#if defined(__x86_64__)
	x64_task_t *t = (x64_task_t *)child->arch_info;
	// Remember son, we are running in newly created process.
	t->userModeRegs = kmalloc(sizeof(registers_t));
	memcpy(t->userModeRegs,syscall_regs,sizeof(registers_t));
	t->userModeRegs->rax = 0;
#else
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
#endif
}

char *arch_getName() {
#if defined(__x86_64__)
	return "x86_64";
#else
    return "x86";
#endif
}

void userModeEntry() {
    return; // nothing here
}

void arch_switchToUserMode() {
	// We can use x86_jumpToUser
#if defined(__x86_64__)
	return;
#else
    int esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    x86_jumpToUser((int)userModeEntry,esp);
#endif
}

/* Oh.....the fucking signals..... */
/* first ever long name of function */
// Define the fucking process signal handler inside the kernel, because we don't want to distrub the libc, that may not even prepared
// for this.
typedef void (*sigh)(int);
static void i686_sigreturn(int sig) {
	// It's time to do some magic!!!!
	// Використаємо асемблерну мову, адже це єдиний шлях зробити те що потрібно
#if defined(__x86_64__)
	uintptr_t r = 0;
	int ret = 0;
	asm volatile("movq %%r12, %0" : "=r"(r));
	asm volatile("int %1;\n" : "=a"(ret) : "i"(0x80), "a"(12),"d"(r));
#else
	int ret = 0;
	asm volatile("int %1;\n" : "=a"(ret) : "i"(0x80), "a"(12),"d"(sig));
#endif
}
static void i686_userHandler() {
	// Регістр ECX містить номер сигналу(EAX перезаписується цим кодом, а ECX ні)
	// Регістр EDX містить адресу самого обробника у користувацькій програмі.
	// Нам потрібно збільшити вказівник стеку рівно на 8КіБ інакше у нас буде пошкодження стеку.
	// This is well be abolute mess....
  // Please don't decrease the size of bytes that we add to stack pointer here,
  // otherwise this corrupt the whole kernel memory!
#if defined(__x86_64__)
	asm volatile("addq $8192,%rsp");
#else
	//asm volatile("addl $4100,%esp");
#endif
	// Make sure we working right address space.
	// Все, тепер ми спокійно можемо модифікувати стек(він все одно перезапишеться після поввернення)
	uintptr_t signum = 0;
	uintptr_t to = 0;
	// Запишемо дані з регістрів у наші константи.
	ProcessSignal *sig = NULL;
#if defined(__x86_64__)
	asm volatile("mov %%rdi, %0" : "=r"(signum));
#else
	asm volatile("movl %%ecx, %0" : "=r"(signum));
#endif
	x86_task_t *tsk = runningTask->arch_info;
	asm volatile("movl %0, %%esp" : : "r"(tsk->kernelESP-2048));
  // Part of ARCH processing signal code.
	sig = &runningTask->signal_handlers[signum];
        if (sig->handler == 0 && signum == 1) {
                thread_killThread(thread_getThread(thread_getCurrent()),-signum,true);
        } else if (sig->handler == 0) {
                x86_switch(sig->returnStack);
        }
        registers_t *ra = (registers_t *)sig->returnStack;
	uintptr_t st = ra->useresp;
        // Check if we HAVE alternative stack for processing.
        if ((sig->flags & 2) == 2) {
                // WE DO.
                st = (uintptr_t)runningTask->sigStack->stack;
        }
	PUSH(st,uintptr_t,signum);
	PUSH(st,uintptr_t,signum);
	PUSH(st,uintptr_t,(uintptr_t)i686_sigreturn);
	// Now. A little bit of strange design.
	/*
	 * So. Because our registers_t is tempararly data, it can be overwritten by
	 * signal handler without any issues.
	*/
	registers_t *reg = (registers_t *)sbrk(runningTask,sizeof(registers_t));
	memset(reg,0,sizeof(registers_t));
#if !defined(__x86_64__)
	reg->eip = sig->handler;
	reg->useresp = st;
	reg->cs = 0x1b;
	reg->ds = reg->ss = 0x23;
#else
	reg->rip = sig->handler;
	// SystemV AMD64 Calling convertion.
	reg->rsi = signum;
	reg->rdi = reg->rsi;
	reg->r12 = reg->rsi;
	reg->cs = 0x1b;
	reg->ds = reg->ss = 0x23;
	reg->useresp = st;
#endif
	// Викличемо реальний обробник.
	// Jump using assembler.
        x86_switch(reg);
}
void arch_processSignal(process_t *nextTask,ProcessSignal handler,...) {
    // Extract the signal number
    arch_cli();
    va_list list;
    va_start(list,handler);
    int num = va_arg(list,int);
    va_end(list);
#if !defined(__x86_64__)
    x86_task_t *task = (x86_task_t *)nextTask->arch_info;
    //kprintf("Copying\n");
    bool c = false;
    if (handler.returnStack == NULL) {
	    handler.returnStack = kmalloc(sizeof(registers_t));
    } else {
            c = true;
    }
    // Update handler?
    nextTask->signal_handlers[num] = handler;
    if (!c || handler.exited) {
        memcpy(handler.returnStack,task->syscallRegs,sizeof(registers_t));
    }
    task->userTaskRegisters->eip = (uintptr_t)i686_userHandler;
    task->userTaskRegisters->ecx = num;
    task->userTaskRegisters->edx = (uintptr_t)&handler;
    //kprintf("Task changed. Switching\n");
#else
    // x86_64 code begins here
    x64_task_t *task = (x64_task_t *)nextTask->arch_info;
    //kprintf("task = 0x%x, task->sigReturn = 0x%x, userModeRegs = 0x%x, syscall regs -> 0x%x\n",task,task->sigReturn,task->userModeRegs,task->syscallRegs);
    if (handler.returnStack == NULL) {
	    handler.returnStack = kmalloc(sizeof(registers_t));
    }
    nextTask->signal_handlers[num] = handler;
    if (task->syscallRegs == NULL) {
	    // What? Okay. Change the fucking userModeRegs.
	    kprintf("Serious kernel bug!!!!!! OR the process in endless loop and DOESN'T call any kind of syscall or the KERNEL HAS BUG!");
	  return;
    }
    memcpy(handler.returnStack,task->syscallRegs,sizeof(registers_t));
    task->userModeRegs->rdi = (uint64_t)num;
    task->userModeRegs->rip = (uintptr_t)i686_userHandler;
#endif
}
void arch_exitSignal(process_t *prc,ProcessSignal *to) {
#if !defined(__x86_64__)
	if (to->returnStack == NULL) return;
	//memcpy(task->userTaskRegisters,task->signalReturn,sizeof(registers_t));
	x86_switch(to->returnStack);
#else
	if (to->returnStack == NULL) {
		kprintf("Impossible to process signal return if the signal never processed\r\n");
		return;
	}
	x86_switch(to->returnStack);
#endif

}
#if defined(__x86_64__)
void arch_fpu_save(void *ptr) {
	if (!ptr) return;
	// Use inline asm.
	asm volatile("fxsave (%0)" : : "r"(ptr));
}
void arch_fpu_restore(void *ptr) {
	if (!ptr) return;
	asm volatile("fxrstor (%0)" : : "r" (ptr));
}
extern void x86_switch(registers_t *to);
#endif
uintptr_t arch_getMemStart() {
#if !defined(__x86_64__)
	uintptr_t addr = (uintptr_t)arch_getKernelEnd();
	return (addr + 4095) & ~4095;
#else
	// Limine will boot us up in end of available memory i guest, soo.
	return (uintptr_t)0x100000;
#endif
}
/*
 * arch_clone_current:
 * Clone current process context only and prepare new usable stack for it.
 * The stack copy and alignment is required for proper thread functionallity.
 * Otherwise we got stack corruption
 *
*/
void arch_clone_current(process_t *prc,void *stackPtr,uintptr_t stackSize) {
	if (prc->started) return; // we can't process already running process.
#if defined(__x86_64__)
	x64_task_t *tsk = (x64_task_t*)prc->arch_info;
#else
	x86_task_t *tsk = (x86_task_t*)prc->arch_info;
#endif
	// Create new registers, so the thread_main will think that we return from fork.
	registers_t *regs = (registers_t*)kmalloc(sizeof(registers_t));
	memcpy(regs,syscall_regs,sizeof(registers_t));
	// Check if the stack ptr or stack size isn't zero.
	uintptr_t copyAddr = syscall_regs->useresp & ~0xfff;
	uintptr_t offset = syscall_regs->useresp - copyAddr;
	if (stackPtr == NULL || stackSize == 0) {
		// Allocate new stack.
		uintptr_t rsp = (uintptr_t)sbrk(prc,50*4096);
		memset((void *)rsp,0,50*4096);
		regs->useresp = rsp;
	} else {
		regs->useresp = (uintptr_t)stackPtr;
	}
	// Copy the stack otherwise we get em, stack corruption?
	uintptr_t dstAddr = (uintptr_t)regs->useresp;
	memcpy((void *)dstAddr,(void *)copyAddr,50*4096);
	regs->useresp = dstAddr + offset;
#if defined(__x86_64__)
	regs->rax = 0;
	tsk->userModeRegs = regs;
#else
	regs->eax = 0;
	tsk->forkESP = (int)regs;
#endif
}
void arch_init_drivers() {
    keyboard_init();
    fbdev_init();
    ps2mouse_init();
    pci_init();
    mbr_init();
    //ahci_init();
    //fat_init();
    ext2_main();
    iso9660_init();
    atapi_init();
    amdnet_init();
    amdgpu_init();
    //usb_ehci_init();
    ac97_init();
}
