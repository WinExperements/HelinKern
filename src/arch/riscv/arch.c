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
extern void riscv_mtrap_handler();
int riscv_trap_handler(int returnTo,int cause,void *regs);
int intStack_ptr = 0;
void arch_entry_point(void *arg) {
	// RISC-V Bootloader currently doesn't support helinboot protocol.
	kernel_main("-v");
}
void arch_pre_init() {}
void arch_init() {
	// Setup interrupts.
  // Currently we work in machine mode.
  int mie;
  asm volatile("csrr %0, mie" : "=r"(mie));
  // Enable e-call from user-mode and e-call from machine mode.
  mie |= (1<<8);
  mie |= (1<<11);
  asm volatile("csrw mie, %0" : : "r"(mie));
  // Load machine mode trap handler
  asm volatile("csrw mtvec, %0" : : "r"((int)riscv_mtrap_handler));
  kprintf("Kernel end: 0x%x\r\n",(int)kernel_end);
  intStack_ptr = (int)kernel_end+100;
  asm volatile("csrw mscratch, %0" : : "r"(intStack_ptr));
  asm volatile("ecall");
}
void arch_cli() {
	
}
void arch_sti() {
	
}
void arch_reset() {
	
}
void arch_poweroff() {
	
}
bool arch_getFBInfo(fbinfo_t *info) {
	return false;
}
int arch_getMemSize() {
  arch_detect();
  return 0;
}
void arch_switchContext(void *prSt) {
	// We need to provide MMU support before context switch.
}
void *arch_prepareContext(int entry,bool isUser) {
	return NULL;
}
void *arch_preapreArchStack(bool isUser) {
	return NULL;
}
void arch_syscall_init() {}
int arch_getModuleAddress() {
	// The bootloader doesn't support it :(
	return 0;
}
int arch_getKernelEnd() {
	return (int)kernel_end;
}
void arch_destroyContext(void *context) {
}
void arch_destroyArchStack(void *stack) {
}
void arch_post_init() {
	
}
bool arch_relocSymbols(module_t *mod,void *ehdr) {
	return false;
}
void arch_reschedule() {
}
/* Print all CPU information */
void arch_detect() {
  kprintf("Detected ISA: ");
  int misa;
  asm volatile("csrr %0, misa" : "=r"(misa));
  // Check bits.
  int bits = (misa >> 30);
  if (bits == 1) {
    kprintf("RV32");
  } else if (bits == 2) {
    kprintf("RV64");
  } else if (bits == 3) {
    kprintf("RV128");
  }
  if ((misa & (1<<4)) == (1<<4)) {
    kprintf("I");
  } else if ((misa & (1<<12)) == (1<<12)) {
    kprintf("M");
  } else if ((misa & (1<<18)) == (1<<18)) {
    kprintf("S");
  } else if ((misa & (1<<20)) == (1<<20)) {
    kprintf("U");
  }
  kprintf("\r\n");
}
void arch_sysinfo() {
	// This method not even is called anymore in the kernel, so it's useless now.
}
void arch_putArgs(process_t *prc,int argc,char **argv) {
}
void arch_trace() {
	kprintf("RISC-V Support is experemental.\r\n");
}
void arch_prepareProcess(process_t *prc) {
}
void arch_fpu_save(void *to) {
	// How to enable FPU on this thing?
}
void arch_fpu_restore(void *from) {
	// Who are you?
}
/* Actually get syscall caller return address */
int arch_syscall_getCallerRet() {
	return 0;
}
void arch_forkProcess(process_t *parent,process_t *child) {
}
char *arch_getName() {
	return "riscv32";
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
	if (hdr == NULL) return false; // buggy code maybe
	return false;
}
/* New HelinOS functions */
void arch_processSignal(process_t *prc,int address,...) {}
void arch_exitSignal(process_t *prc) {}
int riscv_trap_handler(int returnTo,int cause,void *regs) {
  kprintf("RISC-V32 Support module catch an interrupt!\r\n");
  kprintf("IRQ NO: %d\r\n",cause);
  returnTo = returnTo+4;
  // Before return we need to somehow populate the t6 register back.
  // Any kind of malicious software can rewrite pointer to intStack_ptr.
  intStack_ptr = (int)kernel_end + 100;
  return returnTo;
}
