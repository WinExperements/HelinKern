#ifndef ARCH_H
#define ARCH_H
#include <dev/fb.h>
#include <typedefs.h>
#include <module.h>
#include <thread.h>
void arch_entry_point(void *arg);
void arch_pre_init();
/*
    Any arch must setup there component here:
    - Interrupts
    - Timer
*/
void arch_init();
void arch_cli();
void arch_sti();
void arch_reset();
void arch_poweroff();
bool arch_getFBInfo(fbinfo_t *info);
int arch_getMemSize();
void arch_switchContext(void *prSt);
void *arch_prepareContext(int entry,bool isUser);
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
void arch_sysinfo();
void arch_putArgs(process_t *prc,int argc,char **argv);
void arch_trace();
void arch_preapreProcess(process_t *prc);
void arch_fpu_save(void *to);
void arch_fpu_restore(void *from);
/* Actually get syscall caller return address */
int arch_syscall_getCallerRet();
void arch_forkProcess(process_t *parent,process_t *child);
#endif
