#ifndef ISR_H
#define ISR_H
#include "typedefs.h"
#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

typedef struct registers
{
  uint32_t ds;                  // Data segment selector
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  // Pushed by the processor automatically
  uint32_t int_no,error_code;
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t useresp;
  uint32_t ss;
} __attribute__((packed))  registers_t;
typedef struct brdM {
    int eip;
    int ebx,esi,edi,ebp,edx,ecx,eax;
} __attribute__((packed)) ktask_regs_t;
typedef void (*isr_t)(registers_t *);
void isr_handler();
int irq_handler(registers_t *regs);
void idt_init();
#endif
