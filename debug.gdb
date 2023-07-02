# Here you can see all my problems in code
target remote localhost:1234
#b kmalloc if chunk == 0x20026b7
#b sys_exec
#b sys_open
#b kmalloc
#b arch_putArgs
#b src/lib/string.c:66
b sys_opendir
c
