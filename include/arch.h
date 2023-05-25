#ifndef ARCH_H
#define ARCH_H
#include <dev/fb.h>
#include <typedefs.h>
#include <module.h>
void arch_entry_point(void *arg);
void arch_pre_init();
/*
    Any arch must setup there component here:
    - Interrupts
    - Timer
*/
void arch_init();
void arch_idleLoop();
void arch_cli();
void arch_sti();
void arch_reset();
void arch_poweroff();
bool arch_getFBInfo(fbinfo_t *info);
int arch_getMemSize();
void arch_switchContext(void *prSt);
void *arch_prepareContext(int entry);
void *arch_preapreArchStack(bool isUser);
void arch_syscall_init();
int arch_getModuleAddress();
int arch_getKernelEnd();
void arch_destroyContext(void *context);
void arch_destroyArchStack(void *stack);
void arch_post_init();
bool arch_relocSymbols(module_t *mod,void *ehdr);
void arch_reschedule();
/* Print all CPU information */
void arch_detect();
#endif
