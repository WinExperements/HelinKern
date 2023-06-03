#include <thread.h>
#include <kernel.h>
#include <lib/queue.h>
#include <mm/alloc.h>
#include <arch.h>
#include <arch/mmu.h>
#include <dev/fb.h>
#include <output.h>
#include <lib/string.h>
#include <debug.h>
#include <dev/fb.h>
#ifdef X86
#define TIMER_NO 32
#else
#define TIMER_NO 0
#endif
process_t *runningTask;
queue_t *task_list,*running_list;
static int freePid;
static process_t *idle;
extern uint32_t cursor_x,cursor_y;
static void idle_main() {
    arch_sti();
    while(1) {}
}
void thread_init() {
    // Setup the interrupt handler
    interrupt_add(TIMER_NO,clock_handler);
    task_list = queue_new();
    running_list = queue_new();
    idle = thread_create("idle",(int)idle_main,false);
    dequeue(task_list);
    dequeue(running_list);
    idle->state = 0;
    runningTask = idle;
}
void *thread_schedule(void *stack) {
    // scheduler v1, taked code from my old osdev project
    // Edited version
    process_t *nextTask;
    //kprintf("Schedule\r\n");
    if (runningTask != NULL) {
        // Okay we need to find a new if the quota is ended
        if (runningTask->quota < PROCESS_QUOTA && !runningTask->reschedule) {
                runningTask->quota++;
                return stack;
            }
            if (running_list->size == 0) return stack;
            // Simply select new task
            nextTask = dequeue(running_list);
            nextTask->quota = 0;
    } else {
        nextTask = dequeue(running_list);
    }
    if (!nextTask) {
        // no tasks left, goto idle thread
        nextTask = idle;
    } else if (nextTask->died) {
        kprintf("Died task: %s!\r\n",nextTask->name);
        arch_destroyContext(nextTask->stack);
        arch_destroyArchStack(nextTask->arch_info);
        kfree(nextTask->name);
        //queue_remove(task_list,nextTask);
	    if (nextTask->parent == NULL) {
		    DEBUG("No parent for %d! Switching to idle!\r\n",nextTask->pid);
		    kfree(nextTask);
		    nextTask = idle;
	    } else {
        	process_t *parent = nextTask->parent;
        	kfree(nextTask);
        	nextTask = parent;
	    }
    }
    if (nextTask->aspace == NULL) {
        PANIC("Process structure has been rewriten by something!\r\n");
    }
    // switch the address space
    arch_mmu_switch(nextTask->aspace);
    arch_switchContext(nextTask);
    return stack;
}
process_t *thread_create(char *name,int entryPoint,bool isUser) {
    // Allocate the process
    process_t *th = (process_t *)kmalloc(sizeof(process_t));
    memset(th,0,sizeof(process_t));
    th->pid = freePid++;
    th->name = strdup(name);
    th->stack = arch_prepareContext(entryPoint);
    th->arch_info = arch_preapreArchStack(isUser);
    th->aspace = arch_mmu_getKernelSpace();
    th->state = STATUS_RUNNING;
    th->reschedule = true;
    th->workDir = vfs_getRoot();
    th->parent = runningTask;
    th->child = NULL;
    th->died = false;
    enqueue(task_list,th);
    enqueue(running_list,th);
    //kprintf("PID of new process: %d\r\n",th->pid);
    return th;
}
// Clock implementation
static int num_clocks = 0;
static bool schedulerEnabled = true;
static int seconds;
static int nextClocks = 0;
void *clock_handler(void *stack) {
    num_clocks++;
    /*nextClocks++;
    if (nextClocks == 1000) {
        nextClocks = 0;
        seconds++;
        int x = fb_getX();
        int y = fb_getY();
        fb_setpos(0,0);
        kprintf("%d seconds\r\n",seconds);
        fb_setpos(x,y);
    }*/
    if (schedulerEnabled) {
        return thread_schedule(stack);
    }
    return stack;
}
void kwait(int ms) {
    int now = num_clocks;
    ++ms;
    while(num_clocks - now < ms) {}
}
int clock_getUptimeSec() {
    return num_clocks/1000;
}
void clock_setShedulerEnabled(bool enabled) {
    schedulerEnabled = enabled;
}
int thread_getCurrent() {
    return (runningTask == NULL ? 0 : runningTask->pid);
}
process_t *thread_getThread(int pid) {
    for (struct queue_node *n = task_list->head; n; n = n->next) {
        process_t *p = (process_t *)n->value;
        if (p->pid == pid) {
            return p;
        }
    }
    return NULL;
}
void thread_killThread(process_t *prc,int code) {
    DEBUG("Killing thread %s with code %d\r\n",prc->name,code);
    // called when thread killed by an exception or its want to exit
    // remove the process from processes list and insert it to the died list
    arch_cli();
    // insert it to the list
    runningTask = NULL;
    prc->died = true;
    enqueue(running_list,prc);
    enqueue(running_list,prc->parent);
    arch_sti();
    arch_reschedule();
}
void thread_waitPid(process_t *prc) {
    DEBUG("Waitpid for %d\r\n",prc->pid);
    prc->state = STATUS_WAITPID;
}
int thread_getNextPID() {
    return freePid;
}
void thread_changeName(process_t *prc,char *n) {
    int newlen = strlen(n)+1;
    int oldLen = strlen(prc->name)+1;
    if (newlen >= oldLen) {
        kfree(prc->name);
        prc->name = kmalloc(newlen);
    }
    strcpy(prc->name,n);
}
