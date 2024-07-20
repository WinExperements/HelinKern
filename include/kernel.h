#ifndef KERNEL_H
#define KERNEL_H
/*
	This is our kernel main functions
*/
#include <typedefs.h>
typedef __SIZE_TYPE__ (*kernel_irq_handler)(__SIZE_TYPE__ stack);
void kernel_main(const char *args);
void panic(char *file,int line,char *msg);
__SIZE_TYPE__ interrupt_handler(int irq_no,__SIZE_TYPE__ arch_stack);
void interrupt_add(int int_no,kernel_irq_handler handler);
void interrupt_sendEOI();
#endif
