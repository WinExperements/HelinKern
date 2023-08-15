# Here you can see all my problems in code
target remote localhost:1234
#b alloc_init
#b sys_exec
#b fb_clear
#b src/elf.c:109
#b alloc_reserve
#b cpio_load
#b vfs_mount
#b arch_mmu_init
#b arch_putArgs
#b sys_exec
#b sys_mount
#b kmalloc if size == 10
#b kmalloc if size == 7
#b kmalloc if size == 6
#b kernel_main
#b sys_insmod
#b module_resolve_symbols
#b vfs_readBlock
#b vfs_find
#b arch_mmu_getPhysical
#b sys_exec
#b sys_open
#b thread_init
#b arch_prepareProcess
#b kernel_main
#b syscall_get if n == 10
#b tty_ioctl
#b keyboard_handler
#b socket_create
#b unix_send
#b unix_recv
#b unix_accept
#b th_m
b sys_socket
c
