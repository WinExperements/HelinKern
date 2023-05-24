# Here you can see all my problems in code
target remote localhost:1234
#b thread_killForeach
#b test
#b thread_killThread
#b alloc_getUsedSize
#b kfree
#b elf_load_file
#b thread_create
#b kshell_main
#b syscall_get
#b sys_exec
#b sys_exit
#b strcpy
#b memset
#b src/kshell.c:87
#b arch_pre_init
#b arch_post_init
#b ksbrk_page
#b module_load_seg
#b sys_insmod
#b vfs_mount
#b krealloc
b elf_load_file
add-symbol-file module/fat32/fat32.mod 0x200443b
b fat32_mount
c
