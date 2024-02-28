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
#include <dev/socket.h>
#ifdef X86
#include <arch/x86/acpi.h>
#endif
#include <dev.h>
#include <dev/fb.h>
static vfs_node_t *keyboard;
static void parseCommand(int argc,char *cmd[]);
static bool exit = false;
static void start_module();
static void load_m(void *);
static void th_m();
void kshell_main() {
    kprintf("KShell V0.1\r\n");
    kprintf("Please not that you are running in the kernel address space\r\n");
	//output_changeToFB();
    arch_sti();
	// open keyboard device
	process_t *self = thread_getThread(thread_getCurrent());
    // Update(30.01.2024): We need to mount the devfs
    vfs_fs_t *devfs = vfs_findFS("devfs");
    if (!devfs) {
        kprintf("No devfs found! Devfs mount failed!\n");
        return;
    }
    vfs_node_t *devM = vfs_find("/dev");
    if (!devM) {
        kprintf("No /dev found!\n");
        return;
    }
    if (!vfs_mount(devfs,devM,"/dev")) {
        kprintf("pre-init: failed to mount /dev\n");
        return;
    }
	keyboard = vfs_find("/dev/keyboard");
	if (!keyboard) {
		kprintf("No keyboard found, exit\r\n");
		thread_killThread(self,1);
	}
    	kprintf("Activation some command(FAT32 driver test)...\r\n");
	char *buff = kmalloc(100);
    int argc = 0;
    char **argv = kmalloc(100);
    /*argv[0] = "loadm";
    argv[1] = "/initrd/ahci.mod";*/
	while(exit != true) {
        argc = 0;
		kprintf("> ");
		int len = vfs_read(keyboard,0,100,buff);
		buff[len-1] = 0;
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
                char *a = strdup(cmd[1]);
                vfs_node_t *f = vfs_find(a);
                if (!f) {
                    kprintf("failed to load module %s: no file found\r\n",cmd[1]);
                    return;
                }
                int len = f->size;
                void *addr = kmalloc(len+elf_get_module_bytes(f));
                vfs_read(f,0,len,addr);
                module_t *mod = load_module(addr);
                if (mod != NULL) {
                    kprintf("kshell(internal kernel API), calling\n");
                    mod->init(mod);
                } 
                kfree(addr);
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
        } else {
            kprintf("kshell: use mount <fs> <device> <mount ptr>\n");
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
		void *a = kmalloc(512);
        	memset(a,0,512);
        	dev_t *dev = dev_find("hdap3");
        	if (dev == NULL) {
            		kprintf("Nope, hard drive not found, /dev/hdap3 doesn't exist\n");
            		return;
        	}
        	vfs_node_t *nod = dev->devNode;
        	kprintf("Reading up to 254 sectors as one command\n");
            void *buff = kmalloc(512*254);
            vfs_readBlock(nod,0,512*254,buff);
            kprintf("Readed, check the HDD active led\n");
            kfree(buff);
        	//kprintf("%d sectors of hdd readed\n",secCount);
	} else if (strcmp(cmd[0],"wr")) {
        	dev_t *dev = dev_find("hdap3");
        	if (!dev) return;
        	kprintf("Writing screenshot!\n");
        	vfs_node_t *nod = dev->devNode;
        	fbinfo_t *inf = kmalloc(sizeof(fbinfo_t));
        	arch_getFBInfo(inf);
        	int size = (inf->pitch * inf->width) - 4096;
        	uint8_t *buf = (uint8_t *)0x01400000;
        	for (int i = 0; i < size / 512; i++) {
        		vfs_writeBlock(nod,i,512,buf);
        		buf+=512;
        	}
        	kfree(inf);
        	kprintf("Writen, w -> 0x%x, h -> 0x%x, pitch -> 0x%x\n",inf->width,inf->height,inf->pitch);
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
    } else if (strcmp(cmd[0],"panic")) {
        PANIC("PP");
    } else if (strcmp(cmd[0],"serv")) {
	    // Create socket
	    kprintf("Creating kernel UNIX socket server(TEST)\n");
	    Socket *serv = (Socket *)kmalloc(sizeof(Socket));
	    memset(serv,0,sizeof(Socket));
	    if (!socket_create(PF_UNIX,serv)) {
		    kprintf("Failed to create socket\n");
		    kfree(serv);
            return;
		}
	    struct sockaddr *a = (struct sockaddr *)kmalloc(sizeof(struct sockaddr));
	    memset(a,0,sizeof(struct sockaddr));
	    strcpy(a->sa_data,"sock");
	    kprintf("Binding\n");
	    if (serv->bind(serv,0,a,0) < 0) {
		    kprintf("Failed to bind socket\n");
		    serv->destroy(serv);
		    kfree(serv);
            return;
		}
	    kprintf("Socket created, creating client\n");
	    serv->listen(serv,0,0);
	    thread_create("ss",th_m,false);
	    int r = serv->accept(serv,0,a,NULL);
	    kprintf("A: %d\n",r);
	    process_t *c = thread_getThread(1);
	    file_descriptor_t *t = c->fds[r];
	    vfs_node_t *node = t->node;
        kprintf("Node: 0x%x\n",node);
	    char *b = kmalloc(2);
	    serv->recv((Socket *)node->priv_data,0,b,2,0);
	    kprintf("Received: %s\n",b);
        serv->send((Socket *)node->priv_data,0,b,2,0);
	    kfree(b);
	    serv->destroy(serv);
	    kfree(serv);
	    kprintf("Currently, test finished\n");
    #ifdef X86
    } else if (strcmp(cmd[0],"findacp")) {
        kprintf("Okay, finding ACPI\n");
        void *ptr = acpiGetRSDPtr();
        if (ptr != NULL) {
            kprintf("AAAjr\n");
        } else {
            kprintf("ACPI didn't found on this system\n");
        }
	kprintf("Acer Aspire M1935 test\n");
	unsigned int startAddressOfACPI = 0xda5db000;
	unsigned int realACPIAddress = 0xda5be000;
	kprintf("Counting steps to reach 0x%x\n",realACPIAddress);
	int step = 0;
	for (unsigned int a = startAddressOfACPI; a < 0xda5dffff; a+=4) {
		if (a >= realACPIAddress) {
			kprintf("Step is %d, finded address 0x%x\n",step,a);
			break;
		}
		step++;
	}
	kprintf("All steps ended\n");
	kprintf("0x%x, 0x%x\n",realACPIAddress,startAddressOfACPI+118784);
    #endif
    } else if (strcmp(cmd[0],"atapi")) {
            arch_cli();
            #ifdef X86
            atapi_init();
            #endif
            kprintf("did i still here?\n");
            arch_sti();
    } else if (strcmp(cmd[0],"hdd")) {
    	dev_t *hdd = dev_find("hda");
    	if (!hdd) {
    		kprintf("Failed to find HD drive A in devfs!\r\n");
    		return;
    	}
    	vfs_node_t *node = hdd->devNode;
	int size = 200 * 1024 * 1024;
    	void *buff = kmalloc(size);
	if (!buff) {
		kprintf("Failed to allocate buffer!\r\n");
	}
	vfs_readBlock(node,0,size / 512,buff);
    	kprintf("Readed!!!\r\n");
    	kfree(buff);
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
        if (!elf_load_file((void *)addr,NULL)) {
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
static void th_m() {
	kprintf("Parallel thread!\n");
	Socket *client = kmalloc(sizeof(Socket));
	memset(client,0,sizeof(Socket));
	socket_create(PF_UNIX,client);
	// Connect
	struct sockaddr *addr = kmalloc(sizeof(struct sockaddr));
	memset(addr,0,sizeof(struct sockaddr));
	strcpy(addr->sa_data,"/client");
	// try to connect
	if (client->bind(client,0,addr,0) < 0) {
		kprintf("failed to bind client socket!\n");
		client->destroy(client);
		kfree(client);
		thread_killThread(thread_getThread(thread_getCurrent()),0);
	}
	strcpy(addr->sa_data,"/sock");
	if (client->connect(client,0,addr,0) < 0) {
		kprintf("Failed to connect to server!\n");
		client->destroy(client);
		kfree(client);
		thread_killThread(thread_getThread(thread_getCurrent()),0);
	}
	char *msg = "hi";
	char result[3];
	client->send(client,0,msg,2,0);
	client->recv(client,0,result,2,0); // very broken
    kprintf("\n\n\n\nReceived from server: %s\n",result);
	client->destroy(client);
	kfree(client);
	kprintf("Done\n");
	thread_killThread(thread_getThread(thread_getCurrent()),0);
}
