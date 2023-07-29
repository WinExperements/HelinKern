# Here you can see all my problems in code
target remote localhost:1234
#b alloc_init
b sys_exec
#b fb_clear
#b src/elf.c:109
#b alloc_reserve
c
