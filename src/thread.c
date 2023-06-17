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
    // The interrupt handler for timer must be called from ARCH IRQ handler, see porting section in README
    task_list = queue_new();
    running_list = queue_new();
    idle = thread_create("idle",(int)idle_main,false);
    dequeue(task_list);
    dequeue(running_list);
    idle->state = 0;
    runningTask = idle;
}
/* Допоможіть :( */
void *thread_schedule(void *stack) {
     // scheduler v1, code optimized for performance
    if (runningTask != NULL) {
        if (runningTask->quota < PROCESS_QUOTA) {
            runningTask->quota++;
            return stack;
        }
        if (running_list->size == 0) {
            return stack;
        }
    }
    
    process_t *nextTask = dequeue(running_list);
    nextTask = (nextTask && nextTask->state > 3) ? idle : nextTask;
    
    if (!nextTask) {
        nextTask = idle;
    }
    
    if (!nextTask->started) {
        nextTask->started = true;
    }
    
    if (nextTask->aspace == NULL) {
        PANIC("Process structure has been rewritten by something!\r\n");
    }
    
    enqueue(running_list, nextTask);
    
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
    th->stack = arch_prepareContext(entryPoint,isUser);
    th->arch_info = arch_preapreArchStack(isUser);
    th->aspace = arch_mmu_getKernelSpace();
    th->state = STATUS_RUNNING;
    th->reschedule = true;
    th->workDir = vfs_getRoot();
    th->parent = runningTask;
    th->child = NULL;
    th->died = false;
    th->fds = kmalloc(200);
    th->uid = runningTask == NULL ? 0 : runningTask->uid;
    th->gid = runningTask == NULL ? 0 : runningTask->gid;
    enqueue(task_list,th);
    enqueue(running_list,th);
    //kprintf("PID of new process: %d\r\n",th->pid);
    return th;
}
// Clock implementation
static int num_clocks = 0;
static bool schedulerEnabled = true;
//static int seconds;
//static int nextClocks = 0;
void *clock_handler(void *stack) {
    num_clocks++;
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
    //DEBUG("G: %d\r\n",pid);
    for (struct queue_node *n = task_list->head; n; n = n->next) {
        process_t *p = (process_t *)n->value;
        if (p->pid == pid) {
            return p;
        }
    }
    return NULL;
}
void thread_killThread(process_t *prc,int code) {
    DEBUG("Killing thread %s with code %d\r\n", prc->name, code);
    // called when thread is killed by an exception or wants to exit
    // remove the process from processes list and insert it into the died list
    arch_cli();
    // Remove the process from the lists
    prc->died = true;
    queue_remove(task_list, prc);
    queue_remove(running_list, prc);
    // Switch to idle address space
    arch_mmu_switch(idle->aspace);
    DEBUG("Died task: %s!\r\n", prc->name);
    // Cleanup resources
    arch_destroyContext(prc->stack);
    arch_mmu_switch(prc->aspace);
    arch_destroyArchStack(prc->arch_info);
    arch_mmu_switch(idle->aspace);
    kfree(prc->name);
    // Close all file descriptors
    for (int i = 0; i < prc->next_fd; i++) {
        file_descriptor_t *fd = prc->fds[i];
        if (fd != NULL) {
            DEBUG("Closing FD %d\r\n", i);
            vfs_close(fd->node);
            kfree(fd);
        }
    }
    kfree(prc->fds);
    process_t *parent = prc->parent;
    if (parent->state == STATUS_WAITPID) {
        parent->state = STATUS_RUNNING;
        enqueue(running_list, parent);
    }
    runningTask->quota = 10;
    arch_sti();
    arch_reschedule();
}
void thread_waitPid(process_t *prc) {
    DEBUG("Waitpid for %d\r\n",prc->pid);
    prc->state = STATUS_WAITPID;
    queue_remove(running_list,prc);
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
int clock_getUptimeMsec() {
    return num_clocks;
}
int thread_openFor(process_t *caller,vfs_node_t *node) {
    if (!node) return -1;
    DEBUG("%s: open: %s for %s\r\n",__func__,node->name,caller->name);
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));
    memset(fd,0,sizeof(file_descriptor_t));
    fd->node = node;
    int id = caller->next_fd;
    caller->fds[id] = fd;
    caller->next_fd++;
    return id;
}
