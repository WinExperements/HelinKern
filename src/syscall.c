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
static void sys_default() {}
static void sys_exit(int exitCode);
static void sys_kill(int pid,int code);
static int sys_getpid();
static int sys_open(char *path,int flags);
static void sys_close(int fd);
static int sys_read(int fd,int offset,int size,void *buff);
static int sys_write(int fd,int offset,int size,void *buff);
static void *sys_alloc(int size);
static void sys_free(void *ptr);
static void sys_print(char *msg);
static int sys_exec(char *path,int argc,char **argv);
static void sys_reboot(int reason);
static void sys_chdir(char *to);
static void sys_pwd(char *buff,int len);
static int sys_opendir(char *path);
static void sys_closedir(int fd);
static void *sys_readdir(int fd,int p);
static int sys_mount(char *dev,char *mount_point,char *fs);
static void sys_waitpid(int pid);
static int sys_getppid();
static void sys_sysinfo();
static int sys_getuid();
static void sys_setuid(int uid);
static void sys_seek(int fd,int type,int how);
static int sys_tell(int fd);
static void *sys_mmap(int _fd,int addr,int size,int offset,int flags);
static int sys_insmod(char *path);
static void sys_rmmod(char *name);
static int sys_ioctl(int fd,int req,void *argp);
static void sys_setgid(int gid);
static int sys_getgid();
static void *sys_sbrk(int increment);
static int sys_dup(int oldfd,int newfd);
int syscall_table[36] = {
    (int)sys_default,
    (int)sys_print,
    (int)sys_exit,
    (int)sys_kill,
    (int)sys_getpid,
    (int)sys_default,
    (int)sys_default,
    (int)sys_open,
    (int)sys_close,
    (int)sys_read,
    (int)sys_write,
    (int)sys_alloc,
    (int)sys_free,
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
    (int)sys_sysinfo,
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
};
int syscall_num = 37;
static void sys_print(char *msg) {
    kprintf(msg);
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
static void sys_kill(int pid,int code) {
    thread_killThread(thread_getThread(pid),code);
    arch_reschedule();
}
static int sys_getpid() {
    return thread_getCurrent();
}
static int sys_open(char *path,int flags) {
    // get caller
    process_t *caller = thread_getThread(thread_getCurrent());
    int m = strlen(path);
    char *path_copy = kmalloc(m+1);
    memset(path_copy,0,m);
    strcpy(path_copy,path);
    vfs_node_t *node = vfs_find(path);
    if (!node && flags == 7) {
        node = vfs_creat(vfs_getRoot(),path,flags);
    }
    kfree(path_copy);
    
    return thread_openFor(caller,node);
}
static void sys_close(int fd) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *d = caller->fds[fd];
    vfs_close(d->node);
    kfree(d);
    caller->fds[fd] = NULL;
}
static int sys_read(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    int how = vfs_read(fd->node,fd->offset,size,buff);
    fd->offset+=how;
    return how;
}
static int sys_write(int _fd,int offset,int size,void *buff) {
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    int wr = vfs_write(fd->node,fd->offset,size,buff);
    fd->offset+=wr;
}
static void *sys_alloc(int size) {
    return kmalloc(size);
}
static void sys_free(void *ptr) {
    kfree(ptr);
}
static int sys_exec(char *path,int argc,char **argv) {
    int m = strlen(path);
    char *buff = kmalloc(m+1);
    strcpy(buff,path);
    vfs_node_t *file = vfs_find(buff);
    if (!file) {
        kfree(buff);
        return -1;
    }
    arch_cli();
    int len = file->size;
    void *file_buff = kmalloc(len);
    vfs_read(file,0,len,file_buff);
    elf_load_file(file_buff);
    process_t *prc = thread_getThread(thread_getNextPID()-1);
    // Change name to actual file name
    thread_changeName(prc,file->name);
    kfree(file_buff);
    kfree(buff);
    if (argc == 0 || argv == 0) {
        // Передамо звичайні параметри(ім'я файлу і т.д)
        char **params = kmalloc(1);
        params[0] = strdup(path);
        arch_putArgs(prc,1,params);
    } else {
        // We need to copy arguments from caller to prevent #PG when process is exitng!
        char **new_argv = kmalloc(argc);
        for (int i = 0; i < argc; i++) {
            new_argv[i] = strdup(argv[i]);
        }
        arch_putArgs(prc,argc,new_argv);
    }
    vfs_close(file);
    DEBUG("Used kheap after exec: %dKB\r\n",alloc_getUsedSize());
    arch_sti();
    return prc->pid;
}
static void sys_reboot(int reason) {
	if (reason == POWEROFF_MAGIC) {
		arch_poweroff();
	}
	arch_reset();
}
static void sys_chdir(char *to) {
    process_t *prc = thread_getThread(thread_getCurrent());
    int size = strlen(to);
    char *copy = kmalloc(size);
    strcpy(copy,to);
    vfs_node_t *node = vfs_find(copy);
    if (!node) {
        goto exit;
    }
    prc->workDir = node;
exit:
    kfree(copy);
}
static void sys_pwd(char *buff,int len) {
    vfs_node_path(thread_getThread(thread_getCurrent())->workDir,buff,len);
}
static int sys_opendir(char *path) {
    kprintf("%s: %s\r\n",__func__,path);
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
    if (!de) {
        kprintf("Failed to find %s\r\n",dev);
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -1;
    }
    vfs_fs_t *fol = vfs_findFS(f);
    if (!fol) {
        kprintf("Failed to find FS: %s\r\n",fs);
        kfree(dev_copy);
        kfree(mountptr);
        kfree(f);
        return -2;
    }
    aspace_t *aspace = arch_mmu_getAspace();
    arch_cli();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    vfs_mount(fol,de,mountptr);
    arch_sti();
    kfree(dev_copy);
    kfree(mountptr);
    kfree(f);
    arch_mmu_switch(aspace);
    return 0;
}
static void sys_waitpid(int pid) {
    // Most important syscall!
    if (pid > 0) {
    // Check if the given process has been spawned by the current caller
    struct process *child = thread_getThread(pid);
    process_t *parent = thread_getThread(thread_getCurrent());

    DEBUG("sys_waitpid called from %s to wait for %s\r\n", parent->name, child->name);

    if (child != NULL && child->parent == parent) {
        thread_waitPid(parent);

        // Wait for the child process to start and die
        while (!child->started || !child->died) {
            // Perform a blocking wait until the child process status changes
            arch_reschedule();
        }
    }
}
}
static int sys_getppid() {
    process_t *prc = thread_getThread(thread_getCurrent());
    if (prc->parent != NULL) {
        return prc->parent->pid;
    }
    return -1;
}
static void sys_sysinfo() {
	kprintf("HelinKern System Info report\r\n");
	kprintf("Used kernel heap: %dKB\r\n",alloc_getUsedSize()/1024);
	kprintf("Running tasks: %d\r\n",thread_getNextPID());
	arch_sysinfo();
    for (int i = 0; i < thread_getNextPID(); i++) {
        process_t *prc = thread_getThread(i);
        if (!prc) continue;
        kprintf("%s, %d\r\n",prc->name,prc->pid);
    }
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
static void sys_seek(int _fd,int type,int how) {
	process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *fd = caller->fds[_fd];
    if (type == 2) {
        fd->offset = how;
    } else if (type == 3) {
        fd->offset = fd->node->size;
    }
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
    //arch_cli();
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
        arch_mmu_switch(aspace);
        kfree(m);
        clock_setShedulerEnabled(true);
        return -2;
    }
    // call the module entry point
    if (mod->init) {
        mod->init(mod);
    }
    kfree(m);
    clock_setShedulerEnabled(true);
    arch_sti();
    arch_mmu_switch(aspace);
    return 0;
}
static void sys_rmmod(char *name) {
    kprintf("Comming soon!\r\n");
}
static int sys_ioctl(int fd,int req,void *argp) {
    kprintf("sys_ioctl\r\n");
    // Generally used by the compositor to draw windows!
    process_t *caller = thread_getThread(thread_getCurrent());
    file_descriptor_t *file_desc = caller->fds[fd];
    if (file_desc == NULL || file_desc->node == NULL) return -1;
    return vfs_ioctl(file_desc->node,req,argp);
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
    if (!prc) return -1;
    return sbrk(prc,increment);
}
static int sys_dup(int oldfd,int newfd) {
	process_t *prc = thread_getThread(thread_getCurrent());
	if (!prc) return -1;
	// Duplicate there FD
	file_descriptor_t *fd = prc->fds[oldfd];
	if (!fd) return -1;
	file_descriptor_t *copy = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
	// We don't clear memory because we gonna copy it
	memcpy(copy,fd,sizeof(file_descriptor_t));
	// Incrmenet the next FD pointer
	int fd_id = prc->next_fd;
	prc->next_fd++;
	if (newfd > 0) {
		fd_id = newfd;
	}
	prc->fds[fd_id] = copy;
	return fd_id;
}
