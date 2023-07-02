#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/io.h>
#include<output.h>
#include<typedefs.h>
#include <kernel.h>
#include <arch.h>
gdt_entry_t gdt_entries[6];
gdt_ptr_t   gdt_ptr;
tss_entry_t tss_entry;
extern void *x86_irq_handler(registers_t *regs);
void gdt_init()
{
  gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
  gdt_ptr.base  = (uint32_t)&gdt_entries;

  gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment
  tss_write(5,0x10,0);
  gdt_flush((int)&gdt_ptr);
  tss_flush();
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
  uint32_t base = (uint32_t)&tss_entry;
  uint32_t limit = sizeof(tss_entry);
  gdt_set_gate(num,base,limit,0xE9,0x00);
  memset(&tss_entry,0,sizeof(tss_entry));
  tss_entry.ss0 = ss0;
  tss_entry.esp0 = esp0;
  tss_entry.cs = 0x0b;
  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
}
void tss_set_stack(uint32_t ss,uint32_t esp) {
  tss_entry.ss0 = ss;
  tss_entry.esp0 = esp;
}
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
    idt_ptr.base  = (uint32_t)&idt_entries;
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
  idt_set_gate( 0, (uint32_t)isr0 , 0x8, 0x8E);
  idt_set_gate( 1, (uint32_t)isr1 , 0x8, 0x8E);
  idt_set_gate( 2, (uint32_t)isr2 , 0x8, 0x8E);
  idt_set_gate( 3, (uint32_t)isr3 , 0x8, 0x8E);
  idt_set_gate( 4, (uint32_t)isr4 , 0x8, 0x8E);
  idt_set_gate( 5, (uint32_t)isr5 , 0x8, 0x8E);
  idt_set_gate( 6, (uint32_t)isr6 , 0x8, 0x8E);
  idt_set_gate( 7, (uint32_t)isr7 , 0x8, 0x8E);
  idt_set_gate( 8, (uint32_t)isr8 , 0x8, 0x8E);
  idt_set_gate( 9, (uint32_t)isr9 , 0x8, 0x8E);
  idt_set_gate( 10, (uint32_t)isr10 , 0x8, 0x8E);
  idt_set_gate( 11, (uint32_t)isr11 , 0x8, 0x8E);
  idt_set_gate( 12, (uint32_t)isr12 , 0x8, 0x8E);
  idt_set_gate( 13, (uint32_t)isr13 , 0x8, 0x8E);
  idt_set_gate( 14, (uint32_t)isr14 , 0x8, 0x8E);
  idt_set_gate( 15, (uint32_t)isr15 , 0x8, 0x8E);
  idt_set_gate( 16, (uint32_t)isr16 , 0x8, 0x8E);
  idt_set_gate( 17, (uint32_t)isr17 , 0x8, 0x8E);
  idt_set_gate( 18, (uint32_t)isr18 , 0x8, 0x8E);
  idt_set_gate( 19, (uint32_t)isr19 , 0x8, 0x8E);
  idt_set_gate( 20, (uint32_t)isr20 , 0x8, 0x8E);
  idt_set_gate( 21, (uint32_t)isr21 , 0x8, 0x8E);
  idt_set_gate( 22, (uint32_t)isr22 , 0x8, 0x8E);
  idt_set_gate( 23, (uint32_t)isr23 , 0x8, 0x8E);
  idt_set_gate( 24, (uint32_t)isr24 , 0x8, 0x8E);
  idt_set_gate( 25, (uint32_t)isr25 , 0x8, 0x8E);
  idt_set_gate( 26, (uint32_t)isr26 , 0x8, 0x8E);
  idt_set_gate( 27, (uint32_t)isr27 , 0x8, 0x8E);
  idt_set_gate( 28, (uint32_t)isr28 , 0x8, 0x8E);
  idt_set_gate( 29, (uint32_t)isr29 , 0x8, 0x8E);
  idt_set_gate( 30, (uint32_t)isr30 , 0x8, 0x8E);
  idt_set_gate( 31, (uint32_t)isr31 , 0x8, 0x8E);

  // Interrupts from 32 to 47 are reserved for the PIT
  idt_set_gate(32 , (uint32_t)scheduler_irq , 0x8 , 0x8E);
  idt_set_gate(33 , (uint32_t)irq1 , 0x8 , 0x8E);
  idt_set_gate(34 , (uint32_t)irq2 , 0x8 , 0x8E);
  idt_set_gate(35 , (uint32_t)irq3 , 0x8 , 0x8E);
  idt_set_gate(36 , (uint32_t)irq4 , 0x8 , 0x8E);
  idt_set_gate(37 , (uint32_t)irq5 , 0x8 , 0x8E);
  idt_set_gate(38 , (uint32_t)irq6 , 0x8 , 0x8E);
  idt_set_gate(39 , (uint32_t)irq7 , 0x8 , 0x8E);
  idt_set_gate(40 , (uint32_t)irq8 , 0x8 , 0x8E);
  idt_set_gate(41 , (uint32_t)irq9 , 0x8 , 0x8E);
  idt_set_gate(42 , (uint32_t)irq10 , 0x8 , 0x8E);
  idt_set_gate(43 , (uint32_t)irq11 , 0x8 , 0x8E);
  idt_set_gate(44 , (uint32_t)irq12 , 0x8 , 0x8E);
  idt_set_gate(45 , (uint32_t)irq13 , 0x8 , 0x8E);
  idt_set_gate(46 , (uint32_t)irq14 , 0x8 , 0x8E);
  idt_set_gate(47 , (uint32_t)irq15 , 0x8 , 0x8E);
  idt_set_gate(0x80,(uint32_t)syscall_irq,0x8,0x8E);
  idt_flush((uint32_t)&idt_ptr);
  memset(interrupt_handlers,0,256);
}
void idt_set_gate(uint8_t num,uint32_t base,uint16_t sel,uint8_t flags) {
  idt_entries[num].base_lo = base & 0xFFFF;
  idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

  idt_entries[num].sel     = sel;
  idt_entries[num].always0 = 0;
  // We must uncomment the void x86_irq_handler(registers_t *regs)OR below when we get to using user-mode.
  // It sets the interrupt gate's privilege level to 3.
  idt_entries[num].flags   = flags | 0x60;
}
void *x86_irq_handler(registers_t *regs) {
    // Tell the controller that the interrupt ended
    outb(PIC_SLAVE_COMMAND,0x20);
    outb(PIC_MASTER_COMMAND,0x20);
    int int_no = regs->int_no;
    if (int_no < IRQ0) {
        arch_cli();
        int addr = 0;
		asm volatile("movl %%cr2, %0" : "=r"(addr));
        if (int_no == 14) {
            // get faulting address
             int present = regs->error_code & 0x1;
            int rw = regs->error_code & 0x2;
		    kprintf("Page fault!!! When trying to %s %x - IP:%x\n", rw ? "write to" : "read from", addr, regs->eip);
            kprintf("The page was %s\n", present ? "present" : "not present");
        }
        if (regs->eflags != 518) {
            kprintf("%s in %s\r\n",exception_names[int_no],thread_getThread(thread_getCurrent())->name);
            thread_killThread(thread_getThread(thread_getCurrent()),18198);
            arch_reschedule(); // never return?
         }
        kprintf("Exception: %s, halt\r\n",exception_names[int_no]);
        // Halt
        while(1) {}
    }
    if (int_no == IRQ0) {
      // Timer interrupt redirection. See README.
      return clock_handler(regs);
    }
    return interrupt_handler(int_no,regs);
}
