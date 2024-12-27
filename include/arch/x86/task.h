#ifndef X86_TASK_H
#define X86_TASK_H
#include "idt.h"
typedef struct x86_task {
    int kernelESP;
    int stack;
    int userESP;
    int kesp_start;
    // User mode
    int argc,argv,environ;
    int forkESP;
    int userESP_top;
    registers_t *userTaskRegisters;
    // Signals!
    registers_t *signalReturn; // used by sys_sigexit
    registers_t *syscallRegs; // used for signal processing....
} x86_task_t;
#if defined(__x86_64__)
typedef struct x64_task {
	/* Stack pointer for Kernel mode IRQ handling.
	 * Used because well, without it the stack begin corrupted at some point 🤣
	*/
	__SIZE_TYPE__ rsp0;
	__SIZE_TYPE__ rsp0_top;
	__SIZE_TYPE__ userStack;
	__SIZE_TYPE__ userStackTop;
	// Well, this stack is used for non software interrupts(system calls for example)
	__SIZE_TYPE__ softIRQ_top;
	__SIZE_TYPE__ softIRQ;
	registers_t *userModeRegs;	// used to switch to user mode and for signal handling
	registers_t *syscallRegs;
	int argc;
	char **argv;
	char **environ;
	// Signals.
	registers_t *sigReturn;
} x64_task_t; 
typedef struct _taskRegisters {
	uint64_t rsp,rbx,rbp,r12,r13,r14,r15,rdi,rax;
} x64TaskRegisters;
#endif
int read_eip();
#endif
