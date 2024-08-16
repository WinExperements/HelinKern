#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/io.h>
#include <arch/x86/mmu.h> // COW fork
#include<output.h>
#include<typedefs.h>
#include <kernel.h>
#include <arch.h>
#include <arch/mmu.h>
#include <mm/alloc.h>
// POSIX x86 module extension :) 18/03/2024
#include <arch/x86/task.h>
#include <lib/string.h>
extern void copy_page_physical(uintptr_t,uintptr_t);
gdt_entry_t gdt_entries[7];
gdt_ptr_t   gdt_ptr;
tss_entry_t tss_entry;
extern uintptr_t x86_irq_handler(uintptr_t regsAddr);
extern process_t *runningTask;
extern void x86_serial();
char irqStackTest[16384] __attribute__((aligned(4096)));
#if defined(__x86_64__)
struct x86_64_tss {
	uint32_t reserved0;
	uint64_t rsp[3];	// When privilege change occur.
	uint64_t reserved1;
	uint64_t ist[7];
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base;
} __attribute__((packed));
struct GdtTssExtend {
	uint32_t base_uper;
	uint32_t reserved;
	uint32_t zero;
	uint32_t reserved1;
} __attribute__((packed));
struct x86_64_tss tssEntry;
extern uintptr_t liminePhys(void *ptr);
extern char stack_top;	//src/arch/x86/boot_64.s
void putTSS64() {
/*	uint32_t addr = (uint32_t)(uint64_t)liminePhys(&tssEntry);
	gdt_set_gate(5,addr,sizeof(tssEntry)-1,0x89,0);
*/
	uint32_t addr1 = (uint32_t)((uint64_t)&tssEntry & 0xFFFFFFFF);
	uint32_t addr2 = (uint32_t)(((uint64_t)&tssEntry >> 32) & 0xFFFFFFFF);
	uint64_t limit = (uint64_t)&tssEntry + sizeof(struct x86_64_tss);
	gdt_set_gate(5,addr1,limit & 0xFFFFFFFF,0x89,0);
	struct GdtTssExtend ext_tss;
	ext_tss.base_uper = addr2;
	ext_tss.reserved = 0;
	ext_tss.zero = 0;
	ext_tss.reserved1 = 0;
	gdt_entries[6] = *((gdt_entry_t*)&ext_tss);
}
#endif
void gdt_init()
{
  gdt_ptr.limit = (sizeof(gdt_entry_t) * 7) - 1;
  gdt_ptr.base  = (uintptr_t)&gdt_entries;
#if defined(__x86_64__)
  // x86_64 have different mechanism of GDT.
  gdt_set_gate(0,0,0,0,0);
  gdt_set_gate(1,0,-1,0x9A,0xA0);
  gdt_set_gate(2,0,-1,0x92,0xA0);
  gdt_set_gate(3,0,-1,0xFA,0xA0);
  gdt_set_gate(4,0,-1,0xF2,0xA0);
  // Setup TSS.
  memset(&tssEntry,0,sizeof(struct x86_64_tss));
  tssEntry.rsp[0] = (uint64_t)&irqStackTest + 16384;
  tssEntry.ist[0] = (uint64_t)&irqStackTest+16384;
  tssEntry.ist[1] = (uint64_t)&irqStackTest + 16384;
  //gdt_set_gate(5,(uint32_t)(uint64_t)&tssEntry,sizeof(struct x86_64_tss)-1,0x89,0);
  // Set GDT.
  putTSS64();
   gdt_flush((uintptr_t)&gdt_ptr);
   tss_flush();
#else
  gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment
  tss_write(5,0x10,0);
  gdt_flush((uintptr_t)&gdt_ptr);
  tss_flush();
  kprintf("GDT setted to 0x%x\n",&gdt_ptr);
#endif
}
void gdt_set_gate(s32 num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
  gdt_entries[num].base_low    = (base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high   = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low   = (limit & 0xFFFF);
  gdt_entries[num].granularity = (limit >> 16) & 0x0F;

  gdt_entries[num].granularity |= gran & 0xF0;
  gdt_entries[num].access      = access;
}
void tss_write(s32 num,uint32_t ss0,uint32_t esp0) {
#if defined(__x86_64__)
	return;
#else
  uint32_t base = (uint32_t)&tss_entry;
  uint32_t limit = sizeof(tss_entry);
  gdt_set_gate(num,base,limit,0xE9,0x00);
  memset(&tss_entry,0,sizeof(tss_entry));
  tss_entry.ss0 = ss0;
  tss_entry.esp0 = esp0;
  tss_entry.cs = 0x0b;
  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
#endif
}
void tss_set_stack(uint32_t ss,uint32_t esp) {
  tss_entry.ss0 = ss;
  tss_entry.esp0 = esp;
}
#if defined(__x86_64__)
void tss_set_stack64(uint64_t stack,uint64_t softIRQ) {
	tssEntry.rsp[0] = stack;
	tssEntry.ist[0] = stack;
	tssEntry.ist[1] = softIRQ;
}
#endif
struct tss_entry_struct *tss_getTSS() {
    return &tss_entry;
}
// IDT code
idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;
isr_t interrupt_handlers[128];
// Reserve our exceptions descriptions here
static const char *exception_names[] = {
	"division by zero",
	"debug exception",
	"nonmaskable interrupt",
	"breakpoint",
	"overflow",
	"bounds check",
	"invalid instruction",
	"coprocessor error",
	"double fault",
	"copressor overrun",
	"invalid task",
	"segment not present",
	"stack exception",
	"general protection fault",
	"Segmentation fault",
	"unknown",
	"coprocessor error"
};
void idt_init() {
  idt_ptr.limit = sizeof(idt_entry_t) * 256 -1;
    idt_ptr.base  = (uintptr_t)&idt_entries;
    memset(&idt_entries, 0, sizeof(idt_entry_t)*256);
    outb(PIC_MASTER_COMMAND , 0x11);
  outb(PIC_SLAVE_COMMAND , 0x11);
  outb(PIC_MASTER_DATA , 0x20);
  outb(PIC_SLAVE_DATA , 0x28);
  outb(PIC_MASTER_DATA , 0x04);
  outb(PIC_SLAVE_DATA , 0x02);
  outb(PIC_MASTER_DATA , 0x01);
  outb(PIC_SLAVE_DATA , 0x01);
  outb(PIC_MASTER_DATA , 0x0);
  outb(PIC_SLAVE_DATA , 0x0);
  // Initialise the IDT
  idt_set_gate( 0, (uintptr_t)isr0 , 0x8, 0x8E);
  idt_set_gate( 1, (uintptr_t)isr1 , 0x8, 0x8E);
  idt_set_gate( 2, (uintptr_t)isr2 , 0x8, 0x8E);
  idt_set_gate( 3, (uintptr_t)isr3 , 0x8, 0x8E);
  idt_set_gate( 4, (uintptr_t)isr4 , 0x8, 0x8E);
  idt_set_gate( 5, (uintptr_t)isr5 , 0x8, 0x8E);
  idt_set_gate( 6, (uintptr_t)isr6 , 0x8, 0x8E);
  idt_set_gate( 7, (uintptr_t)isr7 , 0x8, 0x8E);
  idt_set_gate( 8, (uintptr_t)isr8 , 0x8, 0x8E);
  idt_set_gate( 9, (uintptr_t)isr9 , 0x8, 0x8E);
  idt_set_gate( 10, (uintptr_t)isr10 , 0x8, 0x8E);
  idt_set_gate( 11, (uintptr_t)isr11 , 0x8, 0x8E);
  idt_set_gate( 12, (uintptr_t)isr12 , 0x8, 0x8E);
  idt_set_gate( 13, (uintptr_t)isr13 , 0x8, 0x8E);
  idt_set_gate( 14, (uintptr_t)isr14 , 0x8, 0x8E);
  idt_set_gate( 15, (uintptr_t)isr15 , 0x8, 0x8E);
  idt_set_gate( 16, (uintptr_t)isr16 , 0x8, 0x8E);
  idt_set_gate( 17, (uintptr_t)isr17 , 0x8, 0x8E);
  idt_set_gate( 18, (uintptr_t)isr18 , 0x8, 0x8E);
  idt_set_gate( 19, (uintptr_t)isr19 , 0x8, 0x8E);
  idt_set_gate( 20, (uintptr_t)isr20 , 0x8, 0x8E);
  idt_set_gate( 21, (uintptr_t)isr21 , 0x8, 0x8E);
  idt_set_gate( 22, (uintptr_t)isr22 , 0x8, 0x8E);
  idt_set_gate( 23, (uintptr_t)isr23 , 0x8, 0x8E);
  idt_set_gate( 24, (uintptr_t)isr24 , 0x8, 0x8E);
  idt_set_gate( 25, (uintptr_t)isr25 , 0x8, 0x8E);
  idt_set_gate( 26, (uintptr_t)isr26 , 0x8, 0x8E);
  idt_set_gate( 27, (uintptr_t)isr27 , 0x8, 0x8E);
  idt_set_gate( 28, (uintptr_t)isr28 , 0x8, 0x8E);
  idt_set_gate( 29, (uintptr_t)isr29 , 0x8, 0x8E);
  idt_set_gate( 30, (uintptr_t)isr30 , 0x8, 0x8E);
  idt_set_gate( 31, (uintptr_t)isr31 , 0x8, 0x8E);

  // Interrupts from 32 to 47 are reserved for the PIT
  idt_set_gate(32 , (uintptr_t)scheduler_irq , 0x8 , 0x8E);
  idt_set_gate(33 , (uintptr_t)irq1 , 0x8 , 0x8E);
  idt_set_gate(34 , (uintptr_t)irq2 , 0x8 , 0x8E);
  idt_set_gate(35 , (uintptr_t)irq3 , 0x8 , 0x8E);
  idt_set_gate(36 , (uintptr_t)irq4 , 0x8 , 0x8E);
  idt_set_gate(37 , (uintptr_t)irq5 , 0x8 , 0x8E);
  idt_set_gate(38 , (uintptr_t)irq6 , 0x8 , 0x8E);
  idt_set_gate(39 , (uintptr_t)irq7 , 0x8 , 0x8E);
  idt_set_gate(40 , (uintptr_t)irq8 , 0x8 , 0x8E);
  idt_set_gate(41 , (uintptr_t)irq9 , 0x8 , 0x8E);
  idt_set_gate(42 , (uintptr_t)irq10 , 0x8 , 0x8E);
  idt_set_gate(43 , (uintptr_t)irq11 , 0x8 , 0x8E);
  idt_set_gate(44 , (uintptr_t)irq12 , 0x8 , 0x8E);
  idt_set_gate(45 , (uintptr_t)irq13 , 0x8 , 0x8E);
  idt_set_gate(46 , (uintptr_t)irq14 , 0x8 , 0x8E);
  idt_set_gate(47 , (uintptr_t)irq15 , 0x8 , 0x8E);
  idt_set_gate(0x80,(uintptr_t)syscall_irq,0x8,0x8E);
#if defined(__x86_64__)
  idt_entries[32].ist = 1;
  idt_entries[33].ist = 1;
  idt_entries[0x80].ist = 2;
  idt_entries[14].ist = 1;
#endif
  idt_flush((uintptr_t)&idt_ptr);
  memset(interrupt_handlers,0,256);
}
void idt_set_gate(uint8_t num,uintptr_t base,uint16_t sel,uint8_t flags) {
#if defined(__x86_64__)
	// X86_64 has different structure of IDT.
	idt_entries[num].offset1 = (uint16_t)(base & 0x000000000000ffff);
	idt_entries[num].offset2 = (uint16_t)((base & 0x00000000ffff0000) >> 16);
	idt_entries[num].offset3 = (uint32_t)((base & 0xffffffff00000000) >> 32);
	idt_entries[num].sel = sel;
	idt_entries[num].ist = 0;
	idt_entries[num].flags = flags | 0x60;
#else
  idt_entries[num].base_lo = base & 0xFFFF;
  idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

  idt_entries[num].sel     = sel;
  idt_entries[num].always0 = 0;
  // We must uncomment the void x86_irq_handler(registers_t *regs)OR below when we get to using user-mode.
  // It sets the interrupt gate's privilege level to 3.
  idt_entries[num].flags   = flags | 0x60;
#endif
}
void dump_registers(const registers_t *regs) {
}
static bool handle_COW(uintptr_t addr,int present) {
    // Handle COW(Copy-On-Write) fork
    if (present) {
	     //kprintf("Page present, maybe COW, trying to copy and remap\n");
             // Get faulting process
	     /*void *curSpace = arch_mmu_getAspace();
	     arch_mmu_switch(arch_mmu_getKernelSpace());
	     kprintf("%s: offset %d\r\n",addr % 4096);
	     arch_mmu_switch(curSpace);*/
            addr = addr & ~0xfff; // align
            process_t *faulter = thread_getThread(thread_getCurrent());
	    if (faulter == NULL) return true;
            if (faulter->parent == NULL) return false;
            	     aspace_t *addrSpace = arch_mmu_getAspace();
		     vaddr_t phys = (vaddr_t)arch_mmu_getPhysical((void *)addr);
		     aspace_t *parentSP = faulter->parent->aspace;
		     /*
		      * If parent doesn't have the same address of page as we have, that means that parent is already have remaped page, that happening because our scheduler starts first parent then child
		     */
             		//kprintf("Physical address of copy: 0x%x, address virtual: 0x%x, addr space: 0x%x\n",phys,addr,addrSpace);
		     	if (phys == 0) {
			     //kprintf("Not COW reson, continuing default behaivor\n");
                	     return false;
			}
			/*CHANGE_PD(parentSP);
			int parentPhys = 0;
			if (phys != (parentPhys = arch_mmu_getPhysical(addr))) {
				alloc_freePage(parentPhys);
			}
			CHANGE_PD(addrSpace);*/
		     	arch_cli(); // for any case
		     	int phys_of_copy = alloc_getPage();
		     	copy_page_physical(phys,phys_of_copy);
		     	arch_mmu_mapPage(NULL,addr,phys_of_copy,PG_PRESENT | PG_WRITE | PG_USER | PG_OWNED);
             		return true;
		}
    return false;
}

void interrupt_sendEOI() {
    outb(PIC_SLAVE_COMMAND,0x20);
    outb(PIC_MASTER_COMMAND,0x20);
}

uintptr_t x86_irq_handler(uintptr_t regsAddr) {
    // Tell the controller that the interrupt ended
    registers_t *regs = (registers_t *)regsAddr;
    if (regs->int_no == IRQ7 || regs->int_no == IRQ15) {
        //kprintf("X86: Spurious IRQ: 0x%x!\n",regs->int_no);
	// OSDEV Wiki says that we don't need to send EOI to the controllers.
	return regsAddr;
    }
    int int_no = regs->int_no;
    uintptr_t whereIs = 0;
#if defined(__x86_64__)
    whereIs = regs->rip;
#else
    whereIs = regs->eip;
#endif
    if (int_no < IRQ0) {
        arch_cli();
        uintptr_t addr = 0;
	asm volatile("mov %%cr2, %0" : "=r"(addr));
        if (int_no == 14) {
            // get faulting address
             int rw = regs->error_code & 0x2;
             int present = regs->error_code & 0x1;
            if (rw && handle_COW(addr,present)) {
                return regsAddr;
            }
            kprintf("Page fault!!! When trying to %s %x - IP:0x%x\n", rw ? "write to" : "read from", addr, whereIs);
            kprintf("The page was %s\n", present ? "present" : "not present");
//	    kprintf("Fault in address space of process %s, 0x%x\r\n",thread_getThread(thread_getCurrent())->name,arch_mmu_getAspace());
        }
        //arch_poweroff();
        if (regs->cs == 0x1b) {
            kprintf("%s in %s(%d) at 0x%x\r\n",exception_names[int_no],thread_getThread(thread_getCurrent())->name,thread_getCurrent(),whereIs);
	    //arch_sti();
            thread_killThread(thread_getThread(thread_getCurrent()),18198);
            //arch_reschedule(); // never return?
	    kprintf("Kill failed\r\n");
         }
        kprintf("Exception: %s, halt, CS: 0x%x, where: 0x%x\r\n",exception_names[int_no],regs->cs,whereIs);
	uintptr_t traceStack = 0;
#if defined(__x86_64__)
	traceStack = regs->rbp;
#else
	traceStack = regs->ebp;
#endif
	//arch_trace(traceStack);

        // Halt
        while(1) {}
    }
    if (int_no == IRQ0) {
      // Timer interrupt redirection. See README.
      // Save current process stack
	// user scheduling code!
	if (runningTask != NULL && runningTask->userProcess) {
#if !defined(__x86_64__)
		x86_task_t *archInfo = (x86_task_t *)runningTask->arch_info;
		archInfo->userTaskRegisters = regs;
		runningTask->arch_info = archInfo; // for any case
#else
		x64_task_t *archInfo = (x64_task_t *)runningTask->arch_info;
		if (regs->cs == 0x1b) {
			if (archInfo->syscallRegs == 0) {
				archInfo->syscallRegs = kmalloc(sizeof(registers_t));
			}
			memcpy(archInfo->syscallRegs,regs,sizeof(registers_t));
			archInfo->userModeRegs = regs;
		}
		archInfo->userModeRegs = regs;
		runningTask->arch_info = archInfo;
#endif
	}
	interrupt_sendEOI();
      	uintptr_t clockStack = clock_handler(regsAddr);
      	return clockStack;
    } /*else {
        kprintf("Unknown interrupt 0x%x\n",int_no);
    }*/
    uintptr_t ret_regs = interrupt_handler(int_no,regsAddr);
    outb(PIC_SLAVE_COMMAND,0x20);
    outb(PIC_MASTER_COMMAND,0x20);
    return ret_regs;
}
