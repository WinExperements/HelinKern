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
#include <dev/socket.h>
#define MAX_PRIORITY 6
process_t *runningTask;
queue_t *task_list;
static int freePid;
static process_t *idle;
extern uint32_t cursor_x,cursor_y;
static queue_t *priority[MAX_PRIORITY];
static void push_prc(process_t *prc);
static process_t *pop_prc();
static void idle_main() {
    arch_sti();
    while(1) {}
}
void thread_init() {
    // The interrupt handler for timer must be called from ARCH IRQ handler, see porting section in README
    task_list = queue_new();
    // Create the priority queue
    for (int i = 0; i < MAX_PRIORITY; i++) {
	    priority[i] = queue_new();
    }
    idle = thread_create("idle",(int)idle_main,false);
    dequeue(task_list);
    idle->state = 0;
    runningTask = idle;
}
/* Допоможіть :( */
void *thread_schedule(void *stack) {
     // scheduler v1, code optimized for performance
     // Priority support
     process_t *nextTask = NULL;
    if (runningTask != NULL) {
        if (runningTask->quota < PROCESS_QUOTA) {
            runningTask->quota++;
            return stack;
        }
	    nextTask = pop_prc();
        if (nextTask == NULL) {
            // Oh! Is this means that the init process just died?
	    // We just send the warrning message and switch to idle
	    //kprintf("Warrning: no processes left to run, maybe your init just died, please reboot device\r\n");
	    push_prc(idle);
	    return stack;
        }
    }
    if (nextTask->state == STATUS_CREATING) {
	    while(nextTask) {
		    nextTask = pop_prc();
		    if (nextTask->state == STATUS_RUNNING) break;
	}
    }
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
    if (nextTask->type == TYPE_THREAD) {
	    if (nextTask->parent->died) {
		    kprintf("Warrning! Thread parent died, killing thread\n");
		    process_t *switchTo = idle;
		    thread_killThread(nextTask,0);
		    nextTask = switchTo;
	}
    }
    push_prc(nextTask);
    if (nextTask->state == STATUS_SLEEP) {
	    if (nextTask->wait_time <= 0) {
		    nextTask->state = STATUS_RUNNING;
		}
	    nextTask->wait_time--;
	    // Switch to idle
	    nextTask = idle;
	}
    // switch the address space
    arch_fpu_save(runningTask->fpu_state);
    arch_fpu_restore(nextTask->fpu_state);
    arch_mmu_switch(nextTask->aspace);
    arch_switchContext(nextTask);
    return stack;
}
process_t *thread_create(char *name,int entryPoint,bool isUser) {
    // Allocate the process
    process_t *th = (process_t *)kmalloc(sizeof(process_t));
    memset(th,0,sizeof(process_t));
    th->priority = 1;
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
    memset(th->fds,0,200);
    th->uid = runningTask == NULL ? 0 : runningTask->uid;
    th->gid = runningTask == NULL ? 0 : runningTask->gid;
    arch_prepareProcess(th);
    enqueue(task_list,th);
    push_prc(th);
    //kprintf("PID of new process: %d\r\n",th->pid);
    if (runningTask != NULL) {
	    runningTask->quota = PROCESS_QUOTA; // New task must always starts after second timer interrupt
	}
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
#ifdef DEBUG
    return idle;
#else
    return NULL;
#endif
}
void thread_killThread(process_t *prc,int code) {
    DEBUG("Killing thread %s with code %d\r\n", prc->name, code);
    // called when thread is killed by an exception or wants to exit
    // remove the process from processes list and insert it into the died list
    arch_cli();
    // Remove the process from the lists
    prc->died = true;
    queue_remove(priority[prc->priority],prc);
    queue_remove(task_list, prc);
    // Switch to idle address space
    arch_mmu_switch(idle->aspace);
    DEBUG("Died task: %s!\r\n", prc->name);
    // Cleanup resources
    arch_destroyContext(prc->stack);
    arch_mmu_switch(prc->aspace);
    arch_destroyArchStack(prc->arch_info);
    arch_mmu_switch(idle->aspace);
    if (prc->aspace != idle->aspace && prc->type != TYPE_THREAD) {
        // Destroy address space
        arch_mmu_destroyAspace(prc->aspace);
    }
    kfree(prc->name);
    // Close all file descriptors
    for (int i = 0; i < prc->next_fd; i++) {
        file_descriptor_t *fd = prc->fds[i];
        if (fd != NULL) {
            DEBUG("Closing FD %d\r\n", i);
            vfs_close(fd->node);
	    if (fd->node->flags == 4) {
		// socket?
		Socket *s = (Socket *)fd->node->priv_data;
		s->destroy(s);
		kfree(s);
		kfree(fd->node);
	    }
            kfree(fd);
        }
    }
    kfree(prc->fds);
    process_t *parent = prc->parent;
    if (parent->died) {
	    // Parent is died, we maybe thread or something else?
	    return;
    }
    if (parent->state == STATUS_WAITPID) {
        parent->state = STATUS_RUNNING;
        push_prc(parent);
    }
    runningTask->quota = PROCESS_QUOTA;
    arch_sti();
    arch_reschedule();
}
void thread_waitPid(process_t *prc) {
    DEBUG("Waitpid for %d\r\n",prc->pid);
    prc->state = STATUS_WAITPID;
    queue_remove(priority[prc->priority],prc);
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
    if (!node) {
	    DEBUG_N("No node passed for thread_openFor, returning -1 as result\n");
	    return -1;
    }
    DEBUG("%s: open: %s for %s\r\n",__func__,node->name,caller->name);
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));
    memset(fd,0,sizeof(file_descriptor_t));
    fd->node = node;
    int id = caller->next_fd;
    caller->fds[id] = fd;
    caller->next_fd++;
    return id;
}

static void push_prc(process_t *prc) {
	enqueue(priority[prc->priority],prc);
}

process_t *pop_prc() {
	for (int i = 0; i < MAX_PRIORITY; i++) {
		if (priority[i]->size > 0) {
			return dequeue(priority[i]);
		}
	}
	return NULL; // No process
}

