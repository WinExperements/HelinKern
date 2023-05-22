#ifndef X86_TASK_H
#define X86_TASK_H
typedef struct x86_task {
    int kernelESP;
    int stack;
} x86_task_t;
#endif
