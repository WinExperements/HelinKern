#include <arch.h>
#include <arch/elf.h>
extern char kernel_end[];
// Memory configuration
int KERN_HEAP_BEGIN = 0x0; // need to be fixed as soon we add MMU support
int KERN_HEAP_END = 0x40000000; // it can be any address
int USER_OFFSET = 0;
int USER_MMAP_START = 0;
int MEMORY_END = 0xFFC00000; // Every SOC have other value
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
void *arch_prepareContext(int entry,bool isUser) {}
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
void arch_trace() {}
void arch_putArgs(process_t *prc,int argc,char **argv) {}
