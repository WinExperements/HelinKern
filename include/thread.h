#ifndef THREAD_H
#define THREAD_H
#include <typedefs.h>
#include <vfs.h>
#define STATUS_RUNNING 1
#define STATUS_SLEEP 2
#define STATUS_WAITING 3
#define STATUS_KILLED 4
#define STATUS_WAITPID 5
#define STATUS_CREATING 6 // Don't switch by the scheduler
#define PROCESS_QUOTA 100 // 100 ms
#define TYPE_PROCESS 0
#define TYPE_THREAD 1
#define NSIGNALS	32 // Number of signals supported by kernel that can be handled.
typedef struct process {
    // Base information
    void *stack;
    char *name;
    int pid;
    void *arch_info;
    void *aspace;
    int state;
    int wait_time;
    int died_child;
    int type; // process or thread
    void *fpu_state;
    // Waitpid
    struct process  *parent;
    struct process *child;
    int quota;
    bool reschedule;
    vfs_node_t *workDir;
    bool died;
    // Memory information
    uint32_t brk_begin,brk_end,brk_next_unallocated_page_begin; // 64-bit support :)
    // Misc
    file_descriptor_t **fds;
    int next_fd;
    bool started;
    // User information
    int uid,gid;
    int priority;
    int switches;
    bool userProcess;	// maximum arch independ code.
    /* Signal specific information */
    int signal_handlers[NSIGNALS]; // static
    void *signalQueue;		// void pointer because of lib/queue.h dependies
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
int thread_openFor(process_t *prc,vfs_node_t *node);
process_t *thread_cloneThread(process_t *parent);
void thread_recreateStack(process_t *prc,int entryPoint,int isUser);
#endif
