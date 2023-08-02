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
b vfs_readBlock
c
