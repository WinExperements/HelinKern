/*
 * Thread and general time/clock API implementation.
 * Very very buggy fucking code.
*/

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
#include <dev/clock.h>
#define MAX_PRIORITY 6
process_t *runningTask;
queue_t *task_list;
static int freePid;
static process_t *idle;
extern uint32_t cursor_x,cursor_y;
rlimit_t limitList[7];
extern bool isAccessable(void *ptr);
// FILO queue code(need to be moved into specific library directory)
typedef struct {
    size_t head;
    size_t tail;
    size_t size;
    void** data;
} filo_t;
uint64_t realTime = 0;
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
    // Initialize clock.
    struct tm realtime_cl;
#ifdef HWCLOCK
    hw_clock_get(&realtime_cl);
    realTime = clock_convertTime(&realtime_cl); // convert timespec to UNIX timestamp.
#else
    memset(&realtime_cl,0,sizeof(struct tm));
#endif
}
/* Допоможіть :( */
uintptr_t thread_schedule(uintptr_t stack) {
     // scheduler v1, code optimized for performance
     // Priority support
     process_t *nextTask = NULL;
     queue_t *prcSignals = NULL;
    if (runningTask != NULL) {
	if (runningTask->quota < PROCESS_QUOTA) {
		if (!runningTask->died) {
                        if (((queue_t *)runningTask->signalQueue)->size > 0) {
                                nextTask = runningTask;
				prcSignals = (queue_t *)nextTask->signalQueue;
                                goto processSignals;
                        }
            		runningTask->quota++;
            		return stack;
		}
        }
    }
	nextTask = pop_prc();
        if (nextTask == NULL) {
	        return stack;
        }
    if (nextTask == NULL || nextTask->state == STATUS_CREATING || nextTask->state == -1 || nextTask->state == STATUS_ZOMBIE || nextTask->died) {
	    //kprintf("Finding next usable process\r\n");
	    // Check if the process doesn't receive any signals.
  selectTask:
	    while(nextTask) {
		    nextTask = pop_prc();
		    if (nextTask == NULL) {
			    return stack;
		    }
		    if (nextTask->died) continue;
		    if (nextTask->state == STATUS_RUNNING || nextTask->state == STATUS_SLEEP) break;
		    if (nextTask->state != -1 || nextTask->state != STATUS_ZOMBIE) {
			    push_prc(nextTask);
		    }
	    }
    }
    nextTask = (nextTask && nextTask->state > 3) ? idle : nextTask;
    if (!nextTask) {
	//kprintf("nextTask = NULL! Switch to idle\r\n");
        return stack;
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
		    thread_killThread(nextTask,0,true);
		    nextTask = switchTo;
	}
    }
    push_prc(nextTask);
    //kprintf("Switch to %s\n",nextTask->name);
    // switch the address space
    if (runningTask != NULL) {
    	arch_fpu_save(runningTask->fpu_state);
    }
    arch_fpu_restore(nextTask->fpu_state);
    arch_mmu_switch(nextTask->aspace);
    prcSignals = (queue_t *)nextTask->signalQueue;
processSignals:
    if (prcSignals->size > 0) {
	    uintptr_t num = (uintptr_t)dequeue(prcSignals);
	    if (num >= 32) {
		    kprintf("Process %s got unknown signal to process!\r\n",nextTask->name);
	    } else {
	    	//kprintf("HERE! The process %s got signal. Signal ID: %d, processing address: 0x%x\r\n",nextTask->name,num,nextTask->signal_handlers[num]);
		    // TODO: Check if process is user or kernel space type.
		    ProcessSignal handler = nextTask->signal_handlers[num];
		    if (handler.handler == 0) {
                            // Default behaivor is KILL!
                            if (num == 20) {
                                    goto actualSwitch;
                                }
                            process_t *par = nextTask->parent;
                            thread_killThread(nextTask,-num,false);
                            // Switch to the parent?
                            if (thread_getThread(par->pid) == NULL) {
                                    goto switchTask;
                                } else {
                                        nextTask = par;
                                        goto switchTask;
                                }
                    }
		    if (nextTask->userProcess) {
                        // Check if the signal isn't already processing.
                        if (handler.blocked) {
                                // push back.
                                enqueue(prcSignals,(void *)num);
                                goto actualSwitch; // wait until original handler exits.
                        }
                        handler.blocked = true;
                        nextTask->signal_handlers[num] = handler;
            		arch_processSignal(nextTask,handler,num);
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
    if (runningTask != NULL && runningTask->pid != 0) {
	    th->workDir = runningTask->workDir;
	    th->root = runningTask->root;
            memcpy(th->cwd,runningTask->cwd,255);
	} else {
		// init?
		th->workDir = th->root = vfs_getRoot();
                strcpy(th->cwd,"/");
	}
    th->parent = runningTask;
    th->child = NULL;
    th->died = false;
    th->userProcess = isUser;
    th->fds = kmalloc(200*sizeof(file_descriptor_t*));
    // Signals handling.
    th->signalQueue = queue_new();
    th->childQueue = queue_new();
    memset(th->fds,0,200*sizeof(file_descriptor_t *));
    th->uid = runningTask == NULL ? 0 : runningTask->uid;
    th->gid = runningTask == NULL ? 0 : runningTask->gid;
    //th->userProcess = isUser;
    arch_prepareProcess(th);
    // Place the process to the parent process list.
    if (runningTask != NULL && runningTask->pid != 0
		    && runningTask->state == STATUS_RUNNING) {
	    enqueue(runningTask->childQueue,th);
	}
    enqueue(task_list,th);
    push_prc(th);
    //kprintf("PID of new process: %d, entry point 0x%x\r\n",th->pid,entryPoint);
    return th;
}
// Clock implementation
static uint64_t num_clocks = 0;
static bool schedulerEnabled = true;
static int msBeforeUpdate; // ms before the software clock update.(if this variable value is >= 1000 then update the clock)
//static int nextClocks = 0;
uintptr_t clock_handler(uintptr_t stack) {
    num_clocks++;
    msBeforeUpdate++;
    // Temporarly solution.
    if (msBeforeUpdate >= 1000) {
	    msBeforeUpdate = 0;
	    realTime++;
    }
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
uint64_t clock_getUptimeSec() {
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
void thread_killThread(process_t *prc,int code,bool reschedule) {
    DEBUG("Killing thread %s with code %d\r\n", prc->name, code);
    // called when thread is killed by an exception or wants to exit
    // remove the process from processes list and insert it into the died list
    arch_cli();
    if (prc->pid == 1) {
        kprintf("Attempt to kill init with exit code = 0x%x!\n",code);
        PANIC("Init died!");
    }
    // Remove the process from the lists
    if (code > 0) {
	    // 8 bits represent the exit code.
	    prc->exit_code = (code << 8);
    } else {
        int whatCode = code * -1;
    	prc->exit_code = whatCode; // maybe killed by a signal?
    }
    prc->died = true;
    prc->state = STATUS_ZOMBIE;
    filo_remove(priority[prc->priority],prc);
    queue_remove(task_list, prc);
    // Try to remove yourself from parent list(if parent didn't died)
    // Check if the process doesn't have zombies.
    queue_for(p,(queue_t*)prc->childQueue) {
	    process_t *child = (process_t *)p->value;
	    if (child->died || child->state == STATUS_ZOMBIE) {
		    // Reparent.
		    process_t *init = (process_t*)task_list->head->value;
		    enqueue(init->childQueue,child);
		    /*thread_removeThread(child);
		    kfree(child);*/
		  }
    }
    // Switch to idle address space.
    arch_mmu_switch(idle->aspace);
    //DEBUG("Died task: %s!\r\n", prc->name);
    // Cleanup resources
    arch_destroyContext(prc->stack);
    arch_mmu_switch(prc->aspace);
    arch_destroyArchStack(prc->arch_info);
    // Remove all pending signals :)
    while(dequeue((queue_t *)prc->signalQueue) != NULL);
    kfree(prc->signalQueue);
    // Destroy the child process queue.
    arch_mmu_switch(idle->aspace);
    //kfree(prc->name);
    // Free signal handlers.
    for (int i = 0; i < NSIGNALS; i++) {
	    ProcessSignal s = prc->signal_handlers[i];
	    if (s.returnStack != NULL) {
        kfree(s.returnStack);
    }
	}
    //kprintf("fds[199] = 0x%x\n",prc->fds[199]);
    // Close all file descriptors
    for (int i = 0; i < 200; i++) {
        file_descriptor_t *fd = prc->fds[i];
          if (fd != NULL) {
              //kprintf("Closing FD %d, %s\r\n", i,fd->node->name);
              vfs_close(fd->node);
	        if ((fd->node->flags & VFS_SOCKET) == VFS_SOCKET) {
		          // socket?
                          kprintf("Trying to destroy socket %s with index %d for process %s with priv_data ptr 0x%x\n",fd->node->name,i,prc->name,fd->node->priv_data);
		          if (((Socket *)fd->node->priv_data)->owner != prc->pid) {
			            goto freeFd;
	    	      	  }
		                socket_destroy((Socket *)fd->node->priv_data);
              		  kfree(fd->node);
	            }
freeFd:
            kfree(fd);
            prc->fds[i] = NULL;
        }
    }
    kfree(prc->fds);
    bool allChildrenDied = true;
    queue_for(ch,(queue_t*)prc->childQueue) {
      process_t *child = (process_t*)ch->value;
      if (child->state != STATUS_ZOMBIE) {
        allChildrenDied = false;
      } else {
        child->parent = NULL;
      }
    }
    if (prc->type != TYPE_THREAD) {
      if ((prc->substate == SUBSTATE_EXEC) || (prc->parent == NULL && allChildrenDied)) {
        arch_mmu_destroyAspace(prc->aspace,true);
      } else {
        arch_mmu_destroyAspace(prc->aspace,false);
      }
    }
    while(dequeue((queue_t*)prc->childQueue));
    kfree(prc->childQueue);
    process_t *parent = prc->parent;
	  if (prc->type == TYPE_THREAD) {
		    // We thread? Sooo, clean up resources.
		    kfree(prc);
		    queue_remove(task_list,prc);
	      arch_sti();
        arch_reschedule();
    }
    if (reschedule) {
      if (runningTask != NULL) {
	      if (runningTask == prc) {
		      runningTask = idle;
		      idle->quota = PROCESS_QUOTA+1;
		    } else {
	    		  runningTask->quota = PROCESS_QUOTA+1;
		    }
      }
    }
    // Notify the parent about the process died.
    if (parent != NULL) enqueue((queue_t*)parent->signalQueue,(void*)20);
    //DEBUG("Died task %s switches by scheduler: %d\n",prc->name,prc->switches);
    if (reschedule) {
      arch_sti();
      arch_reschedule();
    }
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
	kfree(prc->name);
	prc->name = strdup(n);
}
uint64_t clock_getUptimeMsec() {
    return num_clocks;
}
int thread_openFor(process_t *caller,vfs_node_t *node,int flags) {
    if (!node) {
	    DEBUG_N("No node passed for thread_openFor, returning -1 as result\n");
	    return -EBADF;
    }
    if (caller->next_fd+1 > limitList[RLIMIT_NOFILE].r_cur) {
	    vfs_close(node);
	    // Out of resources.
	    return -ENFILE;
    }
    //DEBUG("%s: open: %s for %s\r\n",__func__,node->name,caller->name);
    file_descriptor_t *fd = kmalloc(sizeof(file_descriptor_t));
    memset(fd,0,sizeof(file_descriptor_t));
    fd->node = node;
    fd->pid = caller->pid;
    fd->flags = flags;
    int id = caller->next_fd;
    caller->fds[id] = fd;
    caller->next_fd++;
    return id;
}

void push_prc(process_t *prc) {
  if (prc->priority >= 6) {
    kprintf("Report! Process state: %d. Died: %d\n",prc->state,prc->died);
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
        arch_sti();
        while(1) {}
}
uint64_t clock_get() {
	return realTime;
}
uint64_t clock_convertTime(struct tm *ti) {
	/*int days = 0;
	for (int i = 1970; i < 1970+ti->tm_year; i++) {
		days += 365;
	}
	uint64_t secs = days * SEC_PER_YR;
	secs += (ti->tm_hour * SEC_PER_HOUR);
	secs += (ti->tm_min * SEC_PER_MIN);
	secs += (ti->tm_sec);*/
	if (ti->tm_year < 1970) {
		ti->tm_year += 1900;
	}
	int year_diff = ti->tm_year - 1970 + 1;
	int leap_years = year_diff / 4 - year_diff / 100 + year_diff / 400;
	uint64_t days = year_diff * 365;
	for (int i = 0; i < ti->tm_mon; ++i) {
		days += 30;
	}
	days += ti->tm_mday - 1;
	uint64_t secs = days * 86400 + ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;
	return secs;
}
void thread_removeThread(process_t *prc) {
	queue_remove(task_list,prc);
}
