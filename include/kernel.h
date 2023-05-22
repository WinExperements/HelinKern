#ifndef KERNEL_H
#define KERNEL_H
/*
	This is our kernel main functions
*/
#include <typedefs.h>
typedef void *(*kernel_irq_handler)(void *);
void kernel_main(const char *args);
void panic(char *file,int line,char *msg);
void *interrupt_handler(int irq_no,void *arch_stack);
void interrupt_add(int int_no,kernel_irq_handler handler);
#endif
