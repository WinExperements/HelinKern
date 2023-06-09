#ifndef X86_TASK_H
#define X86_TASK_H
#include "idt.h"
typedef struct x86_task {
    int kernelESP;
    int stack;
    int userESP;
    int kesp_start;
} x86_task_t;
#endif
