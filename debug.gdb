# Here you can see all my problems in code
target remote localhost:1234
#b kmalloc if chunk == 0x20026b7
#b sys_exec
#b sys_open
#b kmalloc
#b arch_putArgs
#b src/lib/string.c:66
#b sys_opendir
#b sys_ioctl
#b sys_sysinfo
#b thread_killThread
#b sys_exit
#b sys_write
#b sys_clone
#b sys_waitForStart
#b kernel_main
#b sys_usleep
b tty_ioctl
c
