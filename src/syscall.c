/*
 * Syscall realization.
 *
 * We are currently monolitic kernel, so here we implement all list of required syscalls.
*/


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
#include <ipc/ipc_manager.h>
#include <dev/fb.h>
#include <resources.h>
#include <symbols.h>
#include <arch/elf.h>
// Pthreads structure that passed to newlib thread_entry
typedef struct _pthread_str {
	uintptr_t entry;
	void *arg;
} pthread_str;

struct utsname {
    char *sysname;    /* Operating system name (e.g., "Linux") */
    char *nodename;   /* Name within communications network */
    char *release;
    char *version;
    char *machine;
};
struct statfs {
	char f_mntfromname[90];
	char f_mnttoname[90];
	char f_fstypename[90];
};
struct timespec {
	long tv_sec;
	long tv_usec;
};
struct sigaction {
        void (*sa_handler)(int);
        unsigned long sa_mask;
        int sa_flags;
};
// stat.
extern char *ringBuff;
char system_hostname[255];
extern queue_t *priority[6];
extern process_t *runningTask; // sheduler?
static void sys_default() {}
static void sys_exit(int exitCode);
static int sys_kill(int pid,int code);
static int sys_getpid();
static int sys_open(char *path,int flags);
static int sys_close(int fd);
static int sys_read(int fd,int offset,int size,void *buff);
static int sys_write(int fd,int offset,int size,void *buff);
// This 3 syscalls need to be changed.
// TODO: Reformate this syscalls into clock_* syscalls.
static void sys_signal(int sig,int handler);
static void sys_sigexit();
static int sys_fstat(int fd,struct stat *s);
// End of deprecated system calls. Need reform. Never use it.
static int sys_exec(char *path,int argc,char **argv,char **environ);
static int sys_reboot(int reason);
static int sys_chdir(char *to);
static int sys_pwd(char *buff,int len);
static int sys_opendir(char *path);
static void sys_closedir(int fd);
static int sys_readdir(int fd,int p,void *dirPtr);
static int sys_mount(char *dev,char *mount_point,char *fs);
static int sys_waitpid(int pid,int *status,int options);
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
// if duptype is 1 then this is dup(), if duptype is 2 then it is dup2()
static int sys_dup(int oldfd,int newfd,int duptype);
static int sys_clone(void *stack,uintptr_t stackSize); // pthread support, yay
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
static int sys_access(char *path,int mode);
static int sys_fchmod(int fd,int mode);
static int sys_stat(const char *path,struct stat *);
static int sys_unlink(char *path);
static int sys_clock(int clock,uint64_t *out);
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
static int sys_ipc(int cmd,int magicID,int ipcCmd,void *args);
static int sys_getfsstat(struct statfs *buf,long bufsize,int mode);
static int sys_getrlimit(int resID,rlimit_t *to);
static int sys_setrlimit(int resID,rlimit_t *to);
static int sys_getrusage(int resID,void *to);
static int sys_openat(int dirfd,char *path,int mode);
static unsigned long sys_sysconf(int id);
static int sys_fcntl(int fd,int cmd,long long arg);
static int sys_mprotect(void *addr,size_t len,int prot);
static int sys_mkdirat(int dirfd,const char *name,int mode);
static int sys_mknod(const char *name,int mode,int devid);
static int sys_mknodat(int dirfd,const char *name,int mode,int devid);
static int sys_utime(int fd,struct timespec **times,int flags);
static int sys_symlinkat(int dirfd,char *from,char *to);
static int sys_readlink(char *path,char *buff,int size);
static int sys_readlinkat(char *path,int dirfd,char *buff,int size);
// Finally. More signal support.
static int sys_sigwait(unsigned long *sigset,int *sig);
static int sys_chmod(const char *path,int mode);
static int sys_fchmodat(int dirfd,const char *path,int fd);
static int sys_unlinkat(int dirfd,const char *name);
// Advanced signal IPC support.
static int sys_sigaction(int sig,const struct sigaction *act,struct sigaction *oldact);
static int sys_signalstack(struct sigStack *ss,struct sigStack *old_ss);
static void *kmalloc_user(process_t *prc,int size);
uintptr_t syscall_table[94] = {
    (uintptr_t)sys_default,
    (uintptr_t)sys_fstat,
    (uintptr_t)sys_exit,
    (uintptr_t)sys_kill,
    (uintptr_t)sys_getpid,
    (uintptr_t)sys_stat,
    (uintptr_t)sys_unlink,
    (uintptr_t)sys_open,
    (uintptr_t)sys_close,
    (uintptr_t)sys_read,
    (uintptr_t)sys_write,
    (uintptr_t)sys_signal,
    (uintptr_t)sys_sigexit,
    (uintptr_t)sys_exec,
    (uintptr_t)sys_reboot,
    (uintptr_t)sys_symlink,
    (uintptr_t)sys_pwd,
    (uintptr_t)sys_chdir,
    (uintptr_t)sys_opendir,
    (uintptr_t)sys_closedir,
    (uintptr_t)sys_readdir,
    (uintptr_t)sys_mount,
    (uintptr_t)sys_waitpid,
    (uintptr_t)sys_getppid,
    (uintptr_t)sys_sync,
    (uintptr_t)sys_getuid,
    (uintptr_t)sys_setuid,
    (uintptr_t)sys_seek,
    (uintptr_t)sys_tell,
    (uintptr_t)sys_mmap,
    (uintptr_t)sys_insmod,
    (uintptr_t)sys_rmmod,
    (uintptr_t)sys_ioctl,
    (uintptr_t)sys_setgid,
    (uintptr_t)sys_getgid,
    (uintptr_t)sys_sbrk,
    (uintptr_t)sys_dup,
    (uintptr_t)sys_clone,
    (uintptr_t)sys_waitForStart,
    (uintptr_t)sys_usleep,
    (uintptr_t)sys_truncate,
    (uintptr_t)sys_socket,
    (uintptr_t)sys_bind,
    (uintptr_t)sys_listen,
    (uintptr_t)sys_accept,
    (uintptr_t)sys_connect,
    (uintptr_t)sys_send,
    (uintptr_t)sys_recv,
    (uintptr_t)sys_ready,
    (uintptr_t)sys_fork,
    (uintptr_t)sys_uname,
    (uintptr_t)sys_sethostname,
    (uintptr_t)sys_gethostname,
    (uintptr_t)sys_creat,
    (uintptr_t)sys_munmap,
    (uintptr_t)sys_umount,
    (uintptr_t)sys_access,
    (uintptr_t)sys_fchmod,
    (uintptr_t)sys_clock,
    (uintptr_t)sys_settime,
    (uintptr_t)sys_syslog,
    (uintptr_t)sys_chroot,
    (uintptr_t)sys_fchdir,
    (uintptr_t)sys_chown,
    (uintptr_t)sys_fchown,
    (uintptr_t)sys_rm,
    (uintptr_t)sys_getpgid,
    (uintptr_t)sys_nice,
    (uintptr_t)sys_symlinkat,
    (uintptr_t)sys_ipc,
    (uintptr_t)sys_getfsstat,
    (uintptr_t)sys_getrlimit,
    (uintptr_t)sys_setrlimit,
    (uintptr_t)sys_getrusage,
    (uintptr_t)sys_openat,
    (uintptr_t)sys_sysconf,
    (uintptr_t)sys_fcntl,
    (uintptr_t)sys_mprotect,
    (uintptr_t)sys_mkdirat,
    (uintptr_t)sys_mknod,
    (uintptr_t)sys_mknodat,
    (uintptr_t)sys_utime,
    (uintptr_t)sys_default, // sys_link
    (uintptr_t)sys_readlink,
    (uintptr_t)sys_readlinkat,
    (uintptr_t)sys_sigwait,
    (uintptr_t)sys_chmod,
    (uintptr_t)sys_fchmodat,
    (uintptr_t)sys_unlinkat,
    (uintptr_t)sys_sigaction,
    (uintptr_t)sys_signalstack,
};
int syscall_num = 94;
extern char *ringBuff;
extern int ringBuffSize;
extern int ringBuffPtr;
extern bool disableOutput;
extern void push_prc(process_t *prc); // src/thread.c
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
/*static bool checkPtr(void *ptr) {
  return ((uintptr_t)ptr < KERN_HEAP_BEGIN) || ((uintptr_t)ptr > KERN_HEAP_END);
}*/
static int stat_convMode(vfs_node_t *node) {
        int mode = 0100000;
        if ((node->flags & VFS_DIRECTORY) == VFS_DIRECTORY) {
                mode = 040000;
        } else if ((node->flags & VFS_BLOCK) == VFS_BLOCK) {
                mode = 060000;
        }
        return mode;
}
static int sys_fstat(int fd,struct stat *stat) {
  process_t *caller = thread_getThread(thread_getCurrent());
  file_descriptor_t *f = caller->fds[fd];
  if (f == NULL) {
    return -EBADF;
  }
  if (stat == NULL) {
    return -EBADF;
  }
  memset(stat,0,sizeof(struct stat));
  vfs_node_t *node = f->node;
  stat->st_ino = node->inode;
  stat->st_mode = node->mask;
  stat->st_mode |= stat_convMode(node);
  stat->st_uid = node->uid;
  stat->st_gid = node->gid;
  stat->st_size = node->size;
  stat->st_mtime = node->mtime;
  stat->st_ctime = node->ctime;
  stat->st_atime = node->atime;
  return 0;
}
bool isAccessable(void *ptr) {
    return arch_mmu_getPhysical(ptr) != 0;
}
void syscall_init() {
    // redirect to arch specific init code
    arch_syscall_init();
}
uintptr_t syscall_get(int n) {
	if (n > syscall_num) return 0;
	//DEBUG("Get syscall %d from %s\r\n",n,thread_getThread(thread_getCurrent())->name);
	return syscall_table[n];
}
static void sys_exit(int exitCode) {
    DEBUG("sys_exit: %d\r\n",exitCode);
    thread_killThread(thread_getThread(thread_getCurrent()),exitCode,true);
}
static int sys_kill(int pid,int sig) {
	//			    ta
	if (sig == 9 || sig == 0) {
		// Well, the previous code has no check if the thread_getThread will return 0.
		process_t *thToKill = thread_getThread(pid);
                if (thToKill == NULL ||  !thToKill->userProcess) {
                        return -ESRCH;
                }
		if (thToKill->died || thToKill->state == STATUS_ZOMBIE) {
			return -ESRCH;
		}
		thread_killThread(thToKill,-9,true);
		if (runningTask == thToKill)arch_reschedule();
		return 0;
	}
	if (sig >= 32) {
		return -EINVAL;
	}
	process_t *prc = thread_getThread(pid);
	if (!prc) {
		return -ESRCH;
	}
	if (prc->died || prc->state == STATUS_ZOMBIE) return -ESRCH;
	  queue_t *que = (queue_t *)prc->signalQueue;
	  enqueue(que,(void *)sig);
  	if (prc->state != STATUS_RUNNING && prc->state != STATUS_CREATING) {
		    prc->state = STATUS_RUNNING;
		    push_prc(prc);
  	}
    // Default handling?
    ProcessSignal hndlr = prc->signal_handlers[sig];
    if (hndlr.handler == 0) {
        switch(sig) {
          case 1:
          thread_killThread(prc,-sig,true);
          break;
        }
    }
  thread_forceSwitch();
  return 0;
}
static int sys_getpid() {
    return thread_getCurrent();
}
static int sys_open(char *path,int flags) {
    // get caller
    // Finnaly i found this fucking bug
    process_t *caller = thread_getThread(thread_getCurrent());
    if (strlen(path) == 0) {
      // this shit destroys our kernel.
      return -ENOENT;
    }
    char *path_copy = strdup(path);
    vfs_node_t *node = NULL;
    if (strcmp(path_copy,".")) {
	    node = caller->workDir;
	} else {
		node = vfs_find(path_copy);
	}
    // Check permissions.
    kfree(path_copy);
    if (node != NULL) {
	    if (!vfs_hasPerm(node,PERM_READ,caller->gid,caller->uid)) {
	    	return -EACCES;
    	}
            // Check if we don't open directory if not required to.
            if ((flags & 0x200000) != 0x200000) {
                    if ((node->flags & VFS_DIRECTORY) == VFS_DIRECTORY) {
                            return -EISDIR;
                    }
            }
    }
    if (node == NULL) {
	    if (((flags & 2) == 2)
		    || ((flags & 0x200) == 0x200) ||
		    ((flags & 1) == 1)) {
	    // Before any creation, check if file system isn't read only.
	    	int r = sys_creat(path,0);
	    	if (r > 0) {
		    kfree(path_copy);
		    return -ENOENT;
		}
	    // Path is modified by strtok....
	    path_copy = strdup(path);
	    node = vfs_find(path_copy);
	    kfree(path_copy);
	    if (node == NULL) {
		    return -ENOENT;
		}
	} else {
		return -ENOENT;
	}
    } else {
	    // node not found.
	    if (node == NULL) return -ENOENT;
    }
    return thread_openFor(caller,node,flags);
}
static int sys_close(int fd) {
    process_t *caller = thread_getThread(thread_getCurrent());
    if (fd < 0 || fd >= 200) return -EBADF;
    file_descriptor_t *d = caller->fds[fd];
    if (!isAccessable(d) || !isAccessable(d->node) || !isAccessable(d->node->fs)) return -EBADF;
    if (d == NULL || d->node == NULL) {
	    return -EBADF;
    }
    //kprintf("Closing fd %d: %s\n",fd,d->node->name);
    if (d->node->fs->close != NULL) {
	    if (!isAccessable(d->node->fs->close)) {
		    return -EBADF;
		}
	}
    vfs_close(d->node);
    if ((d->node->flags & VFS_SOCKET) == VFS_SOCKET) {
	Socket *s = (Socket *)d->node->priv_data;
	socket_destroy(s);
        kfree(d->node);
    }
    kfree(d);
    caller->fds[fd] = NULL;
    return 0;
}
static int sys_read(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    DEBUG("Read for %s, file descriptor %d\n",caller->name,_fd);
    if (_fd < 0 || _fd >= 200) return -EBADF;
    file_descriptor_t *fd = caller->fds[_fd];
    if ((uintptr_t)fd < KERN_HEAP_BEGIN || (uintptr_t)fd > KERN_HEAP_END) return -EBADF;
    if ((uintptr_t)fd->node < KERN_HEAP_BEGIN || (uintptr_t)fd->node > KERN_HEAP_END) return -EBADF;
    /*if (!isAccessable(fd)) return -EBADF;
    if (!isAccessable(fd->node) || !isAccessable(fd->node->fs) || !isAccessable(fd->node->fs->read)) return -EBADF;*/
    if (fd == NULL || fd->node == NULL) {
	    kprintf("WARRNING: read: invalid FD passed\n");
	    return -EBADF;
	}
    // Check permissions.
    if (!vfs_hasPerm(fd->node,PERM_READ,caller->gid,caller->uid)) {
	    return -EACCES;
    }
    if ((fd->node->flags & VFS_SOCKET) == VFS_SOCKET) {
	   // kprintf("WARRNING: Redirect from read to recv!\n");
	    Socket *sock = (Socket *)fd->node->priv_data;
	    int socksize = sock->recv(sock,_fd,buff,size,0);
	    return socksize;
	}
    if (offset == 0 && fd->offset > fd->node->size) {
	    return 0;
	} else if (fd->offset+offset >= fd->node->size) {
		return 0;
	}
    int how = vfs_read(fd->node,fd->offset,size,buff);
    fd->offset+=how;
    return how;
}
static int sys_write(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    if (_fd < 0 || _fd >= 200) return -EBADF;
    file_descriptor_t *fd = caller->fds[_fd];
    if ((uintptr_t)fd < KERN_HEAP_BEGIN || ((uintptr_t)fd > KERN_HEAP_END)) return -EBADF;
    if (fd == NULL || fd->node == NULL) {
	    return -EFAULT;
    }
    if ((fd->node->mount_flags & VFS_MOUNT_RO) == VFS_MOUNT_RO) {
	    return -EROFS;
    }
    // Check if we have permissions to do it.
    if (!vfs_hasPerm(fd->node,PERM_WRITE,caller->gid,caller->uid)) {
	    return -EACCES;
    }
    /*if ((fd->flags & FD_RDWR) != FD_RDWR) {
	    return -EACCES;
	}*/
    if ((fd->node->flags & VFS_SOCKET) == VFS_SOCKET) {
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
		return;
	}
	if (handler == 0) {
		return;
	}
	process_t *prc = thread_getThread(thread_getCurrent());
	if (!prc) {
		kprintf("No such process!\r\n");
		return;
	}
	ProcessSignal s = prc->signal_handlers[signal];
	// Setup?
	s.handler = handler;
	prc->signal_handlers[signal] = s;
	//kprintf("%s: registred for %d to 0x%x\r\n",__func__,signal,handler);
}
static void sys_sigexit(int signum) {
	arch_cli();
	process_t *caller = thread_getThread(thread_getCurrent());
	//kprintf("%s: caller -> %s\r\n",__func__,caller->name);
	// We don't care about "default" values in ProcessSignal as some architectures can not use this values at all.
	ProcessSignal *s = &caller->signal_handlers[signum];
        s->blocked = false;
        // Check if we still have some some sort of the signals....
        if (((queue_t*)caller->signalQueue)->size > 0) {
                // We can't currently return until we process all signals.
                uintptr_t n = (uintptr_t)dequeue((queue_t *)caller->signalQueue);
                if (n == signum) {
                        enqueue((queue_t *)caller->signalQueue,(void *)n);
                } else {
                        s->exited = true;
                }
                thread_forceSwitch();
        }
        s->exited = true;
	arch_exitSignal(caller,s);
	//while(1) {}
	arch_reschedule();
	// what?
	PANIC("Serious kernel bug");
}
static void cloneStrArray(char **fromAr,char ***toAr,void *fromAspace,process_t *toP,int ArrSize) {
  if (fromAr == NULL || toAr == NULL || toP == NULL) return;
  char **krnBuff = kmalloc(sizeof(char *) * (ArrSize + 1));
  for (int i = 0; i < ArrSize; i++) {
    if (fromAr[i] == NULL) continue;
    krnBuff[i] = strdup(fromAr[i]);
  }
  arch_mmu_switch(toP->aspace);
  *toAr = sbrk(toP,sizeof(char *) * (ArrSize + 1));
  char **ar = *toAr;
  memset(ar,0,sizeof(char *) * (ArrSize + 1));
  for (int i = 0; i < ArrSize; i++) {
    char *clone = strdup_user(toP,krnBuff[i]);
    ar[i] = clone;
  }
  arch_mmu_switch(arch_mmu_getKernelSpace());
  for (int i = 0; i < ArrSize; i++) {
    kfree(krnBuff[i]);
  }
  kfree(krnBuff);
}
static int sys_exec(char *path,int argc,char **argv,char **environ) {
    // Yeah it's can be stupid, but the buff kmalloc is overwriting our argv array
    if (!path || !isAccessable(path)) return -1;
    int m = strlen(path);
    char *buff = kmalloc(m+1);
    strcpy(buff,path);
    vfs_node_t *file = vfs_find(buff);
    if (!file) {
        kfree(buff);
        return ENOENT;
    }
    kfree(buff);
    // Check if directory.
    /*if ((file->flags & VFS_DIRECTORY) == VFS_DIRECTORY) {
	    return EISDIR;
	}*/
    process_t *caller = thread_getThread(thread_getCurrent());
    if (caller != NULL) {
    	bool hasPerm = vfs_hasPerm(file,PERM_EXEC,caller->gid,caller->uid);
    	if (caller->pid == 0) caller = NULL;
    	if (caller != NULL && !hasPerm) {
	    	// No permission to execute applications.
	    	return 13; //EACCES.
    	}
    }
    // Check if the ELF file is correct and CAN be executed on this platform.
    Elf32_Ehdr *hdr = kmalloc(sizeof(Elf32_Ehdr));
    vfs_read(file,0,sizeof(Elf32_Ehdr),hdr);
    if (!elf_check_file(hdr) || !arch_elf_check_header(hdr)) {
	    vfs_close(file);
	    kfree(buff);
	    kfree(hdr);
	    return ENOEXEC;
    }
    kfree(hdr);
    int len = file->size;
    int environSize = 0;
    aspace_t *realSpace = arch_mmu_getAspace();
    while(environ[environSize] != NULL) {
      uintptr_t space = (uintptr_t)arch_mmu_getPhysical(environ[environSize]);
      if (space == 0 || space < 0x1000) {
        break; // corupted.
      }
      environSize++;
    }
    // Clone argv and env. as we can't access them later.
    char **env_kernel = kmalloc(sizeof(char*) * environSize);
    for (int i = 0; i < environSize; i++) {
	    env_kernel[i] = strdup(environ[i]);
	}
    // The same as argv.
    char **argv_kernel = kmalloc(sizeof(char **)*argc);
    for (int i = 0; i < argc; i++) {
	    argv_kernel[i] = strdup(argv[i]);
	  }
    arch_cli();
    clock_setShedulerEnabled(false);
    arch_mmu_switch(arch_mmu_getKernelSpace());
    if (caller != NULL) {
	    // Destroy previous space.
	    arch_mmu_destroyAspace(caller->aspace,false);
    }
    arch_sti();
    if (!elf_load_file(file,caller)) {
	    if (caller != NULL) {
		    arch_cli();
		    arch_mmu_switch(arch_mmu_getKernelSpace());
		    /* elf_load_file will probably destroy our process.
		     * Because the ELF header itself okay, but we still
		     * got error? Then it's likely I/O error of
		     * drive where we load file from.
		     */
		    // Free the env_kernel and argv_kernel.
		    for (int i = 0; i < environSize; i++) {
			    kfree(env_kernel[i]);
		    }
		    for (int i = 0; i < argc; i++) {
			    kfree(argv_kernel[i]);
		    }
		    kfree(env_kernel);
		    kfree(argv_kernel);
		    arch_reschedule(); // get out of there!
		}
	    return ENOEXEC;
    }
    arch_cli();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    process_t *prc = (caller != NULL ? caller : thread_getThread(thread_getNextPID()-1));
    // Change name to actual file name
    thread_changeName(prc,file->name);
    kfree(buff);
    vfs_close(file);
    char **new_argv = NULL;
    char **newEnviron = NULL;
    if (argc == 0 || argv == 0) {
        	// Передамо звичайні параметри(ім'я файлу і т.д)
  	    	arch_mmu_switch(prc->aspace);
        	new_argv = sbrk(prc,sizeof(char *)*2);
        	new_argv[0] = strdup_user(prc,file->name);
	      	arch_mmu_switch(arch_mmu_getKernelSpace());
        	argc = 1;
    } else {
	    // We need to copy arguments from caller to prevent #PG when process is exitng!
	    // 14/04/2024: We need to switch to process address space to prevent #PG when process is trying to access it's own arguments.
	    // Also required if current platform supports mapping only active page directory(x86,x86_64 for example)
	    // We need to somehow access argv from ANOTHER address space.
	    // Need to be optimized.
	    arch_mmu_switch(prc->aspace);
	    new_argv = sbrk(prc,sizeof(char *)*(argc+1));
	    for (int i = 0; i < argc; i++) {
		    new_argv[i] = strdup_user(prc,argv_kernel[i]);
		    kfree(argv_kernel[i]);
	    }
	    kfree(argv_kernel);
	    new_argv[argc] = NULL;
    }
    // Clone environ.
    arch_mmu_switch(prc->aspace);
    if (environSize == 0) {
	    newEnviron = sbrk(prc,sizeof(char *)*1);
	    newEnviron[0] = NULL;
    } else {
    	newEnviron = sbrk(prc,sizeof(char *)*(environSize+1));
    	for (int i = 0; i < environSize; i++) {
	    	newEnviron[i] = strdup_user(prc,env_kernel[i]);
	    	kfree(env_kernel[i]);
    	}
    	kfree(env_kernel);
	newEnviron[environSize] = NULL;
    }
    arch_mmu_switch(arch_mmu_getKernelSpace());
    arch_putArgs(prc,argc,new_argv,newEnviron);
    //if (caller != NULL )arch_mmu_destroyAspace(original,false);
    DEBUG("Used kheap after exec: %dKB\r\n",alloc_getUsedSize());
    clock_setShedulerEnabled(true);
    thread_forceSwitch();
    arch_reschedule();
    return prc->pid;
}
static int sys_reboot(int reason) {
	if (thread_getThread(thread_getCurrent())->uid != 0) {
		return EACCES;
	}
	arch_cli();
	if (reason == POWEROFF_MAGIC) {
		// Poweroff all POWER devices
		arch_cli();
		disableOutput = false;
		kprintf("Used physical memory before: %d KiB\r\n",alloc_getUsedPhysMem() / 1024);
    		kprintf("kernel: Request power off from userland root process\r\n");
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
        arch_cli();
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
	return -ENOENT;
    }
    prc->workDir = node;
    if (size > 255) {
            size = 254;
        }
    memcpy(prc->cwd,to,size);
    kfree(copy);
    return 0;
}
static int sys_pwd(char *buff,int len) {
	if (buff == NULL) return -22;
    //vfs_node_path(thread_getThread(thread_getCurrent())->workDir,buff,len);
    if (len > 255) {
            len = 255;
        }
    memcpy(buff,thread_getThread(thread_getCurrent())->cwd,len);
    return 0;
}
static int sys_opendir(char *path) {
    return sys_open(path,0x200000);
}
static void sys_closedir(int fd) {
    sys_close(fd);
}
// dirPtr - actual pointer to the directory structure.
// now used because kernel doesn't allows userspace programs to access
// kernel mode structures and data anymore.
static int sys_readdir(int fd,int p,void *dirPtr) {
    process_t *caller = thread_getThread(thread_getCurrent());
    if (fd < 0 || fd >= 200) return -EBADF;
    file_descriptor_t *_fd = caller->fds[fd];
    if (_fd == NULL) {
	    return -EBADF;
	}
    int r = vfs_readdir(_fd->node,p,(struct dirent *)dirPtr);
    return r;
}
static int sys_mount(char *dev,char *mount_point,char *fs) {
    // Check if caller have required privileges.
    process_t *prc = thread_getThread(thread_getCurrent());
    if (prc->uid != 0) {
	    return -EACCES;
	  }
    // Validate pointers.
    if (!isAccessable(dev) || !isAccessable(mount_point) || !isAccessable(fs)) {
      return -EFAULT;
    }
    char *dev_copy = strdup(dev);
    char *mountptr = strdup(mount_point);
    char *f = strdup(fs);
    vfs_node_t *de = vfs_find(dev_copy);
    if (!de) {
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -ENOENT;
    }
    vfs_fs_t *fol = vfs_findFS(f);
    if (!fol) {
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -ENOENT;
    }
    aspace_t *aspace = arch_mmu_getAspace();
    prc->aspace = arch_mmu_getKernelSpace();
    arch_mmu_switch(prc->aspace);
   // kprintf("DEBUG! %s: fol: %x, de: %x, mountptr: 0x%x\n",__func__,fol,de,mountptr);
   // kprintf("Calling vfs_mount\n");
    if (!vfs_mount(fol,de,mountptr)) {
	    kfree(dev_copy);
	    kfree(mountptr);
	    kfree(f);
	    arch_mmu_switch(aspace);
	    prc->aspace = aspace;
	    return -EINVAL;
    }
    kfree(dev_copy);
    kfree(mountptr);
    kfree(f);
    prc->aspace = aspace;
    arch_mmu_switch(aspace);
    // When the mount process is complete, then retrive latest element and fill the device_path.
    vfs_mount_t *mntList = vfs_getMntList();
    while(mntList->next != NULL) {
      mntList = mntList->next;
    }
    mntList->device_path = strdup(dev);
    return 0;
}
static int sys_waitpid(int pid,int *status,int options) {
    // Most important syscall!
    process_t *parent = thread_getThread(thread_getCurrent());
    if (pid > 0) {
    	// Check if the given process has been spawned by the current caller
    	struct process *child = thread_getThread(pid);
    	//DEBUG("sys_waitpid called from %s to wait for %s\r\n", parent->name, child->name);
    	if (child != NULL && child->parent == parent) {
        //	thread_waitPid(parent);
		// Wait for the child process to start and die
        	while (!child->started || !child->died || child->state != STATUS_ZOMBIE) {
            		// Perform a blocking wait until the child process status changes
            		arch_reschedule();
        	}
		if (status != NULL) {*status = child->exit_code;}
		queue_remove(parent->childQueue,child);
                kfree(child->name);
		kfree(child);
		return pid;
    	}
	} else if (pid < 0) {
		// wait for any child that has been spawned by the fucking process
		int nonblock = ((options & 1) == 1);
		int how = 1;
                queue_t *childQ = (queue_t*)parent->childQueue;
		while(how) {
			queue_for(p,childQ) {
                                // Check if queue element doesn't fucked up.
                                if ((uintptr_t)p < KERN_HEAP_BEGIN || (uintptr_t)p > KERN_HEAP_END) return -1;
				process_t *child = (process_t *)p->value;
				if ((uintptr_t)child < KERN_HEAP_BEGIN || (uintptr_t)child > KERN_HEAP_END) {
                                kprintf("Removing unknown task(%s)\n",parent->name);
                                childQ->size = 0;
                                childQ->head = NULL;
				return -1;
				}
				if (child->died || child->state == STATUS_ZOMBIE) {
					queue_remove(parent->childQueue,child);
					int r = child->pid;
					if (status != NULL) {*status = child->exit_code;}
                                        kfree(child->name);
					kfree(child);
					return r;
				}
			}
                if (nonblock) how--;
                //  arch_reschedule();
		}
		return 0;
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
    if (_fd < 0 || _fd >= 200) {
	    return -EBADF;
	}
    if (caller == NULL) return -1;
    file_descriptor_t *fd = caller->fds[_fd];
    if (fd == NULL) {
	    return -EBADF;
    }
    if (type == 0) {
        fd->offset = how;
    } else if (type == 2) {
        fd->offset = fd->node->size - how;
    } else if (type == 1) {
    	fd->offset += how;
    }
    return fd->offset;
}
static int sys_tell(int _fd) {
    process_t *caller = thread_getThread(thread_getCurrent());
    if (_fd < 0 || _fd >= 200) {
	    return -EBADF;
	}
    file_descriptor_t *fd = caller->fds[_fd];
    return fd->offset;
}
static void *sys_mmap(int _fd,int addr,int size,int offset,int flags) {
    process_t *caller = thread_getThread(thread_getCurrent());
    if (size == 0) return (void *)-EINVAL;
    // Check if flags doesn't explode our kernel
    if (((flags & 0x0001) == 0x001) && ((flags & 0x002) == 0x002)) {
      // Whatahell
      return (void *)-EINVAL;
    }
    uint32_t where = 0;
    if (_fd <= 0) {
      if ((flags & 0x0008) == 0x0008) {
        // Map fixed address space.
        if (addr == 0) {
          return (void *)-EINVAL;
        }
        where = (uint32_t)arch_mmu_query(NULL,(uintptr_t)addr,size);
        if (where != addr) {
          // cannot placed here.
          return (void *)-ENOMEM;
        }
      }
      if (addr != 0) {
        where = (uint32_t)arch_mmu_query(NULL,(uintptr_t)(uint32_t)addr,size);
      } else {
	      where =  (uint32_t)arch_mmu_query(NULL,(uintptr_t)(uint32_t)USER_MMAP_START,size);
      }
	    if (where == -1) {
		    return (void *)-ENOMEM;
	    }
	    int pages = (size/4096)+2;
      if (pages == 0) pages = 1;
	    for (int i = 0; i < pages; i++) {
		    int off = (i*4096);
        uintptr_t page = alloc_getPage();
        if (page == -1) {
          return (void *)-ENOMEM;
        }
			int map_flags = arch_mmu_processFlags(PROT_READ | PROT_WRITE | PROT_USER | PROT_OWNED);
		    arch_mmu_mapPage(NULL,where+off,page,map_flags); // MMU_OWNED used to allow munmap to free the pages.
	    }
	    return (void *)(uintptr_t)where;
    }
    if (_fd < 0 || _fd >= 200) {
	    return (void *)-EBADF;
    }
    file_descriptor_t *fd = caller->fds[_fd];
    vfs_node_t *node = fd->node;
    // Check permission.
    if (!vfs_hasPerm(node,PERM_WRITE,caller->gid,caller->uid)) {
	    return (void *)-EACCES;
    }
    if (node != NULL) {
                // Some changes here!
                if (node->fs->mmap == NULL) {
                        // All work done by ourself.
                        if (size > node->size) {
                                // Limit :(
                                size = node->size;
                        }
                        if (addr != 0) {
                                where = (uint32_t)arch_mmu_query(NULL,(uintptr_t)(uint32_t)addr,size);
                        } else {
                                where = (uint32_t)arch_mmu_query(NULL,(uintptr_t)(uint32_t)USER_MMAP_START,size);
                        }
                        // Now! Allocate some pages!
                        int map_flags = arch_mmu_processFlags(PROT_READ | PROT_WRITE | PROT_USER | PROT_OWNED);
                        for (int i = 0; i < (size/4096)+1; i++) {
                                uintptr_t pa = alloc_getPage();
                                arch_mmu_mapPage(NULL,where+(i*4096),pa,map_flags);
                        }
                        // Now, read the file content in our allocated memory.
                        vfs_read(node,offset,size,(void *)where);
                        return (void *)where;
                }
                return vfs_mmap(node,addr,size,offset,flags);
    }
    return (void *)-1;
}
static int sys_insmod(char *path) {
    int len = strlen(path);
    char *copy = kmalloc(len+1);
    strcpy(copy,path);
    vfs_node_t *module = vfs_find(copy);
    if (!module) {
        kfree(copy);
        return -ENOENT;
    }
    kfree(copy);
    // allocate space that need to be used
    void *m = kmalloc(module->size+elf_get_module_bytes(module));
    if (!m) {
        return -2;
    }
    vfs_read(module,0,module->size,m);
    // Load
    module_t *mod = load_module(m);
    if (!mod) {
	kfree(m);
        return -2;
    }
    kprintf("Module %s load address(used for GDB): 0x%x\n",mod->name,mod->load_address);
    // call the module entry point
    if (mod->init) {
	//kprintf("sys_insmod: calling entry point of module\n");
        mod->init(mod);
    }
    //kfree(m);
    //kprintf("syscall: exit from sys_insmod\n");
    return 0;
}
static void sys_rmmod(char *name) {
    kprintf("Comming soon!\r\n");
}
static int sys_ioctl(int fd,int req,va_list list) {
    // Generally used by the compositor to draw windows!
    process_t *caller = thread_getThread(thread_getCurrent());
    if (fd < 0 || fd >= 200) return -EBADF;
    file_descriptor_t *file_desc = caller->fds[fd];
    if (!isAccessable(file_desc) || !isAccessable(file_desc->node)) return -EBADF;
    if (file_desc == NULL || file_desc->node == NULL) return -ENOENT;
    // Sanity check
    if (!isAccessable(file_desc->node->fs) || !isAccessable(file_desc->node->fs->ioctl)) return -EBADF;
    if (file_desc->node->fs == NULL|| file_desc->node->fs->ioctl == NULL) return -EBADF;
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
    if (!prc) return (void *)-ENOMEM;
    if (increment == 0) {
	    increment = 1;
	}
    void *ptr = sbrk(prc,increment);
    if (ptr == (void*)-1) {
      return (void *)-1;
    }
    //kprintf("Ptr -> 0x%x\r\n",ptr);
    return (void *)ptr;
}
static int sys_dup(int oldfd,int newfd,int dupfd) {
	process_t *prc = thread_getThread(thread_getCurrent());
	if (!prc) return -EBADF;
	// Duplicate there FD
	file_descriptor_t *fd = prc->fds[oldfd];
	if (!fd && dupfd != 1) return -EBADF;
	if (newfd >= 200) {
		return -24;
	}
	if (dupfd == 2) {
		// newfd = oldfd, copy the newfd file descriptor
                file_descriptor_t *cp = kmalloc(sizeof(file_descriptor_t));
                memcpy(cp,fd,sizeof(file_descriptor_t));
                prc->fds[newfd] = cp;
		return newfd;
	}
        file_descriptor_t *copy = kmalloc(sizeof(file_descriptor_t));
	// We don't clear memory because we gonna copy it
	if (fd != NULL) {
		memcpy(copy,fd,sizeof(file_descriptor_t));
	}
	// find new usable fd.
	int fd_id = -1;
	for (int i = 0; i < prc->next_fd; i++) {
		if (prc->fds[i] == NULL) {
			// Found!
			fd_id = i;
			break;
		}
	}
	if (fd_id == -1) {
		// Not found. Okay.
		fd_id = prc->next_fd;
		prc->next_fd++;
	}
	prc->fds[fd_id] = copy;
	return fd_id;
}
/*
 * Modification of 26.07.2024:
 * We pass most of our argumnets to arch_new_thread,
 * so the arch specific code will do the 'magic'
 * and prepare the context of new thread to the
 * passed arguments, also changes the stack at call,
 * if passed by the caller.
 *
*/
static int sys_clone(void *stackPtr,uintptr_t stackSize) {
    arch_cli();
    process_t *caller = thread_getThread(thread_getCurrent());
    // Acording to the wiki, we need just to create an process with the same VM space
    process_t *thread = thread_create("thread",0,true);
    thread->state = STATUS_CREATING;
    // Change the address space
    // TODO: Add support for other flags
    thread->aspace = caller->aspace;
    thread->type = TYPE_THREAD;
    thread->brk_begin = caller->brk_begin;
    thread->brk_end = caller->brk_end;
    thread->brk_next_unallocated_page_begin = caller->brk_next_unallocated_page_begin;
    arch_clone_current(thread,NULL,0);
    caller->brk_begin = thread->brk_begin;
    caller->brk_end = thread->brk_end;
    caller->brk_next_unallocated_page_begin = thread->brk_next_unallocated_page_begin;
    // Clone the file descriptors
    for (int i = 0; i < caller->next_fd; i++) {
	    file_descriptor_t *desc = caller->fds[i];
	    if (desc != NULL) {
		    thread_openFor(thread,desc->node,desc->flags);
		} else {
      caller->fds[i] = NULL;
    }
	}
    thread->state = STATUS_RUNNING;
    return thread->pid;
}

static void sys_waitForStart(int pid) {
	process_t *waitFor = thread_getThread(pid);
	if (waitFor == NULL || waitFor->pid == 0) {
		// Returned when no such process ID(PID)
		return;
	}
	arch_sti(); // enable interrupts, so scheduler can work normally
	// Wait
	while(!waitFor->started);
}

static void sys_usleep(int ms) {
	//arch_cli();
	/*process_t *caller = thread_getThread(thread_getCurrent());
	caller->wait_time = ms;
	caller->state = STATUS_SLEEP;
	caller->quota = PROCESS_QUOTA;*/
	arch_sti();
	kwait(ms);
	//arch_reschedule();
}

static int sys_truncate(int fd,int size) {
	process_t *prc = thread_getThread(thread_getCurrent());
	if (fd < 0 || fd >= 200) {
		return -EBADF;
	}
	file_descriptor_t *desc = prc->fds[fd];
	if (!desc) return -ENOENT;
	vfs_truncate(desc->node,size);
	return 0;
}

static int sys_socket(int domain,int type,int protocol) {
	Socket *s = kmalloc(sizeof(Socket));
	memset(s,0,sizeof(Socket));
	if (!socket_create(domain,type,s)) {
		kfree(s);
		return -EBADF;
	}
	s->owner = thread_getCurrent();
	s->flags = type;
	// Create and open node
	vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
	memset(n,0,sizeof(vfs_node_t));
	n->flags = VFS_SOCKET; // socket FD instance, destroy at close
	n->priv_data = s;
	n->mask = (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
	return thread_openFor(thread_getThread(thread_getCurrent()),n,FD_RDWR);
}
static int sys_bind(int sockfd,struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (sockfd < 0 || sockfd >= 200) {
		return -EBADF;
	}
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
        Socket *s = desc->node->priv_data;
        return s->bind(s,sockfd,addr,len);
}
static int sys_listen(int sockfd,int backlog) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
        Socket *s = desc->node->priv_data;
        return s->listen(s,sockfd,backlog);
}
static int sys_accept(int sockfd,struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
        Socket *s = desc->node->priv_data;
        return s->accept(s,sockfd,addr,(socklen_t *)&len);
}
static int sys_connect(int sockfd,const struct sockaddr *addr,int len) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
        Socket *s = desc->node->priv_data;
        return s->connect(s,sockfd,addr,(socklen_t *)&len);
}
static int sys_send(int sockfd,const void *buff,int len,int flags) {
	process_t *caller = thread_getThread(thread_getCurrent());
        file_descriptor_t *desc = caller->fds[sockfd];
        if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
        Socket *s = desc->node->priv_data;
        return s->send(s,sockfd,buff,len,flags);
}
static int sys_recv(int sockfd,void *buff,int len,int flags) {
	process_t *caller = thread_getThread(thread_getCurrent());
	file_descriptor_t *desc = caller->fds[sockfd];
	if (desc == NULL || desc->node == NULL || (desc->node->flags & VFS_SOCKET) != VFS_SOCKET) return -EBADF;
	Socket *s = desc->node->priv_data;
	return s->recv(s,sockfd,buff,len,flags);
}

static int sys_ready(int fd) {
	process_t *t = thread_getThread(thread_getCurrent());
	file_descriptor_t *ft = t->fds[fd];
	if (ft != NULL) {
		if ((ft->node->flags & VFS_SOCKET) == VFS_SOCKET) {
			// Check if socket is ready for connections
			Socket *sock = (Socket *)ft->node->priv_data;
			// Check for incomming data, THEN check if the connection is tryied.
			if (sock->isReady) {
				if (sock->isReady(sock)) {
					return 3;
				}
				// No incomming data? Check if we have incomming clients.
				if (sock->acceptQueue != NULL) {
					// Check if have something.
					if (sock->acceptQueue->size != 0) {
						return 3;
					}
				}
			}
			return 2;
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
	arch_cli();
	process_t *parent = thread_getThread(thread_getCurrent());
	// Clone!
 	aspace_t *orig = arch_mmu_getAspace();
	arch_mmu_switch(arch_mmu_getKernelSpace());
	process_t *child =  thread_create(parent->name,(void *)arch_syscall_getCallerRet(),true);
	child->state = STATUS_CREATING;
	child->substate = SUBSTATE_FORK;
	memset(child->fds,0,200);
	// Clone the parent root and work directories.
	child->workDir = parent->workDir;
	child->root = parent->root;
	aspace_t *space = arch_mmu_newAspace();
	arch_mmu_duplicate(orig,space);
	// Arch specific code for fork
    	arch_forkProcess(parent,child);
	child->aspace = space;
	//arch_cloneStack(parent,child);
	// Clone the FD's
	for (int i = 0; i < 200; i++) {
		file_descriptor_t *desc = parent->fds[i];
		// We MUST do indentical FD list as parent.
		if (desc == NULL) {
			child->fds[i] = NULL;
			continue;
		}
		file_descriptor_t *f = kmalloc(sizeof(file_descriptor_t));
		memcpy(f,desc,sizeof(file_descriptor_t));
		// Change owner of FD.
		f->pid = child->pid;
		child->fds[i] = f;
	}
	child->next_fd = parent->next_fd;
	// sys_sbrk
	child->brk_begin = parent->brk_begin;
 	child->brk_end = parent->brk_end;
  	child->brk_next_unallocated_page_begin = parent->brk_next_unallocated_page_begin;
	child->parent = parent;
	parent->child = child;
	child->state = STATUS_RUNNING; // yeah
  	// Test.
  	arch_mmu_switch(space);
  	//fb_map();
	arch_mmu_switch(orig);
	//thread_forceSwitch();
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
    name->nodename = (system_hostname == NULL ? strdup_user(caller,"") : strdup_user(caller,system_hostname));
    return 0;
}
static int sys_sethostname(char *name,int len) {
	if (name == NULL || len == 0) return -1;
	if (len > 255) {
                len = 254;
        }
        memcpy(system_hostname,name,len);
        system_hostname[len] = 0;
	return 0;
}
static int sys_gethostname(char *name,int len) {
	if (system_hostname == 0) {
		return -EFAULT;
	}
	int copy_len = strlen(system_hostname);
        if (copy_len+1 < len) {
                copy_len = len;
        }
	memcpy(name,system_hostname,copy_len-1);
	name[copy_len] = 0;
	return 0;
}

static int sys_creat(char *pth,int type) {
    // Used to create directory, yes i don't know why i named it as sys_creat
    if (pth == NULL ) return EINVAL; // EINVL
    return vfs_createFile(pth,type);
}

static int sys_sync(int fd) {
    return -ENOSYS;
}

static int sys_munmap(void *ptr,int size) {
	// unmap specific area of memory.
	// TODO: Call unmap specific function if exist.
	uint32_t pages = (size / 4096)+1;
	arch_mmu_unmap(NULL,(vaddr_t)ptr,pages);
	return 0;
}
static int sys_umount(char *mountpoint) {
    // Check the pointer.
    if (!isAccessable(mountpoint)) {
      return -EFAULT;
    }
    // Find the mount point in table.
    struct vfsmnt *mnt = vfs_getMntList();
    while(mnt != NULL) {
      if (strcmp(mnt->target_path,mountpoint)) {
        mnt->prev->next = mnt->next;
        // Call FS specific function.
        if (mnt->fs->umount != NULL) {
          mnt->fs->umount(mnt->target);
        }
        // Restore original pointer?
        mnt->target->priv_data = mnt->priv_data;
        kfree(mnt->device_path);
        kfree(mnt->target_path);
        kfree(mnt);
        return 0;
        break;
      }
      mnt = mnt->next;
    }
    return -EINVAL;
}

static int sys_access(char *path,int mode) {
	// Check if the process can access the file pointed by fd
	char *usr_path = strdup(path);
	vfs_node_t *node = vfs_find(usr_path);
	if (node == NULL) {
		kfree(usr_path);
		return -ENOENT;
	}
	/*if ((node->flags != 0)) {
		kfree(usr_path);
		return -1;
	}*/
	kfree(usr_path);
	return 0;
}
static int sys_fchmod(int fd,int mode) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (fd < 0 || fd >= 200) {
		return -EBADF;
	}
	file_descriptor_t *desc = caller->fds[fd];
	if (!desc) {
		return -ENOENT; // errno ENON
	}
	vfs_node_t *node = desc->node;
	// We need some security thinks, but later.
	if (caller->uid != 0) {
		// User isn't root, check write access.
		if (!vfs_hasPerm(node,PERM_WRITE,caller->gid,caller->uid)) {
			// Caller doesn't have privileges to do this.
			return -EACCES;
		}
	}
	node->mask = mode;
	// After changing RAM node, try to change also in-disk data(if available)
	vfs_writeMetadata(node);
	return 0;
}
static int sys_stat(const char *path,struct stat *stat) {
	process_t *prc = thread_getThread(thread_getCurrent());
	/* Return some peace of information that currently available */
        char *pth = strdup(path);
        vfs_node_t *node = vfs_find(pth);
        kfree(pth);
        if (!node) {
                return ENOENT;
        }
	if (stat == NULL) return -EFAULT;
	memset(stat,0,sizeof(struct stat));
	// Populate non FS specific information based on VFS node.
        if (node->mask >= 0x100000) {
                return EFAULT;
        }
	stat->st_ino = node->inode;
	stat->st_mode = node->mask;
	stat->st_mode |= stat_convMode(node);
	stat->st_uid = node->uid;
	stat->st_gid = node->gid;
	stat->st_size = node->size;
	stat->st_mtime = node->mtime;
	stat->st_ctime = node->ctime;
	stat->st_atime = node->atime;
	/* Try to call FS specific stat */
        if (!isAccessable(node->fs) && !isAccessable(node->fs->stat)) {
		node->fs->stat(node,stat);
	}
	return 0;
}
static int sys_unlink(char *path) {
	char *path_copy = strdup(path);
	vfs_node_t *node = vfs_find(path_copy);
	if (!node) {
		kfree(path_copy);
		return ENOENT;
	}
	if (!node->fs || !node->fs->rm) {
		return EBADF;
	}
	if (!node->fs->rm(node)) {
		return EFAULT;
	}
	return 0;
}

static int sys_clock(int clock,uint64_t *out) {
	if (out == NULL) {
		return EINVAL;
	}
	switch(clock) {
		case 0:
		case 4:
			*out = clock_getUptimeSec();
			break;
		case 1:
		*out = clock_get();
		break;
		default:
		return -22;
	}
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
	if (type == 0 && buff == NULL) {
		return ringBuffPtr;
  	}
  	memcpy(buff,ringBuff,len);
	return 0;
}

int sys_chroot(char *path) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (caller == NULL) return -1;
	char *pth = strdup(path);
	if (!isAccessable(path)) {
		return EFAULT;
	}
	vfs_node_t *to = vfs_find(pth);
	kfree(pth);
	if (!to) {
		return ENOENT;
	}
	caller->root = to;
	caller->workDir = to;
	return 0;
}

int sys_fchdir(int _fd) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (_fd < 0 || _fd >= 200) {
		return EBADF;
	}
	file_descriptor_t *fd = caller->fds[_fd];
	if (fd == NULL) {
		return ENOENT;
	}
	caller->workDir = fd->node;
	return 0;
}
int sys_fchown(int _fd,int owner,int group) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (_fd < 0 || _fd >= 200) {
		return EBADF;
	}
	file_descriptor_t *fd = caller->fds[_fd];
	// Check if we have permissions to do so.
	if (caller->uid != 0) {
		// User isn't privileged.
		if (!vfs_hasPerm(fd->node,PERM_WRITE,caller->gid,caller->uid)) {
			return EACCES;
		}
	}
	fd->node->uid = owner;
	fd->node->gid = group;
	// Sync.
	vfs_writeMetadata(fd->node);
	return 0;
}
int sys_chown(int mode,char *path,int owner,int group) {
	process_t *caller = thread_getThread(thread_getCurrent());
  char *pth = strdup(path);
	vfs_node_t *node = vfs_find(path);
  kfree(pth);
	if (!node) {
		return ENOENT;
	}
	// Check access.
	if (caller->uid != 0) {
		// Not privileged.
		if (!vfs_hasPerm(node,PERM_WRITE,caller->gid,caller->uid)) {
			return EACCES;
		}
	}
	node->uid = owner;
	node->gid = group;
	vfs_writeMetadata(node);
	return 0;
}
int sys_rm(int mode,char *path) {
	// yes
	vfs_node_t *obj = vfs_find(path);
	if (obj == NULL) {
		return ENOENT;
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
	// Make copy of target and path.
	char *target_copy = strdup(target);
	char *path_copy = strdup(path);
	// Find target
	vfs_node_t *targetNode = vfs_find(target_copy);
	if (targetNode == NULL) {
		kfree(target_copy);
		kfree(path_copy);
		return -ENOENT;
	}
	if ((targetNode->mount_flags & VFS_MOUNT_RO) == VFS_MOUNT_RO) {
		// We need make sure that the file system can be writable.
		kfree(target_copy);
		kfree(path_copy);
		return -EROFS;
	}
	kfree(target_copy);
	// Now call FS specific function.
	// Oh no! Previous implementation will cause use-after-free....
	int ret =  vfs_symlink(targetNode,path_copy);
	kfree(path_copy);
	return ret;
}
struct prcInfo {
	char name[20];
	int pid;
	int status;
  uintptr_t usedMem;
};
static int sys_ipc(int cmd,int magicID,int ipcCmd,void *args) {
  switch(cmd) {
    case 1: {
      // IPC create.
      return ipc_create(magicID,args);
    } break;
    case 2: {
		    if (magicID == 'P') {
			    if (ipcCmd == 0) {
				    queue_t *list = (queue_t*)thread_getThreadList();
				    return list->size;
				} else if (ipcCmd == 1) {
					queue_t *list = (queue_t*)thread_getThreadList();
					struct prcInfo *lst = (struct prcInfo *)args;
					int i = 0;
					queue_for(node,list) {
						if ((uintptr_t)node < KERN_HEAP_BEGIN || ((uintptr_t)node > KERN_HEAP_END)) {
							// Remove this from table.
							kprintf("BUG");
						}
						process_t *prc = (process_t *)node->value;
						if (prc->stack == NULL) continue;
						struct prcInfo el = lst[i];
						strcpy(el.name,prc->name);
						el.pid = prc->pid;
						el.status = prc->state;
                                                el.usedMem = prc->brk_next_unallocated_page_begin - prc->brk_begin;
						lst[i] = el;
						i++;
					}
					return 0;
				} else if (ipcCmd == 2) {
					kprintf("DEBUG!\r\n");
					extern void ahci_init();
                                        //ahci_init();
					return 0;
				}

		    }
		    return ipc_cmd(magicID,ipcCmd,args);
    } break;
    case 3: {
      return ipc_destroy(magicID);
    } break;
  }
  return -1;
}
int sys_getfsstat(struct statfs *buf,long bufsize,int mode) {
	if (buf == NULL) {
		int how = 0;
		for (vfs_mount_t *mnt = vfs_getMntList(); mnt != NULL; mnt = mnt->next) {
			how++;
		}
		return how;
	}
	int how = bufsize / sizeof(struct statfs);
	vfs_mount_t *root = vfs_getMntList();
	for (int i = 0; i < how; i++) {
		if (root == NULL) break;
		if (root->device == NULL) {
			strcpy(buf[i].f_mntfromname,"none");
		} else {
      if (root->device_path != NULL) {
        strcpy(buf[i].f_mntfromname,root->device_path);
      } else {
        vfs_node_path(root->device,buf[i].f_mntfromname,90);
      }
	//		strcpy(buf[i].f_mntfromname,root->device->name);
		}
		strcpy(buf[i].f_mnttoname,root->target_path);
		strcpy(buf[i].f_fstypename,root->fs->fs_name);
		root = root->next;
	}
	return 0;
}
int sys_getrlimit(int id,rlimit_t *to) {
	if (id > 6) {
		return 14;
	}
	to->r_cur = limitList[id].r_cur;
	to->r_max = limitList[id].r_max;
	return 0;
}
int sys_setrlimit(int id,rlimit_t *to) {
	return 38;
}
int sys_getrusage(int resID,void *to) {
	// TODO: Implement THIS!
	return 0;
}
static int sys_openat(int dirfd,char *path,int mode) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (dirfd == -2) {
		// AT_FDCWD.
		return sys_open(path,mode);
	}
	if (dirfd < 0 || dirfd >= 200) {
		return -EBADF;
	}
	file_descriptor_t *fd = caller->fds[dirfd];
	if (!fd) {
		return -ENOENT;
	}
	if ((fd->node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
		return -ENOTDIR;
	}
	char *pth = strdup(path);
	char *token = strtok(pth,"/");
	vfs_node_t *node = fd->node;
	while(token != NULL) {
		node = node->fs->finddir(node,token);
		if (node == NULL) {
			kfree(pth);
			return -ENOENT;
		}
		token = strtok(NULL,"/");
	}
	kfree(pth);
	return thread_openFor(caller,node,FD_RDWR);
}
static unsigned long sys_sysconf(int id) {
	if (id == 1) {
		return alloc_getUsedPhysMem();
	} else if (id == 2) {
		return alloc_getKernelHeapPages();
	} else if (id == 3) {
		return alloc_getUsedSize();
	} else if (id == 4) {
		return alloc_getReservedSize();
	} else if (id == 5) {
                uintptr_t a = alloc_getPage();
                alloc_freePage(a);
                uintptr_t riz = a - arch_getMemStart();
                return riz;
        } else if (id == 6) {
                // total memory.
                return alloc_getAllMemory();
        }
	return 0;
}
int sys_fcntl(int fd,int cmd,long long arg) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (fd < 0 || fd >= 200) {
		return -EBADF;
	}
	file_descriptor_t *fds = caller->fds[fd];
	if ((uintptr_t)fds < KERN_HEAP_BEGIN || (uintptr_t)fds > KERN_HEAP_END) return -EBADF;
	if (fds == NULL || fds->node == NULL) return -9;
	vfs_node_t *node = fds->node;
	switch(cmd) {
		case 3:
			if ((node->flags & VFS_SOCKET) == VFS_SOCKET) {
				Socket *socket = (Socket *)node->priv_data;
				return socket->flags;
			} else {
				return node->flags;
			}
		case 4:
			if ((node->flags & VFS_SOCKET) == VFS_SOCKET) {
				Socket *socket = (Socket *)node->priv_data;
				socket->flags = arg;
			} else {
				node->flags = arg;
			}
			return 0;
	}
	return -22;
}
// Цей системний виклик дещо цікавий, адже
// нам потрібно змінити права доступу для
// кожної сторінки, розміром у 4КіБ, ба-
// зуючись на прарорцях у параметрі prot.
static int sys_mprotect(void *addr,size_t len,int prot) {
	int pages = (len / 4096);
	if (pages == 0) pages = 1;
	// TODO: Обробити прапорці у параметрі prot.
	int newFlags = arch_mmu_processFlags(prot);
	for (int i = 0; i < pages; i++) {
		// Нам потрібно отримати фізичну ядресу нашої сторінки.
		uintptr_t physAddr = (uintptr_t)arch_mmu_getPhysical(addr+(i*4096));
		if (physAddr == NULL || physAddr == -1) {
			// Не існує? Дивно.
			return -12;
		}
		// Зробимо свою справу.
		arch_mmu_mapPage(NULL,(vaddr_t)addr+(i*4096),physAddr,newFlags);
	}
	return 0; // якщо вийшло звісно.
}
static int sys_mkdirat(int dirfd,const char *name,int mode) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (dirfd < 0 || dirfd >= 200) return -EBADF;
	file_descriptor_t *fd = caller->fds[dirfd];
	if (fd == NULL) return -EBADF;
	if (fd->node == NULL) return -EBADF;
	if (fd->node->fs == NULL) return -EBADF;
	if (fd->node->fs->creat == NULL) return -EBADF;
	if (fd->node->fs->finddir(fd->node,name) != NULL) {
		return -17;
	}
	vfs_node_t *newDir = NULL;
	if ((newDir = fd->node->fs->creat(fd->node,name,VFS_DIRECTORY)) == NULL) {
		return -22;
	}
	newDir->uid = caller->uid;
	newDir->gid = caller->gid;
	newDir->mask = mode;
	return 0;
}
static int sys_mknod(const char *name,int mode,int devid) {
	return -ENOSYS;
}
static int sys_mknodat(int dirfd,const char *name,int mode,int devid) {
	return -ENOSYS;
}
static int sys_utime(int fd,struct timespec **time,int flags) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (fd < 0 || fd >= 200) return -EBADF;
	file_descriptor_t *_fd = caller->fds[fd];
	if (_fd == NULL) {
		return -EBADF;
	}
	vfs_node_t *node = _fd->node;
	node->mtime = time[1]->tv_sec;
	node->atime = time[0]->tv_sec;
	return 0;
}
static int sys_symlinkat(int dirfd,char *from,char *to) {
	process_t *caller = thread_getThread(thread_getCurrent());
	if (dirfd < 0 || dirfd >= 200) return -EBADF;
	file_descriptor_t *fd = caller->fds[dirfd];
	if (fd == NULL || fd->node == NULL ||
			(fd->node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
		return -EBADF;
	}
	// Well. Do the same as sys_symlink.
	char *from_copy = strdup(from);
	vfs_node_t *fromnode = vfs_find(from);
	if (!fromnode || fromnode->fs == NULL) {
		kfree(from_copy);
		return -ENOENT;
	}
	// Check source.
	if (fromnode->fs->symlink == NULL) {
		kfree(from_copy);
		return -ENOSYS;
	}
	char *to_copy = strdup(to);
	int ret = fromnode->fs->symlink(fromnode,to);
	kfree(to_copy);
	return ret;
}
static int sys_sigwait(unsigned long *sigset,int *sig) {
	process_t *caller = thread_getThread(thread_getCurrent());
	kprintf("sys_sigwait: called for %s\n",caller->name);
	return -ENOSYS;
}
static int sys_readlink(char *from,char *buff,int buffsz) {
  process_t *caller = thread_getThread(thread_getCurrent());
  char *from_copy = strdup(from);
  if (!from_copy) return -ENOMEM;
  vfs_node_t *target = vfs_find(from);
  kfree(from_copy);
  if (!target) {
    return -ENOENT;
  }
  if (!vfs_hasPerm(target,PERM_READ,caller->uid,caller->gid)) {
    return -EPERM;
  }
  if (target->fs == NULL || target->fs->readlink == NULL) {
    return -ENOSYS;
  }
  return target->fs->readlink(target,buff,buffsz);
}
static int sys_readlinkat(char *from,int dirfd,char *buff,int buffsz) {
	process_t *caller = thread_getThread(thread_getCurrent());
  vfs_node_t *searchIn = NULL;
  if (dirfd == -2) {
    searchIn = caller->workDir;
  } else {
    file_descriptor_t *fd = caller->fds[dirfd];
    if (fd == NULL) {
      return -EBADF;
    }
    searchIn = fd->node;
  }
  if ((searchIn->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
    return -ENOTDIR;
  }
  // we use strdup because the strtok currupts the given string. And the strtok is used in various part of the kernel itself.
  char *from_copy = strdup(from);
  vfs_node_t *node = vfs_finddir(searchIn,from_copy);
  kfree(from_copy);
  if (!node) {
    return -ENOENT;
  }
  if (node->fs == NULL || node->fs->readlink == NULL) {
    return -ENOSYS;
  }
  return node->fs->readlink(node,buff,buffsz);
}
static int sys_chmod(const char *path,int mode) {
  process_t *caller = thread_getThread(thread_getCurrent());
  if (!isAccessable(path)) return -EFAULT;
  char *pth = strdup(path);
  vfs_node_t *node = vfs_find(pth);
  kfree(pth);
  if (!node) return -ENOENT;
  if (!vfs_hasPerm(node,PERM_WRITE,caller->gid,caller->uid)) {
    return -EACCES;
  }
  node->mask = mode;
  vfs_writeMetadata(node);
  return 0;
}
static int sys_fchmodat(int dirfd,const char *path,int mode) {
  process_t *caller = thread_getThread(thread_getCurrent());
  if (!isAccessable(path)) return -EFAULT;
  if (dirfd < 0 || dirfd > 0) return -EBADF;
  file_descriptor_t *fd = caller->fds[dirfd];
  if (fd == NULL) return -EBADF;
  if ((fd->node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) return -EBADF;
  char *pth = strdup(path);
  vfs_node_t *node = vfs_finddir(fd->node,pth);
  kfree(pth);
  if (!node) {
    return -ENOENT;
  }
  if (!vfs_hasPerm(node,PERM_WRITE,caller->gid,caller->uid)) {
    return -EACCES;
  }
  node->mask = mode;
  vfs_writeMetadata(node);
  return 0;
}
int sys_unlinkat(int dirfd,const char *name) {
  process_t *caller = thread_getThread(thread_getCurrent());
  /*
  * if dirfd == -2 then that means current work directory.
  */
  vfs_node_t *node = NULL;
  vfs_node_t *dir = NULL;
  char *n = strdup(name);
  if (dirfd == -2) {
    dir = caller->workDir;
  } else {
    if (dirfd < 0 || dirfd >= 200) {
      return -EBADF;
    }
    file_descriptor_t *fd = caller->fds[dirfd];
    if (fd == NULL) {
      return -EBADF;
    }
    dir = fd->node;
  }
  if (n[0] == '/') {
    // Search in root!
    node = vfs_find(n);
  } else {
    // Check if the directory is even an directory!
    if ((dir->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
      return -ENOTDIR;
    }
    node = vfs_finddir(dir,n);
  }
  kfree(n);
  if (!node) {
    return -ENOENT;
  }
  if (node->fs->rm == NULL) {
    return -EROFS;
  }
  if (!node->fs->rm(node)) {
    return -EFAULT;
  }
  return 0;
}
static int sys_sigaction(int sig,const struct sigaction *act,struct sigaction *oldact) {
        /* Поки що мінімальна реалізація. */
        process_t *caller = thread_getThread(thread_getCurrent());
        if (sig >= 32) {
                return EINVAL;
        }
        if (act == NULL) {
                return EINVAL;
        }
        ProcessSignal hndl = caller->signal_handlers[sig];
        hndl.handler = (uintptr_t)act->sa_handler;
        hndl.flags = act->sa_flags;
        // Update!
        caller->signal_handlers[sig] = hndl;
        return 0;
}
static int sys_signalstack(struct sigStack *ss,struct sigStack *old_ss) {
        process_t *caller = thread_getThread(thread_getCurrent());
        if (ss == NULL && old_ss == NULL) {
                return EFAULT;
        }
        if (old_ss != NULL) {
                if (caller->sigStack != NULL) {
                        memcpy(old_ss,caller->sigStack,sizeof(struct sigStack));
                }
        }
        if (ss != NULL) {
                // Check if the stack size meet minimum size.
                if (ss->size < 4096) {
                        return ENOMEM;
                }
                // Check if the alternative stack is accesable.
                if (!isAccessable(ss->stack+ss->size)) {
                        return EFAULT;
                }
                memcpy(caller->sigStack,ss,sizeof(struct sigStack));
        }
        return 0;
}
