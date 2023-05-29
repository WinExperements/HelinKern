#include <syscall.h>
#include <output.h>
#include <arch.h>
#include <thread.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <elf.h>
#include <arch/mmu.h>
#include <debug.h>
static void sys_default() {}
static void sys_exit(int exitCode);
static void sys_kill(int pid,int code);
static int sys_getpid();
static vfs_node_t *sys_open(char *path,int flags);
static void sys_close(vfs_node_t *node);
static void sys_read(vfs_node_t *node,int offset,int size,void *buff);
static void sys_write(vfs_node_t *node,int offset,int size,void *buff);
static void *sys_alloc(int size);
static void sys_free(void *ptr);
static void sys_print(char *msg);
static int sys_exec(char *path);
static void sys_reboot(int reason);
static void sys_chdir(char *to);
static void sys_pwd(char *buff,int len);
static vfs_node_t *sys_opendir(char *path);
static void sys_closedir(vfs_node_t *node);
static void *sys_readdir(vfs_node_t *node,int p);
static int sys_mount(char *dev,char *mount_point,char *fs);
static void sys_waitpid(int pid);
static int sys_getppid();
static void sys_sysinfo();
static int sys_getuid();
static void sys_setuid(int uid);
static void sys_seek(vfs_node_t *node,int type,int how);
static int sys_tell(vfs_node_t *node);
static void *sys_mmap(vfs_node_t *node,int addr,int size,int offset,int flags);
static int sys_insmod(char *path);
static void sys_rmmod(char *name);
int syscall_table[32] = {
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
};
int syscall_num = 32;
static void sys_print(char *msg) {
    kprintf(msg);
}
void syscall_init() {
    // redirect to arch specific init code
    arch_syscall_init();
}
int syscall_get(int n) {
	if (n > syscall_num) return 0;
	return syscall_table[n];
}
static void sys_exit(int exitCode) {
    thread_killThread(thread_getThread(thread_getCurrent()),exitCode);
}
static void sys_kill(int pid,int code) {
    thread_killThread(thread_getThread(pid),code);
}
static int sys_getpid() {
    return thread_getCurrent();
}
static vfs_node_t *sys_open(char *path,int flags) {
    int m = strlen(path);
    char *path_copy = kmalloc(m+1);
    memset(path_copy,0,m);
    strcpy(path_copy,path);
    vfs_node_t *node = vfs_find(path);
    if (!node && flags == 7) {
        node = vfs_creat(vfs_getRoot(),path,flags);
    }
    kfree(path_copy);
    return node;
}
static void sys_close(vfs_node_t *node) {}
static void sys_read(vfs_node_t *node,int offset,int size,void *buff) {
    vfs_read(node,offset,size,buff);
}
static void sys_write(vfs_node_t *node,int offset,int size,void *buff) {
    vfs_write(node,offset,size,buff);
}
static void *sys_alloc(int size) {
    return kmalloc(size);
}
static void sys_free(void *ptr) {
    kfree(ptr);
}
static int sys_exec(char *path) {
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
    kfree(file_buff);
    kfree(buff);
    vfs_close(file);
    DEBUG("Used kheap after exec: %dKB\r\n",alloc_getUsedSize()/1024);
    arch_sti();
    return thread_getNextPID()-1;
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
static vfs_node_t *sys_opendir(char *path) {return vfs_find(path);}
static void sys_closedir(vfs_node_t *node) {}
static void *sys_readdir(vfs_node_t *node,int p) {return vfs_readdir(node,p);}
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
    // most important syscall!
    if (pid > 0) {
        //check if given process has spawned by current caller
        struct process *child = thread_getThread(pid);
        process_t *parent = thread_getThread(thread_getCurrent());
        if (child != NULL && child->parent == parent) {
            thread_waitPid(parent);
            while(parent->state == STATUS_WAITPID) {}
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
static void sys_sysinfo() {}
static int sys_getuid() {return 0;}
static void sys_setuid(int uid) {}
static void sys_seek(vfs_node_t *node,int type,int how) {}
static int sys_tell(vfs_node_t *node) {return 0;}
static void *sys_mmap(vfs_node_t *node,int addr,int size,int offset,int flags) {
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

