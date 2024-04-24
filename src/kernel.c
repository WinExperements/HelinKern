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
// Sockets!
#include <socket/unix.h>
#ifdef X86
#include <pci/pci.h>
#endif
#ifdef HWCLOCK
#include <dev/clock.h>
#endif
static int fb_addr;
extern int *syscall_table;
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
    kprintf("Sorry, but the OS seems crashed by unknown reason. Send the information above to the system administrator for help.\r\n");
    kprintf("PANIC: %s:%d %s\r\n",file,line,msg);
    arch_trace();
    clock_setShedulerEnabled(false);
    kprintf("Rebooting in 5 seconds....");
    //arch_sti();
    //kwait(5000);
   // arch_reset();
    kprintf("Reboot failed, halt");
    while(1) {}
}
void kernel_main(const char *args) {
    // Bootstrap start
    arch_pre_init(); // must setup the base thinks in the architecture, for example: just check if the CPU is supported
    // init output
    output_uart_init();
    kprintf("HelinOS kernel, build date: %s using GCC %s\r\n",__DATE__,__VERSION__);
    fbinfo_t fb;
    if (arch_getFBInfo(&fb)) {
        fb_init(&fb);
        fb_enable();
        fb_addr = (int)fb.addr;
        fb_clear(0xffffff);
        // Pre-boot framebuffer
        output_changeToFB(); // important!
        //disableOutput= false;
        kprintf("Pre-Boot Platform Framebuffer. This buffer doesn't support scrolling outwise the MMU doesn't be initialized by arch-specific code\r\n");
        disableOutput = true; // change to false if you want to run it on an Lumia device with the provided in tree bootloader.
    }
    arch_init();
    // Hi!
    fb_disableCursor();
    int mem = arch_getMemSize();
    alloc_init(arch_getKernelEnd(),mem);
    arch_mmu_init();
    alloc_mapItself();
    fb_map();
    fb_enableCursor();
    // Bootstrap end
    kheap_init();
    ringBuff = (char *)kmalloc(RING_BUFF_SIZE);
    kprintf("%s: begin of parsing kernel arguments!\n",__func__);
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
    // Populate rootfs with something
    vfs_creat(vfs_getRoot(),"dev",VFS_DIRECTORY);
    vfs_creat(vfs_getRoot(),"bin",VFS_DIRECTORY);
    vfs_creat(vfs_getRoot(),"initrd",VFS_DIRECTORY);
    vfs_creat(vfs_getRoot(),"fat",VFS_DIRECTORY);
    dev_init();
    partTab_init();
    cpio_init();
    #ifdef X86
    keyboard_init();
    fbdev_init();
    ps2mouse_init();
    //kprintf("**** DELL AND REAL HARDWARE BUG ****\r\n");
    mbr_init();
    pci_init();
    ahci_init();
    ext2_main();
    #endif
    tty_init();
    socket_init();
#ifdef HWCLOCK
    kprintf("initializing hardware clock\r\n");
    hw_clock_init();
    struct tm curtime;
    hw_clock_get(&curtime);
    kprintf("Current time, reported by hardware clock: %d:%d %d/%d/%d\r\n",curtime.tm_hour,curtime.tm_min,curtime.tm_mday,curtime.tm_mon,curtime.tm_year);
#endif
    // register some sockets
    unix_register();
    arch_post_init();
    clock_setShedulerEnabled(true);
    arch_detect();
    kprintf("Monolitic kernel: Booting up some drives drivers....\n");
    // Directly try to mount initrd and run init if it fails then panic like Linux kernel or spawn kshell
    vfs_node_t *initrd = vfs_find("/initrdram");
    if (!initrd) {
        PANIC("Cannot find initrd. Pass it as module with initrd.cpio argument");
    }
    vfs_fs_t *cpio = vfs_findFS("cpio");
    if (!cpio) {
        PANIC("Cannot find CPIO FS in kernel!");
    }
    vfs_mount(cpio,initrd,"/initrd");
    vfs_node_t *mounted = vfs_find("/initrd");
    if (!mounted || !strcmp(mounted->fs->fs_name,"cpio")) {
        PANIC("Failed to mount initrd");
    }
#if 1
    kprintf("Installing some modules\r\n");
    /*int (*insmod)(char *) = ((int (*)(char *))syscall_get(30));
    insmod("/initrd/pci.mod");*/
    int (*exec)(char *,int,char **) = ((int (*)(char *,int,char **))syscall_get(13));
    int pid = exec("/initrd/init",0,NULL); // Ядро передасть параметри за замовчуванням.
    if (pid < 0) {
        PANIC("Failed to execute init");
   }
   //exec("/initrd/demo-hello",0,NULL);
#else
    //thread_create("kshell",(int)kshell_main,false);
	thread_create("kshell",(int)kshell_main,false);
#endif
    arch_sti();
}

