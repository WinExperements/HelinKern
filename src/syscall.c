#include <syscall.h>
#include <output.h>
#include <arch.h>
#include <thread.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <elf.h>
#include <arch/mmu.h>
#include <debug.h>
#include <debug.h>
#include <dev/socket.h>
#include <dev/clock.h>
#include <dev.h>
// Pthreads structure that passed to newlib thread_entry
typedef struct _pthread_str {
	int entry;
	void *arg;
} pthread_str;

struct utsname {
    char *sysname;    /* Operating system name (e.g., "Linux") */
    char *nodename;   /* Name within communications network */
    char *release;
    char *version;
    char *machine;
};

extern char *ringBuff;
static char *system_hostname = NULL;
extern queue_t *priority[6];
static void sys_default() {}
static void sys_exit(int exitCode);
static void sys_kill(int pid,int code);
static int sys_getpid();
static int sys_open(char *path,int flags);
static void sys_close(int fd);
static int sys_read(int fd,int offset,int size,void *buff);
static int sys_write(int fd,int offset,int size,void *buff);
// This 3 syscalls need to be changed.
// TODO: Reformate this syscalls into clock_* syscalls.
static void sys_signal(int sig,int handler);
static void sys_sigexit();
static void sys_print(char *msg);
// End of deprecated system calls. Need reform. Never use it.
static int sys_exec(char *path,int argc,char **argv);
static int sys_reboot(int reason);
static int sys_chdir(char *to);
static void sys_pwd(char *buff,int len);
static int sys_opendir(char *path);
static void sys_closedir(int fd);
static void *sys_readdir(int fd,int p);
static int sys_mount(char *dev,char *mount_point,char *fs);
static int sys_waitpid(int pid);
static int sys_getppid();
static int sys_getuid();
static void sys_setuid(int uid);
static int sys_seek(int fd,int type,int how);
static int sys_tell(int fd);
static void *sys_mmap(int _fd,int addr,int size,int offset,int flags);
static int sys_insmod(char *path);
static void sys_rmmod(char *name);
static int sys_ioctl(int fd,int req,va_list list);
static void sys_setgid(int gid);
static int sys_getgid();
static void *sys_sbrk(int increment);
static int sys_dup(int oldfd,int newfd);
static int sys_clone(int entryPoint,int arg1,int arg2); // pthread support, yay
static int sys_truncate(int fd,int newsize);
// HelinOS specific
static void sys_waitForStart(int pid); // waits for thread begin started, automatically enabled the interrupts
static void sys_usleep(int ms);
static int sys_socket(int domain,int type,int protocol);
static int sys_bind(int sockfd,struct sockaddr *addr,int len);
static int sys_listen(int sockfd,int backlog);
static int sys_accept(int sockfd,struct sockaddr *addr,int len);
static int sys_connect(int sockfd,const struct sockaddr *addr,int len);
static int sys_send(int sockfd,const void *buff,int len,int flags);
static int sys_recv(int sockfd,void *buff,int len,int flags);
static int sys_ready(int fd); // for select
static int sys_fork(); // yeah fork like in all UNIXes
static int sys_uname(struct utsname *name);
static int sys_sethostname(char *name,int len);
static int sys_gethostname(char *name,int len);
static int sys_creat(char *path,int type);
static int sys_sync(int fd);
static int sys_munmap(void *ptr,int size);
static int sys_umount(char *mountpoint);
static int sys_access(int fd);
static int sys_chmod(int fd,int mode);
static int sys_stat(int fd,struct stat *);
static int sys_unlink(char *path);
static int sys_gettime(int clock,struct tm *time);
static int sys_settime(int clock,struct tm *time);
static int sys_syslog(int type,char *buf,int len);
static int sys_chroot(char *to);
static int sys_fchdir(int fd);
static int sys_fchown(int fd,int owner,int group);
static int sys_chown(int mode,char *path,int owner,int group);
static int sys_rm(int mode,char *path);
static int sys_getpgid(int pid);
static int sys_nice(int priority);
static int sys_symlink(char *target,char *path);
static void *kmalloc_user(process_t *prc,int size);
int syscall_table[69] = {
    (int)sys_default,
    (int)sys_print,
    (int)sys_exit,
    (int)sys_kill,
    (int)sys_getpid,
    (int)sys_stat,
    (int)sys_unlink,
    (int)sys_open,
    (int)sys_close,
    (int)sys_read,
    (int)sys_write,
    (int)sys_signal,
    (int)sys_sigexit,
    (int)sys_exec,
    (int)sys_reboot,
    (int)sys_default,
    (int)sys_pwd,
    (int)sys_chdir,
    (int)sys_opendir,
    (int)sys_closedir,
    (int)sys_readdir,
    (int)sys_mount,
    (int)sys_waitpid,
    (int)sys_getppid,
    (int)sys_sync,
    (int)sys_getuid,
    (int)sys_setuid,
    (int)sys_seek,
    (int)sys_tell,
    (int)sys_mmap,
    (int)sys_insmod,
    (int)sys_rmmod,
    (int)sys_ioctl,
    (int)sys_setgid,
    (int)sys_getgid,
    (int)sys_sbrk,
    (int)sys_dup,
    (int)sys_clone,
    (int)sys_waitForStart,
    (int)sys_usleep,
    (int)sys_truncate,
    (int)sys_socket,
    (int)sys_bind,
    (int)sys_listen,
    (int)sys_accept,
    (int)sys_connect,
    (int)sys_send,
    (int)sys_recv,
    (int)sys_ready,
    (int)sys_fork,
    (int)sys_uname,
    (int)sys_sethostname,
    (int)sys_gethostname,
    (int)sys_creat,
    (int)sys_munmap,
    (int)sys_umount,
    (int)sys_access,
    (int)sys_chmod,
    (int)sys_gettime,
    (int)sys_settime,
    (int)sys_syslog,
    (int)sys_chroot,
    (int)sys_fchdir,
    (int)sys_fchown,
    (int)sys_chown,
    (int)sys_rm,
    (int)sys_getpgid,
    (int)sys_nice,
    (int)sys_symlink,
};
int syscall_num = 69;
extern void push_prc(process_t *);
static void *kmalloc_user(process_t *prc,int size) {
	return sbrk(prc,size);
}
static char *strdup_user(process_t *prc,char *str) {
	int len = strlen(str);
	char *new = sbrk(prc,len+1);
	strcpy(new,str);
	new[len] = 0;
	return new;
}
static void sys_print(char *msg) {
    /*
    	WARRNING! Deprecated. Never use it.
     */
    //sys_write(1,0,sizeof(msg),msg);
}
static bool isAccessable(void *ptr) {
    return arch_mmu_getPhysical(ptr) != 0;
}
void syscall_init() {
    // redirect to arch specific init code
    arch_syscall_init();
}
int syscall_get(int n) {
	if (n > syscall_num) return 0;
	DEBUG("Get syscall %d from %s\r\n",n,thread_getThread(thread_getCurrent())->name);
	return syscall_table[n];
}
static void sys_exit(int exitCode) {
    DEBUG("sys_exit: %d\r\n",exitCode);
    thread_killThread(thread_getThread(thread_getCurrent()),exitCode);
}
static void sys_kill(int pid,int sig) {
	if (sig == 9 || sig == 0) {
		thread_killThread(thread_getThread(pid),-9);
		arch_reschedule();
		return;
	}
	if (sig >= 32) {
		kprintf("%s: doesn't supported signal: %d\r\n",__func__,sig);
		return;
	}
	process_t *prc = thread_getThread(pid);
	if (!prc) {
		kprintf("%s: cannot find process %d!\r\n",__func__,pid);
		return;
	}
	queue_t *que = (queue_t *)prc->signalQueue;
	enqueue(que,(void *)sig);
	arch_reschedule();
}
static int sys_getpid() {
    return thread_getCurrent();
}
static int sys_open(char *path,int flags) {
    // get caller
    // Finnaly i found this fucking bug
    process_t *caller = thread_getThread(thread_getCurrent());
    char *path_copy = strdup(path);
    vfs_node_t *node = vfs_find(path_copy);
    if (!node && flags == 7) {
	strcpy(path_copy,path);
        node = vfs_creat(vfs_getRoot(),path_copy,flags);
    }
    kfree(path_copy);
    return thread_openFor(caller,node);
}
static void sys_close(int fd) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *d = caller->fds[fd];
    vfs_close(d->node);
    if (d->node->flags == 4) {
	Socket *s = (Socket *)d->node->priv_data;
	socket_destroy(s);
        kfree(d->node);
    }
    kfree(d);
    caller->fds[fd] = NULL;
}
static int sys_read(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    DEBUG("Read for %s, file descriptor %d\n",caller->name,_fd);
    file_descriptor_t *fd = caller->fds[_fd];
    if (fd == NULL || fd->node == NULL) {
	    kprintf("WARRNING: read: invalid FD passed\n");
	    return 0;
	}
    if (fd->node->flags == 4) {
	   // kprintf("WARRNING: Redirect from read to recv!\n");
	    Socket *sock = (Socket *)fd->node->priv_data;
	    int socksize = sock->recv(sock,_fd,buff,size,0);
	    return socksize;
	}
    int how = vfs_read(fd->node,fd->offset,size,buff);
    fd->offset+=how;
    if (offset >= fd->node->size) {
        return 0;
    }
    return how;
}
static int sys_write(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    if (fd == NULL || fd->node == NULL) {
	    kprintf("WARRNING: write: invalid FD passed\n");
	    kprintf("Exit from sys_write!\r\n");
	    return 0;
	}
    if (fd->node->flags == 4) {
	    //kprintf("WARRNING: redirecting from write to send!\n");
	    Socket *sock = (Socket *)fd->node->priv_data;
	    //kprintf("Send from %s to socket owner: %s\n",caller->name,thread_getThread(sock->conn->owner)->name);
	    int socksize = sock->send(sock,_fd,buff,size,0);
	    return socksize;
	}
    int wr = vfs_write(fd->node,fd->offset,size,buff);
    fd->offset+=wr;
    return wr;
}
static void sys_signal(int signal,int handler) {
	if (signal >= 32) {
		kprintf("Signal is out of bounds!\r\n");
		return;
	}
	if (handler == 0) {
		kprintf("Handler is out of bounds!\r\n");
		return;
	}
	process_t *prc = thread_getThread(thread_getCurrent());
	if (!prc) {
		kprintf("Now such process!\r\n");
		return;
	}
	prc->signal_handlers[signal] = handler;
	//kprintf("%s: registred for %d to 0x%x\r\n",__func__,signal,handler);
}
static void sys_sigexit() {
	process_t *caller = thread_getThread(thread_getCurrent());
	//kprintf("%s: caller -> %s\r\n",__func__,caller->name);
	arch_exitSignal(caller);
	arch_reschedule();
	// what?
	PANIC("Serious kernel bug");
}
static int sys_exec(char *path,int argc,char **argv) {
    // Yeah it's can be stupid, but the buff kmalloc is overwriting our argv array
    if (!path || !isAccessable(path)) return -1;
    int m = strlen(path);
    char *buff = kmalloc(m+1);
    strcpy(buff,path);
    vfs_node_t *file = vfs_find(buff);
    if (!file) {
        kfree(buff);
        return -1;
    }
    process_t *caller = thread_getThread(thread_getCurrent());
    if (caller->pid == 0) caller = NULL;
    arch_cli();
    clock_setShedulerEnabled(false);
    int len = file->size;
    aspace_t *original = (caller != NULL ? caller->aspace : NULL);
    arch_mmu_switch(arch_mmu_getKernelSpace());
    void *file_buff = kmalloc(len+1);
    vfs_read(file,0,len,file_buff);
    if (caller != NULL )caller->aspace = arch_mmu_newAspace();
    elf_load_file(file_buff,caller);
    arch_mmu_switch(arch_mmu_getKernelSpace());
    process_t *prc = (caller != NULL ? caller : thread_getThread(thread_getNextPID()-1));
    // Change name to actual file name
    thread_changeName(prc,file->name);
    kfree(file_buff);
    kfree(buff);
    kprintf("%s: begin of user process argument copy...",__func__);
    if (caller != NULL )arch_mmu_switch(original);
    vfs_close(file);
    char **new_argv = NULL;
    if (argc == 0 || argv == 0) {
        // Передамо звичайні параметри(ім'я файлу і т.д)
	arch_mmu_switch(prc->aspace);
        new_argv = sbrk(prc,2);
        new_argv[0] = strdup_user(prc,path);
	arch_mmu_switch(arch_mmu_getKernelSpace());
        argc = 1;
    } else {
        // We need to copy arguments from caller to prevent #PG when process is exitng!
	// 14/04/2024: We need to switch to process address space to prevent #PG when process is trying to access it's own arguments.
	// Also required if current platform supports mapping only active page directory(x86,x86_64 for example)
	// We need to somehow access argv from ANOTHER address space.
	// Need to be optimized.
	char **krnArgv = kmalloc(sizeof(char *) * (argc + 1));
	for (int i = 0; i < argc; i++) {
		krnArgv[i] = strdup(argv[i]);
	}
	arch_mmu_switch(prc->aspace);
	new_argv = sbrk(prc,sizeof(char *) * (argc + 1));
	for (int i = 0; i < argc; i++) {
		new_argv[i] = strdup_user(prc,krnArgv[i]);
	}
	arch_mmu_switch( arch_mmu_getKernelSpace());
	for (int i = 0; i < argc; i++) {
		kfree(krnArgv[i]);
	}
	kfree(krnArgv);
    }
    kprintf("done\r\n");
    arch_putArgs(prc,argc,new_argv);
    if (caller != NULL )arch_mmu_destroyAspace(original,false);
    DEBUG("Used kheap after exec: %dKB\r\n",alloc_getUsedSize());
    clock_setShedulerEnabled(true);
    arch_reschedule();
    return prc->pid;
}
static int sys_reboot(int reason) {
	if (thread_getThread(thread_getCurrent())->uid != 0) {
		return 13;
	}
	if (reason == POWEROFF_MAGIC) {
		// Poweroff all POWER devices
		for (dev_t *dev = dev_getRoot(); dev != NULL; dev = dev->next) {
			if ((dev->type & DEVFS_TYPE_POWER) == DEVFS_TYPE_POWER) {
				if (dev->poweroff != NULL) {
					kprintf("Powering off device %s...",dev->name);
					kprintf(dev->poweroff(dev->devNode) ? "done" : "fail");
					kprintf("\r\n");
				}
			}
		}
		arch_poweroff();
		while(1) {}
	}
	arch_reset();
	return 0; // what?
}
static int sys_chdir(char *to) {
    process_t *prc = thread_getThread(thread_getCurrent());
    int size = strlen(to);
    char *copy = kmalloc(size+1);
    strcpy(copy,to);
    vfs_node_t *node = vfs_find(copy);
    if (!node) {
        kfree(copy);
	return -1;
    }
    prc->workDir = node;
    kfree(copy);
    return 0;
}
static void sys_pwd(char *buff,int len) {
    vfs_node_path(thread_getThread(thread_getCurrent())->workDir,buff,len);
}
static int sys_opendir(char *path) {
    return sys_open(path,0);
}
static void sys_closedir(int fd) {
    sys_close(fd);
}
static void *sys_readdir(int fd,int p) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *_fd = caller->fds[fd];
    return vfs_readdir(_fd->node,p);
}
static int sys_mount(char *dev,char *mount_point,char *fs) {
    char *dev_copy = strdup(dev);
    char *mountptr = strdup(mount_point);
    char *f = strdup(fs);
    vfs_node_t *de = vfs_find(dev_copy);
    process_t *prc = thread_getThread(thread_getCurrent());
    if (!de) {
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -1;
    }
    vfs_fs_t *fol = vfs_findFS(f);
    if (!fol) {
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -2;
    }
    aspace_t *aspace = arch_mmu_getAspace();
    prc->aspace = arch_mmu_getKernelSpace();
    arch_mmu_switch(prc->aspace);
   // kprintf("DEBUG! %s: fol: %x, de: %x, mountptr: 0x%x\n",__func__,fol,de,mountptr);
   // kprintf("Calling vfs_mount\n");
    if (!vfs_mount(fol,de,mountptr)) {
	    kprintf("Return of vfs_mount with error -3\r\n");
	    kfree(dev_copy);
	    kfree(mountptr);
	    kfree(f);
	    arch_mmu_switch(aspace);
	    prc->aspace = aspace;
	    return -3;
    }
    kfree(dev_copy);
    kfree(mountptr);
    kfree(f);
    prc->aspace = aspace;
    arch_mmu_switch(aspace);
    return 0;
}
static int sys_waitpid(int pid) {
    // Most important syscall!
    process_t *parent = thread_getThread(thread_getCurrent());
    if (pid > 0) {
    // Check if the given process has been spawned by the current caller
    struct process *child = thread_getThread(pid);

    DEBUG("sys_waitpid called from %s to wait for %s\r\n", parent->name, child->name);

    if (child != NULL && child->parent == parent) {
        thread_waitPid(parent);

        // Wait for the child process to start and die
        while (!child->started || !child->died) {
            // Perform a blocking wait until the child process status changes
            arch_reschedule();
        }
    }
} else if (pid < 0) {
	// wait for any child that has been spawned by the fucking process
	thread_waitPid(parent);
	while(parent->state == STATUS_WAITPID) {
		arch_reschedule();
	}
	return parent->died_child;
}
	return 0;
}
static int sys_getppid() {
    process_t *prc = thread_getThread(thread_getCurrent());
    if (prc->parent != NULL) {
        return prc->parent->pid;
    }
    return -1;
}
static int sys_getuid() {
     process_t *prc = thread_getThread(thread_getCurrent());
    if (!prc) return 0;
    return prc->uid;
}
static void sys_setuid(int uid) {
     process_t *prc = thread_getThread(thread_getCurrent());
    if (!prc) return;
    prc->uid = uid;
}
static int sys_seek(int _fd,int type,int how) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    if (type == 0) {
        fd->offset = how;
    } else if (type == 2) {
        fd->offset = fd->node->size;
    } else if (type == 1) {
    	return fd->offset;
    }
    return 0;
}
static int sys_tell(int _fd) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    return fd->offset;
}
static void *sys_mmap(int _fd,int addr,int size,int offset,int flags) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    vfs_node_t *node = fd->node;
    if (node != NULL) {
        return vfs_mmap(node,addr,size,offset,flags);
    }
    return NULL;
}
static int sys_insmod(char *path) {
    int len = strlen(path);
    char *copy = kmalloc(len+1);
    strcpy(copy,path);
    vfs_node_t *module = vfs_find(copy);
    if (!module) {
        kfree(copy);
        return -1;
    }
    kfree(copy);
    aspace_t *aspace = arch_mmu_getAspace();
    arch_cli();
    clock_setShedulerEnabled(false);
    arch_mmu_switch(arch_mmu_getKernelSpace());
    // allocate space that need to be used
    void *m = kmalloc(module->size+elf_get_module_bytes(module));
    if (!m) {
        clock_setShedulerEnabled(true);
        arch_mmu_switch(aspace);
        return -2;
    }
    vfs_read(module,0,module->size,m);
    // Load
    module_t *mod = load_module(m);
    if (!mod) {
	kfree(m);
        arch_mmu_switch(aspace);
        clock_setShedulerEnabled(true);
        return -2;
    }
    kprintf("Module %s load address(used for GDB): 0x%x\n",mod->name,mod->load_address);
    // call the module entry point
    if (mod->init) {
	//kprintf("sys_insmod: calling entry point of module\n");
        mod->init(mod);
    }
    kfree(m);
    clock_setShedulerEnabled(true);
    arch_sti();
    arch_mmu_switch(aspace);
    //kprintf("syscall: exit from sys_insmod\n");
    return 0;
}
static void sys_rmmod(char *name) {
    kprintf("Comming soon!\r\n");
}
static int sys_ioctl(int fd,int req,va_list list) {
    // Generally used by the compositor to draw windows!
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *file_desc = caller->fds[fd];
    if (file_desc == NULL || file_desc->node == NULL) return -1;
    // Sanity check
    if (file_desc->node->fs == NULL|| file_desc->node->fs->ioctl == NULL) return -1;
    return file_desc->node->fs->ioctl(file_desc->node,req,list);
}
static void sys_setgid(int gid) {
    process_t *prc = thread_getThread(thread_getCurrent());
    if (!prc) return;
    prc->gid = gid;
}
static int sys_getgid() {
    process_t *prc = thread_getThread(thread_getCurrent());
    if (!prc) return 0;
    return prc->gid;
}
static void *sys_sbrk(int increment) {
    process_t *prc = thread_getThread(thread_getCurrent());
    if (!prc) return (void *)-1;
    void *ptr = sbrk(prc,increment);
    DEBUG("Ptr -> 0x%x\r\n",ptr);
    return ptr;
}
static int sys_dup(int oldfd,int newfd) {
	process_t *prc = thread_getThread(thread_getCurrent());
	if (!prc) return -1;
	// Duplicate there FD
	file_descriptor_t *fd = prc->fds[oldfd];
	if (!fd) return -1;
	file_descriptor_t *copy = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
	if (newfd > 0) {
		// oldfd = newfd, copy the newfd file descriptor
		file_descriptor_t *nefd = prc->fds[newfd];
		// replace vfs node of oldfd to vfs node of newfd
		fd->node = nefd->node;
		return oldfd;
	}
	// We don't clear memory because we gonna copy it
	memcpy(copy,fd,sizeof(file_descriptor_t));
	// Incrmenet the next FD pointer
	int fd_id = prc->next_fd;
	prc->next_fd++;
	prc->fds[fd_id] = copy;
	return fd_id;
}
static int sys_clone(int entryPoint,int arg1,int arg2) {
    process_t *caller = thread_getThread(thread_getCurrent());
    // Acording to the wiki, we need just to create an process with the same VM space
    process_t *thread = thread_create("thread",entryPoint,true);
    // Change the address space
    // TODO: Add support for other flags
    thread->aspace = caller->aspace;
    thread->type = TYPE_THREAD;
    thread->brk_begin = caller->brk_begin;
    thread->brk_end = caller->brk_end;
    thread->brk_next_unallocated_page_begin = caller->brk_next_unallocated_page_begin;
    // Clone the file descriptors
    for (int i = 0; i < caller->next_fd; i++) {
	    file_descriptor_t *desc = caller->fds[i];
	    if (desc != NULL) {
		    thread_openFor(thread,desc->node);
		}
	}
    // Pass test arguments
    pthread_str *st = (pthread_str *)kmalloc(sizeof(pthread_str));
    memset(st,0,sizeof(pthread_str));
    // by mapping, the arg1 is actual entry point, and arg2 is an argument to thread function
    st->entry = arg1;
    st->arg = (void *)arg2;
    arch_putArgs(thread,1,(char **)(int)st); // Only test
    return thread->pid;
}

static void sys_waitForStart(int pid) {
	process_t *waitFor = thread_getThread(pid);
	if (waitFor->pid == 0) {
		// Returned when no such process ID(PID)
		return;
	}
	arch_sti(); // enable interrupts, so scheduler can work normally
	// Wait
	while(!waitFor->started);
}

static void sys_usleep(int ms) {
	arch_cli();
	process_t *caller = thread_getThread(thread_getCurrent());
	caller->wait_time = ms;
	caller->state = STATUS_SLEEP;
	caller->quota = PROCESS_QUOTA;
	arch_sti();
	arch_reschedule();
}

static int sys_truncate(int fd,int size) {
	process_t *prc = thread_getThread(thread_getCurrent());
	file_descriptor_t *desc = prc->fds[fd];
	if (!desc) return -1;
	vfs_truncate(desc->node,size);
	return 0;
}

static int sys_socket(int domain,int type,int protocol) {
	Socket *s = kmalloc(sizeof(Socket));
	memset(s,0,sizeof(Socket));
	if (!socket_create(domain,s)) {
		kfree(s);
		return -1;
	}
	s->owner = thread_getCurrent();
	// Create and open node
	vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
	memset(n,0,sizeof(vfs_node_t));
	n->flags = 4; // socket FD instance, destroy at close
	n->priv_data = s;
	return thread_openFor(thread_getThread(thread_getCurrent()),n);
}
static int sys_bind(int sockfd,struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
        Socket *s = desc->node->priv_data;
        return s->bind(s,sockfd,addr,len);
}
static int sys_listen(int sockfd,int backlog) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
        Socket *s = desc->node->priv_data;
        return s->listen(s,sockfd,backlog);
}
static int sys_accept(int sockfd,struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
        Socket *s = desc->node->priv_data;
        return s->accept(s,sockfd,addr,(socklen_t *)&len);
}
static int sys_connect(int sockfd,const struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
        Socket *s = desc->node->priv_data;
        return s->connect(s,sockfd,addr,(socklen_t *)&len);
}
static int sys_send(int sockfd,const void *buff,int len,int flags) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
        Socket *s = desc->node->priv_data;
        return s->send(s,sockfd,buff,len,flags);
}
static int sys_recv(int sockfd,void *buff,int len,int flags) {
	process_t *caller = thread_getThread(thread_getCurrent());
	file_descriptor_t *desc = caller->fds[sockfd];
	if (desc == NULL || desc->node == NULL || desc->node->flags != 4) return -1;
	Socket *s = desc->node->priv_data;
	return s->recv(s,sockfd,buff,len,flags);
}

static int sys_ready(int fd) {
	process_t *t = thread_getThread(thread_getCurrent());
	file_descriptor_t *ft = t->fds[fd];
	if (ft != NULL) {
		if (ft->node->flags == 4) {
			// Check if socket is ready for connections
			Socket *sock = (Socket *)ft->node->priv_data;
			if (sock->acceptQueue != NULL) {
				if (sock->acceptQueue->size != 0) {
					return 3;
				} else return 2;
			} else if (sock->isReady) {
				return (sock->isReady(sock) ? 3 : 2);
			} else {
				return 2;
			}
		}
	}
	return (vfs_isReady(ft->node) ? 3 : 2);
}
/*
 * Copy-On-Write implementation of fork system call
 *
 * This sytem call is used as a part of fork-exec model on UNIX systems
 *
 * Return value is child PID.
*/
static int sys_fork() {
	//i Firstly we need to clone parents process_t structure
	process_t *parent = thread_getThread(thread_getCurrent());
	// Clone!
 	aspace_t *orig = arch_mmu_getAspace();
	arch_mmu_switch(arch_mmu_getKernelSpace());
	process_t *child =  thread_create(parent->name,arch_syscall_getCallerRet(),true);
	child->state = STATUS_CREATING;
	aspace_t *space = arch_mmu_newAspace();
	arch_mmu_duplicate(orig,space);
	// Arch specific code for fork
    arch_forkProcess(parent,child);
	child->aspace = space;
	//arch_cloneStack(parent,child);
	// Clone the FD's
	for (int i = 0; i < parent->next_fd; i++) {
		file_descriptor_t *desc = parent->fds[i];
		if (desc != NULL && desc->node != NULL) {
			thread_openFor(child,desc->node);
		}
	}
	// sys_sbrk
	child->brk_begin = parent->brk_begin;
 	child->brk_end = parent->brk_end;
    	child->brk_next_unallocated_page_begin = parent->brk_next_unallocated_page_begin;
	child->parent = parent;
	parent->child = child;
	child->state = STATUS_RUNNING; // yeah
	arch_mmu_switch(orig);
	return child->pid;
}

static int sys_uname(struct utsname *name) {
    if (name == NULL || !name) {
        return -1;
    }
    process_t *caller = thread_getThread(thread_getCurrent());
    arch_mmu_switch(caller->aspace);
    name->sysname = kmalloc_user(caller,6);
    strcpy(name->sysname,"Helin");
    name->release = strdup_user(caller,OS_RELEASE);
    name->version = strdup_user(caller,OS_VERSION);
    name->machine = strdup_user(caller,arch_getName());
    return 0;
}
static int sys_sethostname(char *name,int len) {
	if (name == NULL || len == 0) return -1;
	if (system_hostname != NULL) {
		kfree(system_hostname);
	}
	system_hostname = strdup(name);
	return 0;
}
static int sys_gethostname(char *name,int len) {
	memcpy(name,system_hostname,len-1);
	name[len] = 0;
	return 0;
}

static int sys_creat(char *path,int type) {
    // Used to create directory, yes i don't know why i named it as sys_creat
    if (path == NULL ) return -1; // EINVL
    vfs_node_t *in = NULL;
    process_t *caller = thread_getThread(thread_getCurrent());
    if (path[0] != '/') {
        in = caller->workDir;
    } else {
        return -2; // not implemented :(
    }
    vfs_node_t *node = vfs_creat(in,path,VFS_DIRECTORY);
    if (!node) {
        return -3;
    }
    return 0;
}

static int sys_sync(int fd) {
    return -1;
}

static int sys_munmap(void *ptr,int size) {
    return -1;
}
static int sys_umount(char *mountpoint) {
	// TODO: Add mount table.
    return -1;
}

static int sys_access(int _fd) {
	// Check if the process can access the file pointed by fd
	return 0;
}
static int sys_chmod(int fd,int mode) {
	process_t *caller = thread_getThread(thread_getCurrent());
	file_descriptor_t *desc = caller->fds[fd];
	if (!desc) {
		return 2; // errno ENON
	}
	vfs_node_t *node = desc->node;
	// We need some security thinks, but later.
	node->mask = mode;
	return 0;
}
static int sys_stat(int fd,struct stat *stat) {
	process_t *prc = thread_getThread(thread_getCurrent());
	// Find the file descriptor.
	file_descriptor_t *dsc = prc->fds[fd];
	if (fd >= prc->next_fd) return -1;
	if (dsc == NULL) return -1 /* TODO: Return actual errno */;
	/* Return some peace of information that currently available */
	if (stat == NULL) return -1;
	// VFS must process that piece of request?
	if (dsc->node->fs->stat == NULL) return 38; // not implemented
	return dsc->node->fs->stat(dsc->node,stat);
}
static int sys_unlink(char *path) {
	char *path_copy = strdup(path);
	vfs_node_t *node = vfs_find(path_copy);
	if (!node) {
		kfree(path_copy);
		return 2;
	}
	if (!node->fs || !node->fs->rm) {
		return 22;
	}
	if (!node->fs->rm(node)) {
		return 22;
	}
	return 0;
}

static int sys_gettime(int clock,struct tm *time) {
	if (clock != 0) {
		return 22;
	}
#ifdef HWCLOCK
	hw_clock_get(time);
#endif
	return 0;
}

static int sys_settime(int clock,struct tm *time) {
	if (clock != 0) {
		return 22;
	}
	// settime requires root as process uid
	process_t *caller = thread_getThread(thread_getCurrent());
	if (caller->uid != 0) {
		return 13;
	}
#ifdef HWCLOCK
	hw_clock_set(time);
#endif
	return 0;
}

static int sys_syslog(int type,char *buff,int len) {
	kprintf("%s: kernel ring ptr: 0x%x\r\n",__func__,0xaa);
	//output_write(ringBuff);
	return 0;
}

int sys_chroot(char *path) {
	return 38;
}

int sys_fchdir(int _fd) {
	process_t *caller = thread_getThread(thread_getCurrent());
	file_descriptor_t *fd = caller->fds[_fd];
	if (fd == NULL) {
		return 2;
	}
	caller->workDir = fd->node;
	return 0;
}
int sys_fchown(int fd,int owner,int group) {
	return 38;
}
int sys_chown(int mode,char *path,int owner,int group) {
	return 38;
}
int sys_rm(int mode,char *path) {
	// yes
	vfs_node_t *obj = vfs_find(path);
	if (obj == NULL) {
		return 2;
	}
	if (obj->fs == NULL || obj->fs->rm == NULL) {
		return 5;
	}
	obj->fs->rm(obj);
	return 0;
}
int sys_getpgid(int pid) {
	return 0;
}
int sys_nice(int prio) {
	process_t *caller = thread_getThread(thread_getCurrent());
	caller->priority = prio;
	return 0;
}
int sys_symlink(char *target,char *path) {
	return 38;
}
