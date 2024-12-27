/*
 * Thread/process main header file
 *
*/

#ifndef THREAD_H
#define THREAD_H
#include <typedefs.h>
#include <vfs.h>
#include <dev/clock.h>
#define STATUS_RUNNING 1
#define STATUS_SLEEP 2
#define STATUS_WAITING 3
#define STATUS_ZOMBIE 4
#define STATUS_WAITPID 5
#define STATUS_CREATING 6 // Don't switch by the scheduler
#define SUBSTATE_EXEC 2
#define SUBSTATE_FORK 1
#define PROCESS_QUOTA 10 // 100 ms
#define TYPE_PROCESS 0
#define TYPE_THREAD 1
#define NSIGNALS	32 // Number of signals supported by kernel that can be handled.
typedef struct _signal {
	// Address of real user space handler
	uintptr_t handler;
	// Did the handler already processing?
	int blocked;
        int exited;
        // Flags, for the sigmask and sigprocmask.
        int flags;
	// Where we need to retrun to after processing?
	// void pointer as we don't have the global independ design only architecture specific.
	void *returnStack;
	// Of course the arch specific info
	void *extra;
} ProcessSignal;
struct sigStack {
        void *stack;
        int flags;
        int size;
};
typedef struct process {
    // Base information
    void *stack;
    char *name;
    int pid;
    void *arch_info;
    void *aspace;
    int state;
    // Just for proper memory freeing.
    int substate;
    int exit_code;
    int type; // process or thread
    void *fpu_state;
    // Waitpid
    struct process  *parent;
    struct process *child;
    void *childQueue;	// queue of child processes.
    int quota;
    bool reschedule;
    vfs_node_t *workDir;
    /* Some FS has very huge problems with work direcotry dispatch. So yes,temp resolution. */
    char cwd[255];
    vfs_node_t *root;
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
    ProcessSignal signal_handlers[NSIGNALS]; // static
    void *signalQueue;		// void pointer because of lib/queue.h dependies
    struct sigStack *sigStack;
} process_t;
void thread_init();
process_t *thread_create(char *name,void* entryPoint,bool isUser);
uintptr_t thread_schedule(uintptr_t stack); // interrupt handler
uintptr_t clock_handler(uintptr_t stack);
void kwait(int ms);
uint64_t clock_getUptimeSec();
uint64_t clock_getUptimeMsec();
void clock_setShedulerEnabled(bool enabled);
int thread_getCurrent();
process_t *thread_getThread(int pid);
void thread_killThread(process_t *prc,int code,bool reschedule);
void thread_waitPid(process_t *prc);
int thread_getNextPID();
void thread_changeName(process_t *prc,char *n);
int thread_openFor(process_t *prc,vfs_node_t *node,int flags);
process_t *thread_cloneThread(process_t *parent);
void thread_recreateStack(process_t *prc,int entryPoint,int isUser);
void *thread_getThreadList();
// Forces the scheduler to switch to new task.
void thread_forceSwitch();
uint64_t clock_convertTime(struct tm *time);
void thread_removeThread(process_t *th);
#endif
