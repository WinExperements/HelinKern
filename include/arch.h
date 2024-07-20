#ifndef ARCH_H
#define ARCH_H
#include <dev/fb.h>
#include <typedefs.h>
#include <module.h>
#include <thread.h>
/* Arch specific entry point */
void arch_entry_point(void *arg);
/* Arch pre-init code, for platforms where need to do some things before actual platform init */
void arch_pre_init();
/*
    Any arch must setup there component here:
    - Interrupts
    - Timer
*/
void arch_init();
/* Disable interrupts */
void arch_cli();
/* Enable interrupts */
void arch_sti();
/* Reboot target architecture */
void arch_reset();
/* Powero ff target architecture */
void arch_poweroff();
/* Get framebuffer information(if available). Return NULL if platform doesn't support it, if NULL returned, kernel fallback to the serial console */
bool arch_getFBInfo(fbinfo_t *info);
/* Get total amount of installed memory */
uintptr_t arch_getMemSize();
/* Switch context of process */
void arch_switchContext(void *prSt);
/* Prepare process context */
void *arch_prepareContext(void* entry,bool isUser);
/* Prepare arch specific stack(for user or kernel process, required for architectures where need to store some additional architecture specific information for conext switch, syscall handler or something) */
void *arch_preapreArchStack(bool isUser);
/* Init syscall interrupt handler. When the syscall interrupt was rised, the arch code must check if the syscall number isn't out of bounds, call syscall_get to get handler address, and pass using specific args convertion for specific architecture to pass the arguments for handler.*/
void arch_syscall_init();
/* Get the first module address */
uint64_t arch_getModuleAddress();
/* Get kernel end address in memory(required to init the PMM) */
uint64_t arch_getKernelEnd();
/* Destroy the context information) */
void arch_destroyContext(void *context);
/* Destroy arch specific context for process */
void arch_destroyArchStack(void *stack);
/* Post initializing of target platform */
void arch_post_init();
/* Arch specific relocation of modules symbols code */
bool arch_relocSymbols(module_t *mod,void *ehdr);
/* Raise software timer interrupt */
void arch_reschedule();
/* Print all CPU information */
void arch_detect();
/* Arch specific platform information code, used for debugging */
void arch_sysinfo();
/* Put user space process arguments for target architecture */
void arch_putArgs(process_t *prc,int argc,char **argv,char **environ);
/* trace panic addresses */
/* 0 in stackPtr means current stack */
void arch_trace(uintptr_t stackPtr);
/* Prepare arch specific information for process, or newly created stack, whatever required for target architecture, can be empty body function */
void arch_prepareProcess(process_t *prc);
/* Save FPU context for target process(if available) */
void arch_fpu_save(void *to);
/* Restore FPU context for target process(if available) */
void arch_fpu_restore(void *from);
/* Actually get syscall caller return address */
uintptr_t arch_syscall_getCallerRet();
/* Arch specific code for forking processes */
void arch_forkProcess(process_t *parent,process_t *child);
/* Get architecture name */
char *arch_getName();
/* Switch to user mode */
void arch_switchToUserMode();
/* Arch code to call the user mode address with saved stack pointer.
 * REQUIRE:
 * The kernel will pass arguments into the function, arch code NEED to PUSH the arguments to stack OR using the arch specs load the arguments for passed function address.
 * REQUIRED BY:
 * POSIX signals implementation.
*/
void arch_processSignal(process_t *prc,uintptr_t address,...);
void arch_exitSignal(process_t *prc);
/* Soooo 😔, this function is required only if the kernel is loaded not at the start of available memory.
 * If it starts in start of free memory then return kernel end adddress otherwise
 * return address when the allocation of physical memory can actually begin.
*/
uintptr_t arch_getMemStart();
#endif
