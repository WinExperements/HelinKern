#include <kernel.h>
#include <output.h>
#include <debug.h>
static kernel_irq_handler handlers[256];
void *interrupt_handler(int irq_no,void *arch_stack) {
    if (irq_no < 256) {
        if (handlers[irq_no] != 0) {
            return handlers[irq_no](arch_stack);
        } else {
            kprintf("[WARRNING]: Not handled interrupt: 0x%x\n",irq_no);
        }
    }
    return arch_stack;
}
void interrupt_add(int int_no,kernel_irq_handler handler) {
    if (handlers[int_no] != NULL) {
        kprintf("WARRNING: Interrupt %d already registred\r\n");
        return;
    }
    handlers[int_no] = handler;
}
