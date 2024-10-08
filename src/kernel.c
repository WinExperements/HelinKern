#include <kernel.h>
#include <arch.h>
#include <output.h>
#include <dev/fb.h>
#include <arch/mmu.h>
#include <mm/alloc.h>
#include <thread.h>
#include <syscall.h>
#ifdef X86
#include <dev/keyboard.h>
#include <dev/ps2mouse.h>
#endif
#include <elf.h>
#include <vfs.h>
#include <dev.h>
#include <kshell.h>
#include <debug.h>
#include <fs/cpio.h>
#include <fs/parttab.h>
#include <tty.h>
#include <lib/string.h>
#include <dev/input.h>
#include <dev/socket.h>
#include <mbr/mbr.h>
#include <atapi/atapi.h>
#include <ahci/ahci.h>
#include <ext2/ext2.h>
#include <iso9660/iso9660.h>
#include <fs/tmpfs.h>
// Sockets!
#include <socket/unix.h>
#ifdef X86
#include <pci/pci.h>
#include <arch/x86/idt.h>
#endif
#ifdef HWCLOCK
#include <dev/clock.h>
#endif
#include <ipc/ipc_manager.h>
static int fb_addr;
extern int *syscall_table,ringBuffPtr;
extern bool disableOutput;
extern char *ringBuff;
bool dontUseAta;
void *memset(void *dest,char val,int count) {
  char *temp = (char *)dest;
  for( ; count != 0; count--) *temp++ = val;
  return dest;
}
void panic(char *file,int line,char *msg) {
    arch_cli();
    disableOutput = false;
    arch_trace(0);
    kprintf("Sorry, but the OS seems crashed by unknown reason. Send the information above to the system administrator for help.\r\n");
    kprintf("PANIC: %s:%d %s\r\n",file,line,msg);
    clock_setShedulerEnabled(false);
    kprintf("Rebooting in 5 seconds....");
    //arch_sti();
    //kwait(5000);
   // arch_reset();
    kprintf("Reboot failed, halt");
    while(1) {}
}
static bool mount_root(char *devPath,char *fs) {
	vfs_node_t *devNode = NULL;
	if (devPath[0] == '/') {
		devNode = vfs_find(devPath);
		if (!devNode) {
			return false;
		}
	} else {
		dev_t *dev = dev_find(devPath);
		if (!dev) {
			return false;
		}
		devNode = dev->devNode;
	}
	vfs_fs_t *f = vfs_findFS(fs);
	if (!f) {
		return false;
	}
	return f->mount(devNode,vfs_getRoot(),NULL);
}
void kernel_main(const char *args) {
    // Bootstrap start
    arch_pre_init(); // must setup the base thinks in the architecture, for example: just check if the CPU is supported
    // init output
    output_uart_init();
    fbinfo_t fb;
    if (arch_getFBInfo(&fb)) {
        fb_init(&fb);
        fb_enable();
        fb_addr = (size_t)fb.vaddr;
        fb_clear(0xffffff);
        // Pre-boot framebuffer
        output_changeToFB(); // important!
        disableOutput= false;
        kprintf("Pre-Boot Platform Framebuffer. This buffer doesn't support scrolling outwise the MMU doesn't be initialized by arch-specific code\r\n");
        //disableOutput = true; // change to false if you want to run it on an Lumia device with the provided in tree bootloader.
    }
    arch_init();
    // Hi!
    //disableOutput = false;
    fb_disableCursor();
    uintptr_t mem = arch_getMemSize();
    alloc_init(arch_getKernelEnd(),mem);
    arch_mmu_init();
    alloc_mapItself();
    fb_map();
    fb_enableCursor();
    // Bootstrap end
    kheap_init();
    ringBuff = (char *)kmalloc(1<<CONF_RING_SIZE);
    kprintf("HelinOS kernel compiled by GCC %s on %s\r\n",__VERSION__,__DATE__);
    kprintf("%s: begin of parsing kernel arguments!\n",__func__);
    // Make sure the args is accessable.
    //arch_mmu_mapPage(NULL,(vaddr_t)args,(paddr_t)args,3);
    char *rootdev = NULL;
    char *begin = strtok(args," ");
    while(begin != NULL) {
    	if (strcmp(begin,"-v")) {
    		// verbose boot
    		disableOutput = false;
    		kprintf("Verbose boot!\r\n");
    	} else if (strcmp(begin,"crash")) {
    		PANIC("For debug purpose only");
    	} else if (strcmp(begin,"noatapi")) {
    		// Don't init ATA/ATAPI driver at boot
    		dontUseAta = true;
    	} else if (strcmp(begin,"rootdev")) {
		char *rdev = strtok(NULL," ");
		if (rdev == NULL) break;
		rootdev = strdup(rdev);
	} else {
    		kprintf("WARRNING: Unknown boot argument: %s\n",begin);
    	}
    	begin = strtok(NULL," ");
    }
    thread_init();
    syscall_init();
    vfs_init();
    rootfs_init();
    rootfs_mount("/");
    dev_init();
    ipc_init();
    ipc_init_standard();
    partTab_init();
    cpio_init();
#ifdef X86
    keyboard_init();
    fbdev_init();
    ps2mouse_init();
    pci_init();
    mbr_init();
    ahci_init();
    ext2_main();
    iso9660_init();
//    atapi_init();
    #endif
    tty_init();
    socket_init();
#ifdef HWCLOCK
    kprintf("initializing hardware clock\r\n");
    hw_clock_init();
    struct tm curtime;
    hw_clock_get(&curtime);
    kprintf("Current time, reported by hardware clock: %d:%d %d/%d/%d\r\n",curtime.tm_hour,curtime.tm_min,curtime.tm_mday,curtime.tm_mon,(curtime.tm_year > 100 ? curtime.tm_year + 1900 : curtime.tm_year + 2000));
#endif
    // register some sockets
    unix_register();
    tmpfs_init();
    kprintf("Internal structures initialized. Used physical pages: %d of %d. Used by kernel heap: %d. Physical bitmap took %d pages\r\n",alloc_getUsedPhysMem()/4096,alloc_getAllMemory()/4096,alloc_getKernelHeapPages(),alloc_getBitmapSize()/4096);
    arch_post_init();
    clock_setShedulerEnabled(true);
    arch_detect();
    // Directly try to mount initrd and run init if it fails then panic like Linux kernel or spawn kshell
    // Disable this chunk of code if you want to test your arch specific context switch code.
#if 1
    if (rootdev != NULL) {
	    if (mount_root(rootdev,"iso9660")) {
		goto executeInit;
	    } else {
		    kprintf("Failed to mount device %s, fallback to initramfs\r\n",rootdev);
	    }
	}
    if (!mount_root("/initrdram","cpio")) {
	    PANIC("Failed to mount initramfs. Check your system configuration/boot device integrity. You can also specify root device path using \"rootdev <device path>\" as part of kernel command line");
	}
#endif
#if 1
executeInit:
    kprintf("Detected Memory: %d MB. Used memory: %d(KB)\r\n",alloc_getAllMemory() / 1024 / 1024,alloc_getUsedPhysMem() / 1024);
    /*int (*insmod)(char *) = ((int (*)(char *))syscall_get(30));
    insmod("/initrd/pci.mod");*/
    int (*exec)(char *,int,char **,char **) = ((int (*)(char *,int,char **,char **))syscall_get(13));
    char *environ[] = {NULL};
    char *initPaths[] = {"/init","/sbin/init","/usr/bin/init","/usr/sbin/init",NULL};
    int pid = 0;
    for (int i = 0; initPaths[i] != NULL; i++) {
	    kprintf("Trying to start %s as init....\r\n",initPaths[i]);
	    pid = exec(initPaths[i],0,NULL,environ);
	}
    if (pid < 0) {
	    PANIC("Cannot find working init program");
    }
#else
	thread_create("kshell",kshell_main,false);
	//kshell_main();
	//testTask();
#endif
end:
    arch_sti();
    while(1) {}
}

