#include <arch.h>
#include <arch/elf.h>
#include <helinboot.h> // yes, our specific boot description mechanism.
#include <output.h>
#include <dev/fb.h>
#include <arch/riscv/opensbi.h>
#include <thread.h>
#include <mm/alloc.h>
#include <arch/riscv/irq.h>
#include <arch/riscv/dev/serial.h>
#include <fw/dtb.h>
#include <stddef.h>
extern char kernel_end[];
// Memory configuration
void *_binary_font_psf_start = NULL;
void *_binary_font_psf_end = NULL;
int KERN_HEAP_BEGIN = 0x0; // need to be fixed as soon we add MMU support
int KERN_HEAP_END = 0x40000000; // it can be any address
int USER_OFFSET = 0;
int USER_MMAP_START = 0;
int MEMORY_END = 0xFFC00000; // Every SOC have other value
extern char kernel_end[]; // linker.ld!!!
extern int disableOutput;
extern void riscv_mtrap_handler();
static int *intStack = NULL;
static int softIRQ_req = 0;
static uintptr_t softIRQ_addr = 0;
// Exception names
static char *exception_name[] = {
	"Instruction address misaligned",
	"Instruction access fault",
	"Illegal instruction",
	"Breakpoint",
	"Load address misaligned",
	"Load access fault",
	"Store/AMO address misaligned",
	"Store/AMO access fault",
	"Environment call from U-mode",
	"Environment call from S-mode",
	"Reserved",
	"Reserved11",
	"Instruction page fault",
	"Load page fault",
	"Reserved14",
	"Store/AMO page fault",
	"Reserved16",
	"Reserved17",
	"Software check",
	"HW Error",
};

struct SbiRet set_timer_in_near_future() {
  struct SbiRet return_status;
  
  asm(
      "rdtime t0\n\t" // Get the current time.
      "li t1, 10000\n\t" // Get something to add to the current time, so we have a future time.
      "add a0, t0, t1\n\t" // Calculate the time in the future.
      "li a7, 0x54494D45\n\t"
      "li a6, 0x00\n\t"
      "ecall\n\t"
      "mv %0, a0\n\t"
      "mv %1, a1\n"
      : /* Outputs: */ "=r"(return_status.error), "=r"(return_status.value)
      : /* Inputs: none */
      : /* Clobbered registers: */ "a0", "a1", "a6", "a7", "t0", "t1"
      );
  return return_status;
}

void arch_entry_point(void *arg) {
	// Nothing currently.
	disableOutput = false;
	struct fdt_header *hdr = (struct fdt_header *)arg;
	kprintf("hdr->magic = 0x%x\n",hdr->magic);
	kernel_main("-v");
}
void arch_init() {
	kprintf("%d\n",offsetof(registers_t,spp));
	asm("csrw stvec, %0\n"
			"li t1,32\n"
			"csrs sie,t1\n"
			"li t1,2\n"
			"csrs sie,t1\n"
			"li t1, 0x40000\n"
			"csrs sstatus,t1\n"	// enable SUM.
			: : "r"((uintptr_t)riscv_mtrap_handler)
			: "t1");
	uint64_t sstatus = 0;
	asm("csrr %0, sstatus" : "=r"(sstatus));
	int sum = (sstatus >> 18) & 0x1;
	kprintf("sum => %d\n",sum);
	set_timer_in_near_future();
}
void arch_pre_init() {
	
}
void arch_cli() {
	asm("csrrc x0,sstatus,0x2");
}
void arch_sti() {
	asm("csrrs x0,sstatus,0x2");
}
void arch_reset() {
	kprintf("rv64: reseting!\n");
	sbi_ecall(SBI_EXT_SRST,0,1,0,0,0,0,0);
}
void arch_poweroff() {
	sbi_ecall(SBI_EXT_SRST,0,0,0,0,0,0,0);
}
bool arch_getFBInfo(fbinfo_t *info) {
	return false;
}
uintptr_t arch_getMemSize() {
	return 20*1024*1024;
}
extern void riscv_switchCntx(void *cntx);
void arch_switchContext(void *prSt) {
	riscv_switchCntx(prSt);
}
void *arch_prepareContext(void *entry,bool isUser) {
	uint64_t *cntx = kmalloc(104);
	memset(cntx,0,104);
	cntx[12] = (uint64_t)entry;
	return cntx;
}
void *arch_preapreArchStack(bool isUser) {
	return NULL;
}
void arch_syscall_init() {
	// ecall is always active in S-Mode.	
}
uint64_t arch_getModuleAddress() {
	return 0;
}
uint64_t arch_getKernelEnd() {
	return (uint64_t)&kernel_end;

}
void arch_destroyContext(void *context) {
}
void arch_destroyArchStack(void *stack) {
}
void userEntry() {
	asm("ecall");
}
void arch_post_init() {
	intStack = kmalloc(16384);
	memset(intStack,0,16384);
	asm("mv tp,%0" ::"r"(intStack));
}
bool arch_relocSymbols(module_t *mod,void *ehdr) {
	return false;
}
void arch_reschedule() {
	asm("wfi");
}
/* Print all CPU information */
void arch_detect() {
	struct SbiRet sbi_ver = sbi_ecall(0x10,0,0,0,0,0,0,0);
	int minor = sbi_ver.value & 0xFFFF;
	int major = (sbi_ver.value >> 16) & 0xFFFF;
	kprintf("SBI Specification Version: v%d.%d\n",major,minor);
}
void arch_sysinfo() {
	// This method not even is called anymore in the kernel, so it's useless now.
}
void arch_putArgs(process_t *prc,int argc,char **argv,char **environ) {
}
void arch_trace(uintptr_t t) {
}
void arch_prepareProcess(process_t *prc) {
}
void arch_fpu_save(void *to) {
}
void arch_fpu_restore(void *from) {
}
/* Actually get syscall caller return address */
uintptr_t arch_syscall_getCallerRet() {
	return 0;
}
void arch_forkProcess(process_t *parent,process_t *child) {
}
char *arch_getName() {
	return "riscv64";
}
bool arch_elf_check_header(Elf32_Ehdr *hdr) {
	if (hdr == NULL) return false; // buggy code maybe
	// nah, potim
	return false;
}
void arch_processSignal(process_t *prc,ProcessSignal handler,...) {}
void arch_exitSignal(process_t *prc,ProcessSignal *to) {}
uintptr_t arch_getMemStart() {
	return (uintptr_t)&kernel_end;
}
void arch_clone_current(process_t *prc,void *newStack,uintptr_t stackSize) {}
uintptr_t riscv_trap_handler(uintptr_t returnTo,uintptr_t cause,registers_t *regs) {
	disableOutput = false;
	uint64_t sstatus = 0;
	uintptr_t regs_cp = (uintptr_t)regs;
	asm volatile("csrr %0, sstatus" : "=r"(sstatus));
	int spp = (sstatus >> 8) & 0xF;
	int interp = (cause >> 63) & 1;
	if (spp && !interp) {
		kprintf("Exception %s(%d) in Kernel-Mode at 0x%x!\n",(cause < 20 ? exception_name[cause] : "not available"),cause,returnTo);
		PANIC("Kernel mode CPU exception");
	}
	cause = cause & 0x7FFFFFFFFFFFFFFF;
//	kprintf("GOT interrupt(0x%x). SPP: 0x%d!\n",returnTo,spp);
	int bit = (1 << cause);
	asm("csrc sip,%0" : : "r"(bit)); // clear pending IRQ.
	switch(cause) {
		case 5:
			set_timer_in_near_future();
			clock_handler((uintptr_t)regs);
			break;
		case 1: {
				if (softIRQ_req == 1) {
					kprintf("RV64: Jumping to user mode! To: 0x%x\n",softIRQ_addr);
					asm("csrc sstatus, %0" : : "r"(256));
					return softIRQ_addr;
				}
			}break;
	}
	asm("mv a2,%0" :: "r"(regs_cp));
//	returnTo = returnTo+4;
	return returnTo;
}
void arch_init_drivers() {
	riscv_serial_init();
}
