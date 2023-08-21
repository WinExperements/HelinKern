#ifndef X86_TASK_H
#define X86_TASK_H
#include "idt.h"
typedef struct x86_task {
    int kernelESP;
    int stack;
    int userESP;
    int kesp_start;
    // User mode
    int argc,argv;
    int forkESP;
    int userESP_top;
} x86_task_t;
int read_eip();
#endif
