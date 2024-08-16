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
#add-symbol-file /home/justuser/HelinKern/userland/initrd/init 0x40000000
#b sys_exec
#b dash_execute
#b fb_enable
#b fb_putc
#b arch_processSignal
#add-symbol-file userland/initrd/mbr.mod 0x20289e6
#add-symbol-file userland/initrd/atapi.mod 0x202c615
#b iso9660_read
#b *0x10ab62
#b module/iso9660/main.c:240
b ext2_mount
#b module/atapi/atapi.c:509
b arch_post_init
