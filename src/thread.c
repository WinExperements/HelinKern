#include <thread.h>
#include <kernel.h>
#include <lib/clist.h>
#include <mm/alloc.h>
#include <arch.h>
#include <arch/mmu.h>
#include <dev/fb.h>
#include <output.h>
#ifdef X86
#define TIMER_NO 32
#else
#define TIMER_NO 0
#endif
process_t *runningTask;
clist_definition_t *task_list,*died_list,*waiting_list;
static bool process_findDispatcher(clist_head_t *head,va_list args);
static bool process_findIdDispatcher(clist_head_t *head,va_list args);
static process_t *thread_findByStatus(int status);
static process_t *thread_findNextByStatus(int status,int addr);
static void thread_killForeach(clist_head_t *head_va,va_list args);
static int freePid;
static process_t *idle;
static void idle_main() {
    arch_sti();
    while(1) {}
}
void thread_init() {
    // Setup the interrupt handler
    interrupt_add(TIMER_NO,clock_handler);
    task_list = (clist_definition_t *)kmalloc(sizeof(clist_definition_t));
    task_list->head = NULL;
    task_list->slot_size = sizeof(process_t);
    died_list = kmalloc(sizeof(clist_definition_t));
    died_list->head = NULL;
    died_list->slot_size = 0;
    waiting_list = kmalloc(sizeof(clist_definition_t));
    waiting_list->head = NULL;
    waiting_list->slot_size = sizeof(int);
    idle = thread_create("idle",(int)idle_main,false);
    idle->state = 0;
}
void *thread_schedule(void *stack) {
    // scheduler v1, taked code from my old osdev project
    // Edited version
    if (clist_size(died_list) != 0) {
        // kill there tasks using foreach
        // switch to our kernel VMM space
        arch_mmu_switch(idle->aspace);
        clist_for_each(died_list,thread_killForeach);
        memset(died_list,0,sizeof(clist_definition_t));
        runningTask = NULL;
    }
    process_t *nextTask;
    //kprintf("Schedule\r\n");
    if (runningTask != NULL) {
        // Okay we need to find a new if the quota is ended
        if (runningTask->quota < PROCESS_QUOTA && !runningTask->reschedule) {
                runningTask->quota++;
                return stack;
            }
            arch_mmu_switch(idle->aspace);
            // Simply select new task
            nextTask = thread_findNextByStatus(STATUS_RUNNING,runningTask->lAddr);
            nextTask->quota = 0;
            nextTask->reschedule = false;
    } else {
        nextTask = thread_findByStatus(STATUS_RUNNING);
    }
    if (!nextTask) {
        // no tasks left, goto idle thread
        nextTask = idle;
    }
    if (nextTask->aspace == NULL) {
        PANIC("Process structure has been rewriten by something!\r\n");
    } 
    // switch the address space
    arch_mmu_switch(nextTask->aspace);
    // switch context
    arch_switchContext(nextTask);
    return stack;
}
process_t *thread_create(char *name,int entryPoint,bool isUser) {
    // Allocate the process
    clist_head_t *h = clist_insert_entry_after(task_list,task_list->head);
    process_t *th = (process_t *)h->data;
    th->pid = freePid++;
    th->name = name;
    th->lAddr = (int)h;
    th->stack = arch_prepareContext(entryPoint);
    th->arch_info = arch_preapreArchStack(isUser);
    th->aspace = arch_mmu_getKernelSpace();
    th->state = STATUS_RUNNING;
    th->reschedule = true;
    th->workDir = vfs_getRoot();
    th->parent = runningTask;
    th->child = NULL;
    return th;
}
static bool process_findDispatcher(clist_head_t *head,va_list args) {
    int status = va_arg(args,int);
    process_t *p = (process_t *)head->data;
    return p->state == status;
}
static bool process_findIdDispatcher(clist_head_t *head,va_list args) {
    int pid = va_arg(args,int);
    process_t *p = (process_t *)head->data;
    return p->pid == pid;
}
static process_t *thread_findByStatus(int status) {
    clist_head_t *cur = clist_find(task_list,process_findDispatcher,status);
	if (cur) {
		return (process_t *)cur->data;
	}
	return NULL;
}
static process_t *thread_findNextByStatus(int status,int addr) {
    clist_head_t *cur = clist_find_next(task_list,(clist_head_t *)addr,process_findDispatcher,status);
	if (cur == NULL) {
        return NULL;
    }
	return (process_t *)cur->data;
}
// Clock implementation
static int num_clocks = 0;
static bool schedulerEnabled = true;
void *clock_handler(void *stack) {
    num_clocks++;
    if (schedulerEnabled) {
        return thread_schedule(stack);
    }
    return stack;
}
void kwait(int ms) {
    int startClocks = num_clocks;
    int endClocks = startClocks + (ms*10);
    while(num_clocks <= endClocks) {}
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
    clist_head_t *cur = clist_find(task_list,process_findIdDispatcher,pid);
    if (cur == NULL) {
        return NULL;
    }
    return (process_t *)cur->data;
}
static void thread_killForeach(clist_head_t *head_va,va_list args) {
    // Called when an thread is begin killed or exited
    process_t *prc = (process_t *)head_va->data;
    // Firstly destroy the contexts and the arch stack
    arch_destroyContext(prc->stack);
    arch_destroyArchStack(prc->arch_info);
    // TODO: Clean all allocated memory via arch functions
    //clist_delete_entry(died_list,(clist_head_t *)prc->lAddr);
    // free parent if it waiting
    process_t *parent = prc->parent;
    if (parent && parent->state == STATUS_WAITPID) {
        parent->state = STATUS_RUNNING;
    }
    kfree(prc);
}
void thread_killThread(process_t *prc,int code) {
    // called when thread killed by an exception or its want to exit
    // remove the process from processes list and insert it to the died list
    clist_delete_entry(task_list,(clist_head_t *)prc->lAddr);
    // insert it to the list
    clist_head_t *ent = clist_insert_entry_after(died_list,died_list->head);
    ent->data = prc;
    prc->lAddr = (int)ent;
    // schedule to a new free task
    runningTask = NULL;
    // wait for reschedule
    while(1) {}
}
void thread_waitPid(process_t *prc) {
    prc->state = STATUS_WAITPID;
}
int thread_getNextPID() {
    return freePid;
}
