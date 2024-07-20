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
extern char kernel_end[]; // linker.ld!!!
extern char _binary_font_psf_start;
extern char _binary_font_psf_end;
extern int disableOutput;
void arch_entry_point(void *arg) {
	// Hahaha, we now have the fricking custom bootloader
	kernel_main("-v");
}
void arch_pre_init() {}
void arch_init() {
	disableOutput = false;
	kprintf("HelinOS Headless architecture init code, builded by GCC: ");
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
	return false;
}
int arch_getMemSize() {
  return 0; // Headless platform meaning we don't have anything, at all.
}
void arch_switchContext(void *prSt) {
	// We don't have a MMU......how we able even have multitasking on this platform?
}
void *arch_prepareContext(void* entry,bool isUser) {
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
	return (uint64_t)kernel_end;
}
void arch_destroyContext(void *context) {
	// I SAY: Destroy yourself! Watch, he doesn't listen me....so i can't do it
}
void arch_destroyArchStack(void *stack) {
	// This package doesn't even support MMU on this platform
}
void arch_post_init() {
	
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
}
void arch_sysinfo() {
	// This method not even is called anymore in the kernel, so it's useless now.
}
void arch_putArgs(process_t *prc,int argc,char **argv,char **environ) {
	// Bereh i kladeh tudi, cho ne zrozymilo?
}
void arch_trace() {
	kprintf("Headless platform doesn't have any kind of trace function.");
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
	return "headless";
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
	if (hdr == NULL) return false; // buggy code maybe
	// nah, potim
	return false;
}
/* New HelinOS functions */
void arch_processSignal(process_t *prc,uintptr_t address,...) {}
void arch_exitSignal(process_t *prc) {}
