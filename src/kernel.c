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
static int fb_addr;
extern int *syscall_table;
void *memset(void *dest,char val,int count) {
  char *temp = (char *)dest;
  for( ; count != 0; count--) *temp++ = val;
  return dest;
}
void panic(char *file,int line,char *msg) {
    arch_cli();
    kprintf("PANIC: %s:%d %s\r\n",file,line,msg);
    while(1) {}
}
extern char kernel_end[];
void kernel_main(const char *args) {
    arch_pre_init(); // must setup the base thinks in the architecture, for example: just check if the CPU is supported
    arch_init(); // setup interrupts
    // init output
    output_uart_init();
    fbinfo_t fb;
    if (arch_getFBInfo(&fb)) {
        fb_init(&fb);
        fb_enable();
        fb_addr = (int)fb.addr;
    }
    // Hi!
    fb_disableCursor();
    DEBUG_N("HelinOS starting up...\r\n");
    int mem = arch_getMemSize();
    alloc_init(arch_getKernelEnd(),mem);
    arch_mmu_init();
    fb_map();
    fb_enableCursor();
    kheap_init();
    thread_init();
    syscall_init();
    vfs_init();
    rootfs_init();
    rootfs_mount("/");
    // Populate rootfs with something
    vfs_creat(vfs_getRoot(),"dev",VFS_DIRECTORY);
    vfs_creat(vfs_getRoot(),"bin",VFS_DIRECTORY);
    vfs_creat(vfs_getRoot(),"initrd",VFS_DIRECTORY);
    dev_init();
    cpio_init();
    #ifdef X86
    keyboard_init();
    fbdev_init();
 //   ps2mouse_init();
    #endif
    thread_create("shell",(int)kshell_main,false);
    arch_post_init();
    clock_setShedulerEnabled(true);
    arch_detect();
    if (fb_addr != 0) {
	output_changeToFB();
    }
    /*int (*exec)(char *) = ((int (*)(char *))syscall_get(13));
    exec("/bin/init");*/
    arch_sti();
    //kshell_main();
}
