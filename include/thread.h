#ifndef THREAD_H
#define THREAD_H
#include <typedefs.h>
#include <vfs.h>
#define STATUS_RUNNING 1
#define STATUS_SLEEP 2
#define STATUS_WAITING 3
#define STATUS_KILLED 4
#define STATUS_WAITPID 5
#define PROCESS_QUOTA 10
typedef struct process {
    void *stack;
    char *name;
    int pid;
    void *arch_info;
    void *aspace;
    int state;
    int wait_time;
    struct process  *parent;
    struct process *child;
    int lAddr;
    int quota;
    bool reschedule;
    vfs_node_t *workDir;
    bool died;
    // Memory information
    uint32_t brk_begin,brk_end,brk_next_unallocated_page_begin;
    file_descriptor_t **fds;
    int next_fd;
} process_t;
void thread_init();
process_t *thread_create(char *name,int entryPoint,bool isUser);
void *thread_schedule(); // interrupt handler
void *clock_handler(void *stack);
void kwait(int ms);
int clock_getUptimeSec();
int clock_getUptimeMsec();
void clock_setShedulerEnabled(bool enabled);
int thread_getCurrent();
process_t *thread_getThread(int pid);
void thread_killThread(process_t *prc,int code);
void thread_waitPid(process_t *prc);
int thread_getNextPID();
void thread_changeName(process_t *prc,char *n);
#endif
