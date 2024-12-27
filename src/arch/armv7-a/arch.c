#include <arch.h>
#include <arch/elf.h>
#include <helinboot.h> // yes, our specific boot description mechanism.
#include <output.h>
#include <dev/fb.h>
extern char kernel_end[];
// Memory configuration
int KERN_HEAP_BEGIN = 0x0; // need to be fixed as soon we add MMU support
int KERN_HEAP_END = 0x40000000; // it can be any address
int USER_OFFSET = 0;
int USER_MMAP_START = 0;
int MEMORY_END = 0xFFC00000; // Every SOC have other value
static kernelInfo *bootDesc;
extern char kernel_end[]; // linker.ld!!!
extern char _binary_font_psf_start;
extern char _binary_font_psf_end;
extern int disableOutput;
// Let's port the kernel to the ARMv7A MSM8909
// There will be a lot of #ifdef and something simular. Yes, ARM requires to port kernel to manyy machines.
void ClearScreenWith(uint32_t *fb,int startX,int startY,int w,int h,int color) {
	for (int y = startY; y < h; y++) {
		for (int x = startX; x < w; x++) {
			fb[x + y * 720] = color;
		}
	}
}

void arch_entry_point(void *arg) {
	// Hahaha, we now have the fricking custom bootloader
	#ifdef HELINBOOT_USED
	bootDesc = (kernelInfo *)arg;
	if (bootDesc->magic != HELINBOOT_KERNINFO_MAGIC) {
		// Why you trying to boot this kernel by non compatable bootloader?
		return;
	}
	#endif
	#ifdef QEMUBOARD
	disableOutput = false;
	#endif
	kernel_main("-v");
}
void arch_pre_init() {}
void arch_init() {
	kprintf("HelinOS ARMv7-A architecture init code, builded by GCC: ");
	kprintf(__VERSION__);
	kprintf("\r\n");
}
void arch_cli() {
	// Stop!
}
void arch_sti() {
	// Now you can go
}
void arch_reset() {
	// Every platform has it's own method to reboot itself. We maybe use the EFI services on some Lumia devices to do this?
}
void arch_poweroff() {
	// Analogichno z arch_reset
}
bool arch_getFBInfo(fbinfo_t *info) {
	#ifdef HELINBOOT_USED
	if (bootDesc == NULL) return false;
	if (bootDesc->framebufferWidth == NULL) return false;
	info->paddr = (void *)bootDesc->framebufferAddress;
	info->vaddr = info->paddr;
	info->width = (int)bootDesc->framebufferWidth;
	info->height = (int)bootDesc->framebufferHeight;
	info->bpp = 32;
	// Math!
	info->pitch = 2880;
	return true;
	#else
	return false;
	#endif
}
uintptr_t arch_getMemSize() {
#ifdef HELINBOOT_USED
	// Nah, okay, the kernel supports a ton of boot methods, so this is why it's here.
	// By the realization, we need....
	uint64_t mostBigChunk;
	MemoryMapEntry *list = bootDesc->mmap_start;
	if (list->magic != HELINBOOT_MMAP_MAGIC) {
		// What??
		return -1;
	}
	for (int i = 0; i < bootDesc->memoryMapCount; i++) {
		MemoryMapEntry ent = list[i];
		if (ent.magic != HELINBOOT_MMAP_MAGIC) {
			continue;
		}
		if (ent.type == 7 && ent.size > mostBigChunk) {
			mostBigChunk = ent.size;
		}
	}
	return mostBigChunk;
	#else
	return 20*1024;
	#endif
}
void arch_switchContext(void *prSt) {
	// We don't have a MMU......how we able even have multitasking on this platform?
}
void *arch_prepareContext(void *entry,bool isUser) {
	// i don't listen youuuu
	return NULL;
}
void *arch_preapreArchStack(bool isUser) {
	// No, i don't prepare the stack for you
	return NULL;
}
void arch_syscall_init() {}
uint64_t arch_getModuleAddress() {
	// The bootloader doesn't support it :(
	return 0;
}
uint64_t arch_getKernelEnd() {
	return (uint64_t)(uint32_t)&kernel_end;
}
void arch_destroyContext(void *context) {
	// I SAY: Destroy yourself! Watch, he doesn't listen me....so i can't do it
}
void arch_destroyArchStack(void *stack) {
	// This package doesn't even support MMU on this platform
}
void arch_post_init() {
	// Post init...hmm.. on qualcomm it's must call something like msm_ method
}
bool arch_relocSymbols(module_t *mod,void *ehdr) {
	// Relocate by yourself, please
	return false;
}
void arch_reschedule() {
	// Abra kadabra!
}
/* Print all CPU information */
void arch_detect() {
	kprintf("Some ARMv7-A CPU detected\r\n");
}
void arch_sysinfo() {
	// This method not even is called anymore in the kernel, so it's useless now.
}
void arch_putArgs(process_t *prc,int argc,char **argv,char **environ) {
	// Bereh i kladeh tudi, cho ne zrozymilo?
}
void arch_trace(uintptr_t t) {
	kprintf("Buggy platform: Just a ARMv7-A device, i don't know really what this platform is even is\r\n");
}
void arch_prepareProcess(process_t *prc) {
	// Please, prepare it.
	// TODO: Leave this funny for you comments and write actuall code
}
void arch_fpu_save(void *to) {
	// How to enable FPU on this thing?
}
void arch_fpu_restore(void *from) {
	// Who are you?
}
/* Actually get syscall caller return address */
uintptr_t arch_syscall_getCallerRet() {
	// It's hard to explain why i am lazzy to implement this.....
	return 0;
}
void arch_forkProcess(process_t *parent,process_t *child) {
	// Em...
}
char *arch_getName() {
	return "armv7-a";
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
	// Check if this ELF is 32-bit, has no EABI and is for ARMv7-A
	if (hdr == NULL) return false; // buggy code maybe
	// nah, potim
	return false;
}
void arch_processSignal(process_t *prc,ProcessSignal handler,...) {}
void arch_exitSignal(process_t *prc,ProcessSignal *to) {}
uintptr_t arch_getMemStart() {return 0;}
void arch_clone_current(process_t *prc,void *newStack,uintptr_t stackSize) {}
