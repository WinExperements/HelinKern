#include <vfs.h>
#include <kshell.h>
#include <arch.h>
#include <thread.h>
#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <elf.h>
#include <arch/mmu.h>
#include <syscall.h>
#include <symbols.h>
#include <dev/fb.h>
#include <debug.h>
#include <lib/queue.h>
#include <fs/cpio.h>
static vfs_node_t *keyboard;
static void parseCommand(int argc,char *cmd[]);
static bool exit = false;
static void start_module();
static void load_m(void *);
void kshell_main() {
	//output_changeToFB();
    arch_sti();
	kprintf("KShell V0.1\r\n");
    	kprintf("Please not that you are running in the kernel address space\r\n");
	// open keyboard device
	process_t *self = thread_getThread(thread_getCurrent());
	keyboard = vfs_find("/dev/keyboard");
	if (!keyboard) {
		kprintf("No keyboard found, exit\r\n");
		thread_killThread(self,1);
	}
    	kprintf("Activation some command(FAT32 driver test)...\r\n");
	char *buff = kmalloc(100);
    int argc = 0;
    char **argv = kmalloc(100);
    argv[0] = "mount";
    argv[1] = "cpio";
    argv[2] = "/bin/initrd.cpio";
    argv[3] = "/initrd";
    parseCommand(4,argv);
    argv[0] = "exec";
    argv[1] = "/initrd/init";
    parseCommand(2,argv);
    argv[0] = "exec";
    argv[1] = "/initrd/test";
    parseCommand(2,argv);
    /*argv[0] = "mount";
    argv[1] = "fat32";
    argv[2] = "/dev/hdap0";
    argv[3] = "/initrd";
    parseCommand(4,argv);*/
	while(exit != true) {
        argc = 0;
		kprintf("> ");
		vfs_read(keyboard,0,100,buff);
        argv[argc] = strtok(buff," ");
        while(argv[argc]) {
            argc++;
            argv[argc] = strtok(NULL," ");
        }
        if (argc > 0) {
            parseCommand(argc,argv);
        }
	}
	kfree(buff);
    kfree(argv);
    int (*exec)(char *) = ((int (*)(char *))syscall_get(13));
    int pid = exec("/initrd/init");
    void (*waitpid)(int) = ((void (*)(int))syscall_get(22));
    waitpid(pid);
    kprintf("Child %d exited, exit\r\n",pid);
	thread_killThread(self,0);
}
static void parseCommand(int argc,char *cmd[]) {
    if (strcmp(cmd[0],"ls")) {
        if (!vfs_getRoot()) {
            kprintf("ls: no mount point to /\n");
        } else {
            vfs_node_t *in = vfs_getRoot();
            if (argc > 1) {
                in = vfs_find(cmd[1]);
                if (!in){
                    kprintf("ls: %s: no such file or directory\n");
                }
            }
            struct dirent *d;
            int i = 0;
            while((d = vfs_readdir(in,i)) != 0) {
                if (d->name[0] != 0) {
                       kprintf("%s\n",d->name);
                } else break;
                i++;
            }
        }
    } else if (strcmp(cmd[0],"reboot")) {
        arch_reset();
    } else if (strcmp(cmd[0],"exit")) {
        exit = true;
    } else if (strcmp(cmd[0],"execm")) {
        start_module();
        // waitpid
        void (*waitpid)(int) = ((void (*)(int))syscall_get(22));
        waitpid(thread_getNextPID()-1);
    } else if (strcmp(cmd[0],"syms")) {
        symbols_print();
    } else if (strcmp(cmd[0],"loadm")) {
        if (argc > 1) {
                /*vfs_node_t *f = vfs_find(cmd[1]);
                if (!f) {
                    kprintf("failed to load module %s: no file found\r\n",cmd[1]);
                    return;
                }
                int len = f->size;
                void *addr = kmalloc(len+elf_get_module_bytes(f));
                vfs_read(f,0,len,addr);
                load_m(addr);
                kfree(addr);*/
                // check if our syscall is working
                int (*insmod)(char *) = ((int(*)(char *))syscall_get(30));
                insmod(cmd[1]);
      } else {
        load_m((void *)arch_getModuleAddress());
      }
    } else if (strcmp(cmd[0],"clear")) {
        fb_clear(BLACK);
    } else if (strcmp(cmd[0],"mount")) {
        if (argc > 2) {
            vfs_node_t *dev = vfs_find(cmd[2]);
            if (!dev) {
                kprintf("Device not found!\r\n");
                return;
            }
            vfs_fs_t *fs = vfs_findFS(cmd[1]);
            if (!fs) {
                kprintf("FS not found!\r\n");
                return;
            }
            vfs_mount(fs,dev,cmd[3]);
        }
    } else if (strcmp(cmd[0],"poweroff")) {
        arch_poweroff();
    } else if (strcmp(cmd[0],"kill")) {
        if (argc > 1) {
            int pid = atoi(cmd[1]);
            thread_killThread(thread_getThread(pid),0);
        }
    } else if (strcmp(cmd[0],"cat")) {
        if (argc > 1) {
            vfs_node_t *node = vfs_find(cmd[1]);
            if (!node) {
                kprintf("%s: not found\r\n",cmd[1]);
                return;
            } else if (node->flags == VFS_DIRECTORY) {
                kprintf("%s: is an directory\r\n",cmd[1]);
                return;
            }
            char *buff = kmalloc(node->size);
            vfs_read(node,0,node->size,buff);
            kprintf("%s\r\n",buff);
            kfree(buff);
            vfs_close(node);
        }
    } else if (strcmp(cmd[0],"exec")) {
        if (argc > 1) {
           int (*exec)(char *,int,char **) = ((int (*)(char *,int,char **))syscall_get(13));
            int pid = exec(cmd[1],0,NULL); // Ядро передасть параметри за замовчуванням.
            void (*waitpid)(int) = ((void (*)(int))syscall_get(22));
            waitpid(pid);
        }
    } else if (strcmp(cmd[0],"fin")) {
        kprintf("Finishing boot\r\n");
        kprintf("Starting up UI\r\n");
        fb_putc('h');
    } else if (strcmp(cmd[0],"hdt")) {
		vfs_node_t *file = vfs_find("/bin/init");
		if (!file) {
			kprintf("Failed to open ELF\r\n");
			return;
		}
		vfs_node_t *disk = vfs_find("/dev/hdap0");
		if (!disk) {
			kprintf("No disk found(hdap0)\r\n");
			return;
		}
        if (!strcmp(disk->name,"hdap0")) {
            kprintf("Invalid node\r\n");
            return;
        }
		char *data = kmalloc(file->size);
		kprintf("Select action: write/exec: ");
		char *b = kmalloc(6);
		vfs_read(keyboard,0,6,b);
		if (strcmp(b,"write")) {
            vfs_read(file,0,file->size,data);
			kprintf("Writing %d bytes to the first partition of disk %s\r\n",file->size,disk->name);
			vfs_writeBlock(disk,0,file->size,data);
		} else if (strcmp(b,"read")) {
            if (!strcmp(disk->name,"hdap0")) {
                kprintf("WTF?\r\n");
                kfree(data);
                kfree(b);
                return;
            }
			vfs_readBlock(disk,0,file->size,data);
			elf_load_file(data);
		}
        kfree(b);
		kfree(data);
	} else if (strcmp(cmd[0],"wr")) {
        vfs_node_t *n = vfs_find("/dev/hdap0");
        if (!n) return;
        arch_cli();
        char *data = "Hi!";
        uint16_t *d = kmalloc(512);
        strcpy((char *)d,data);
        vfs_writeBlock(n,0,512,d);
        kprintf("Writen 1 sector: %s, verifying\r\n",d);
        kfree(d);
        arch_sti();
    } else if (strcmp(cmd[0],"r")) {
        char *b = kmalloc(512);
        vfs_node_t *n = vfs_find("/dev/hdap0");
        vfs_readBlock(n,0,512,b);
        kprintf("%s\r\n",b);
        kfree(b);
    } else if (strcmp(cmd[0],"queue")) {
        queue_t *q = queue_new();
        kprintf("Queue initialized, pushing some info then poping\r\n");
        enqueue(q,"Test");
        enqueue(q,"AAA");
        for (int i = 0; i < q->size+1; i++) {
            kprintf("dequeue: %s\r\n",dequeue(q));
        }
        kfree(q);
    } else if (strcmp(cmd[0],"initrd")) {
        kprintf("Trying to load initrd...");
        vfs_node_t *initrd = vfs_find("/bin/initrd.cpio");
        if (!initrd) {
            kprintf("Not found\r\n");
            return;
        }
        void *buff = kmalloc(initrd->size);
        vfs_read(initrd,0,initrd->size,buff);
        vfs_close(initrd);
        cpio_load(buff,initrd->size);
        kprintf("done\r\n");
    } else {
        kprintf("Unknown commmand: %s\r\n",cmd[0]);
    }
}
static void start_module() {
    kprintf("Trying to start actual init\r\n");
    arch_cli();
    int addr = arch_getModuleAddress();
    if (addr != 0) {
        kprintf("Trying to load initrd as ELF\r\n");
        if (!elf_load_file((void *)addr)) {
            kprintf("Failed to load ELF! Check console\r\n");
        } else {
            // change name to "module"
            thread_changeName(thread_getThread(thread_getNextPID()-1),"module");
        }
    } else {
	    kprintf("No initrd passed, skipping...\r\n");
    }
    arch_sti();
}
static void load_m(void *addr) {
    module_t *mod = NULL;
    if (!(mod = load_module((void *)addr))) {
        kprintf("Failed\r\n");
    } else {
        mod->init(mod);
    }
}
