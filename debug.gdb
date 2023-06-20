# Here you can see all my problems in code
target remote localhost:1234
#b alloc_getPage
#b alloc_mapItself
#b alloc_init
#b kshell_main
#b vfs_find
#b fb_map
#b sys_read
#b sys_exec
#b thread_killThread
#b src/syscall.c:172
#b arch_destroyArchStack
#b arch_putArgs
#b tty_read
#b keyboard_read
#b src/dev/keyboard.c:159
#b thread_create
#b dev_add
#b src/kernel.c:75
b syscall_get
c
