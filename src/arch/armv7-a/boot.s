.global _start
_start:
    ldr sp, =STACK_TOP
    bl kernel_main
1:
    b 1b
.size _start, . - _start
