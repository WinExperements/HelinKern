# Here you can see all my problems in code
target remote localhost:1234
#b sys_read if _fd == 5
#b sys_ready
#b syscall_get if n == 44
#b unix_send
#b unix_recv
#b src/socket/unix.c:177
b tty_isReady
c
