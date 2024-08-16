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
#include <resources.h>
#define MAX_PRIORITY 6
process_t *runningTask;
queue_t *task_list;
static int freePid;
static process_t *idle;
extern uint32_t cursor_x,cursor_y;
rlimit_t limitList[7];
// FILO queue code(need to be moved into specific library directory)
typedef struct {
    size_t head;
    size_t tail;
    size_t size;
    void** data;
} filo_t;

void* filo_read(filo_t *queue) {
    if (queue->tail == queue->head) {
        return NULL;
    }
    void* handle = queue->data[queue->tail];
    queue->data[queue->tail] = NULL;
    queue->tail = (queue->tail + 1) % queue->size;
    return handle;
}

int filo_write(filo_t *queue, void* handle) {
    if (((queue->head + 1) % queue->size) == queue->tail) {
        return -1;
    }
    queue->data[queue->head] = handle;
    queue->head = (queue->head + 1) % queue->size;
    return 0;
}
filo_t *filo_new(int max) {
	filo_t *ret = kmalloc(sizeof(filo_t));
	memset(ret,0,sizeof(filo_t));
	ret->size = max;
	ret->data = kmalloc(sizeof(void *) * max);
	return ret;
}
int filo_remove(filo_t *queue, void* handle) {
    size_t current_index = queue->tail;
    while (current_index != queue->head) {
        if (queue->data[current_index] == handle) {
            // Found the element to delete
            queue->data[current_index] = NULL;
            // Shift elements to the right of the deleted element
            while (current_index != queue->head) {
                size_t next_index = (current_index + 1) % queue->size;
                queue->data[current_index] = queue->data[next_index];
                current_index = next_index;
            }
            // Update head pointer
            if (queue->head != 0) {
                queue->head--;
            } else {
                queue->head = queue->size - 1;
            }
            return 0; // Element deleted successfully
        }
        current_index = (current_index + 1) % queue->size;
    }
    return -1; // Element not found in the queue
}

filo_t *priority[MAX_PRIORITY]; // 6000 processes of total
void push_prc(process_t *prc);
static process_t *pop_prc();
static void idle_main() {
    arch_sti();
    while(1) {}
}
void thread_init() {
    // The interrupt handler for timer must be called from ARCH IRQ handler, see porting section in README
    task_list = queue_new();
    // Create the priority FILO queue
    for (int i = 0; i < 2; i++) {
	    priority[i] = filo_new(1000);
    }
    idle = thread_create("idle",idle_main,false);
    dequeue(task_list);
    idle->state = -1;
    runningTask = idle;
    krn_rlimit_init_default();
}
/* Допоможіть :( */
uintptr_t thread_schedule(uintptr_t stack) {
     // scheduler v1, code optimized for performance
     // Priority support
     process_t *nextTask = NULL;
    if (runningTask != NULL) {
	if (runningTask->quota < PROCESS_QUOTA) {
		if (!runningTask->died) {
            		runningTask->quota++;
            		return stack;
		}
        }
    }
	nextTask = pop_prc();
        if (nextTask == NULL) {
	        push_prc(idle);
	        nextTask = idle;
        }
    if (nextTask == NULL || nextTask->state == STATUS_CREATING || nextTask->state == -1 || nextTask->state == STATUS_WAITPID) {
	    //kprintf("Finding next usable process\r\n");
	    // Check if the process doesn't receive any signals.
	    if (((queue_t*)nextTask->signalQueue)->size != 0) {
		    goto switchTask;
	    }
	    while(nextTask) {
		    nextTask = pop_prc();
		    if (nextTask == NULL) {
			    nextTask = idle;
			    break;
		    }
		    if (nextTask->state == STATUS_RUNNING || nextTask->state == STATUS_SLEEP) break;
		    if (nextTask->state != -1 || nextTask->state != STATUS_WAITPID) {
			    push_prc(nextTask);
		    }
	    }
    }
    nextTask = (nextTask && nextTask->state > 3) ? idle : nextTask;
    if (!nextTask) {
	//kprintf("nextTask = NULL! Switch to idle\r\n");
        nextTask = idle;
    }
switchTask:
    nextTask->quota = 0;
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
	    //kprintf("Found sleeping task :)\r\n");
	    if (nextTask->wait_time <= 0) {
		    nextTask->state = STATUS_RUNNING;
		} else {
	    		nextTask->wait_time-=10;
	    		nextTask = idle;
		}
	}
    //kprintf("Switch to %s\n",nextTask->name);
    // switch the address space
    if (runningTask != NULL) {
    	arch_fpu_save(runningTask->fpu_state);
    }
    arch_fpu_restore(nextTask->fpu_state);
    arch_mmu_switch(nextTask->aspace);
    queue_t *prcSignals = nextTask->signalQueue;
    if (prcSignals->size > 0) {
	    uintptr_t num = (uintptr_t)dequeue(prcSignals);
	    if (num >= 32) {
		    kprintf("Process %s got unknown signal to process!\r\n",nextTask->name);
	    } else {
	    	//kprintf("HERE! The process %s got signal. Signal ID: %d, processing address: 0x%x\r\n",nextTask->name,num,nextTask->signal_handlers[num]);
		// TODO: Check if process is user or kernel space type.
		if (nextTask->signal_handlers[num] == 0) {
			//kprintf("%s: process signal handler doesn't set\r\n");
            		goto actualSwitch;
		}
		int to = nextTask->signal_handlers[num];
		if (nextTask->userProcess) {
            		arch_processSignal(nextTask,to,num);
        	}
    }
}
actualSwitch:
    //kprintf("switching to %s\r\n",nextTask->name);
    nextTask->switches++;
  //  interrupt_sendEOI();
    arch_switchContext(nextTask);
    // If we here, then the x86 switch code works -_-
    return stack;
}
process_t *thread_create(char *name,void* entryPoint,bool isUser) {
    // Allocate the process
    process_t *th = (process_t *)kmalloc(sizeof(process_t));
    memset(th,0,sizeof(process_t));
    th->priority = 1;
    th->pid = freePid++;
    th->name = strdup(name);
    th->quota = 10;
    th->stack = arch_prepareContext(entryPoint,isUser);
    th->arch_info = arch_preapreArchStack(isUser);
    th->aspace = arch_mmu_getKernelSpace();
    th->state = STATUS_RUNNING;
    th->reschedule = true;
    th->workDir = vfs_getRoot();
    th->root = vfs_getRoot();
    th->parent = runningTask;
    th->child = NULL;
    th->died = false;
    th->userProcess = isUser;
    th->fds = kmalloc(200);
    // Signals handling.
    th->signalQueue = queue_new();
    memset(th->fds,0,200);
    th->uid = runningTask == NULL ? 0 : runningTask->uid;
    th->gid = runningTask == NULL ? 0 : runningTask->gid;
    //th->userProcess = isUser;
    arch_prepareProcess(th);
    enqueue(task_list,th);
    push_prc(th);
    //kprintf("PID of new process: %d, entry point 0x%x\r\n",th->pid,entryPoint);
    return th;
}
// Clock implementation
static int num_clocks = 0;
static bool schedulerEnabled = true;
//static int seconds;
//static int nextClocks = 0;
uintptr_t clock_handler(uintptr_t stack) {
    num_clocks++;
    if (schedulerEnabled) {
        return thread_schedule(stack);
    }
   // kprintf("ich bin disabled\n");
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
    //kprintf("G: %d\r\n",pid);
    if (pid < 0 || pid > freePid) return NULL;
    for (struct queue_node *n = task_list->head; n; n = n->next) {
        if (!n || n == NULL || n->value == 0) {
            kprintf("Corrupted process list!!\n");
            PANIC("Process list corruption detected\n");
        }
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
    if (prc->pid == 1) {
        kprintf("Attempt to kill init with exit code = 0x%x!\n",code);
        PANIC("Init died!");
    }
    // Remove the process from the lists
    prc->died = true;
    filo_remove(priority[prc->priority],prc);
    queue_remove(task_list, prc);
    // Switch to idle address space
    arch_mmu_switch(idle->aspace);
    DEBUG("Died task: %s!\r\n", prc->name);
    // Cleanup resources
    arch_destroyContext(prc->stack);
    arch_mmu_switch(prc->aspace);
    arch_destroyArchStack(prc->arch_info);
    // Remove all pending signals :)
    while(dequeue((queue_t *)prc->signalQueue) != NULL);
    arch_mmu_switch(idle->aspace);
    if (prc->aspace != idle->aspace && prc->type != TYPE_THREAD) {
        // Check if either the parent or all children have died
        bool allChildrenDied = true;
        process_t *child = prc->child;
        while (child != NULL) {
            if (!child->died) {
                allChildrenDied = false;
                break;
            }
            child = child->child;
    }

    if (prc->parent->died && allChildrenDied) {
        // Destroy address space
        arch_mmu_destroyAspace(prc->aspace, true); // Destroy all memory
    } else {
        // Only destroy writable pages
        arch_mmu_destroyAspace(prc->aspace,false);
    }
}

    kfree(prc->name);
    // Close all file descriptors
    for (int i = 0; i < prc->next_fd; i++) {
        file_descriptor_t *fd = prc->fds[i];
        if (fd != NULL) {
            DEBUG("Closing FD %d, %s\r\n", i,fd->node->name);
            vfs_close(fd->node);
	    if (fd->node->flags == 4) {
		// socket?
		socket_destroy((Socket *)fd->node->priv_data);
		kfree(fd->node);
	    }
            kfree(fd);
        }
    }
    kfree(prc->fds);
    process_t *parent = prc->parent;
    if (parent->died) {
	    // Parent is died, we maybe thread or something else?
	    arch_sti();
        arch_reschedule();
    }
    if ((parent->state & STATUS_WAITPID) == STATUS_WAITPID) {
        parent->state = STATUS_RUNNING;
        parent->died_child = prc->pid;
        push_prc(parent);
    }
    if (runningTask != NULL) {
	    if (runningTask == prc) {
		    runningTask = idle;
		    idle->quota = PROCESS_QUOTA+1;
		} else {
	    		runningTask->quota = PROCESS_QUOTA+1;
		}
    }
    DEBUG("Died task %s switches by scheduler: %d\n",prc->name,prc->switches);
    arch_sti();
    arch_reschedule();
}
void thread_waitPid(process_t *prc) {
    DEBUG("Waitpid for %d\r\n",prc->pid);
    prc->state = STATUS_WAITPID;
    //filo_remove(priority[prc->priority],prc);
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
    if (caller->next_fd+1 > limitList[RLIMIT_NOFILE].r_cur) {
	    // Out of resources.
	    return -1;
    }
    DEBUG("%s: open: %s for %s\r\n",__func__,node->name,caller->name);
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));
    memset(fd,0,sizeof(file_descriptor_t));
    fd->node = node;
    fd->pid = caller->pid;
    int id = caller->next_fd;
    caller->fds[id] = fd;
    caller->next_fd++;
    return id;
}

void push_prc(process_t *prc) {
  if (prc->priority >= 6) {
		PANIC("Corrupted priority ID\r\n");
	}
	if (priority[1] == NULL) {
		PANIC("Corrupted priority queue");
	}
  //kprintf("Pushing process name %s to priority queue %d\r\n",prc->name,prc->priority);
	filo_write(priority[1],prc);
}

process_t *pop_prc() {
	return filo_read(priority[1]);
}
process_t *thread_cloneThread(process_t *parent) {
	return NULL; // TODO
}
void thread_recreateStack(process_t *prc,int entryPoint,int isUser) {
    // caller by elf_load_file to replace caller stack with loadable new
    // Destroy original stack and arch information
    if (prc == NULL || entryPoint == NULL) return;
    void *new_stack = arch_prepareContext((void *)entryPoint,isUser);
    void *arch_info = arch_preapreArchStack(isUser);
    arch_destroyContext(prc->stack);
    arch_destroyArchStack(prc->arch_info);
    // Recreate a new one
    prc->stack = new_stack;
    prc->arch_info = arch_info;
    runningTask = NULL;
}
void *thread_getThreadList() {
	return task_list;
}
void krn_rlimit_init_default() {
	for (int i = 1; i < 7; i++) {
		limitList[i].r_cur = -1;
		limitList[i].r_max = -1;
	}
	limitList[0].r_cur = 0;
	limitList[0].r_max = -1;
	limitList[RLIMIT_NOFILE].r_cur = 1024;
	limitList[RLIMIT_NOFILE].r_max = 4096;
}
void thread_forceSwitch() {
	if (runningTask == NULL) return;
	runningTask->quota = PROCESS_QUOTA+1;
}
