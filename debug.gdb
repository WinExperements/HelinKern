# Here you can see all my problems in code
target remote localhost:1234
#b sys_read if _fd == 5
#b sys_ready
#b syscall_get if n == 44
#b unix_send
#b unix_recv
#b src/socket/unix.c:177
#b tty_isReady
#b arch_mmu_destroyAspace
#b isr14
#b sys_fork
#b thread_main
#b arch_cloneStack
#add-symbol-file /home/sergij/newkernel/userland/initrd/sh 0x40000000
#b execute
#b sys_waitpid
#b sys_exec
b *0x10543b
c
