#include <arch.h>
#include <arch/elf.h>
extern char kernel_end[];
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
    return true;
}
void arch_pre_init() {}
void arch_init() {}
void arch_idleLoop() {while(1) {}}
void arch_cli() {}
void arch_sti() {}
void arch_reset() {}
void arch_poweroff() {}
bool arch_getFBInfo(fbinfo_t *info) {
    return false;
}
int arch_getMemSize() {
    return 128*1024*1024;
}
void arch_switchContext(void *prSt) {}
void *arch_prepareContext(int entry) {}
void *arch_preapreArchStack(bool isUser) {}
void arch_syscall_init() {}
int arch_getModuleAddress() {return 0;}
int arch_getKernelEnd() {return kernel_end;}
void arch_destroyContext(void *context) {}
void arch_destroyArchStack(void *stack)  {}
void arch_post_init() {}
bool arch_relocSymbols(module_t *mod,void *ehdr) {}
void arch_reschedule() {}
/* Print all CPU information */
void arch_detect() {}
void arch_sysinfo() {}
