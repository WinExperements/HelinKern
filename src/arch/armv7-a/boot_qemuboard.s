.global _start
_start:
    ldr sp, =STACK_TOP
    bl arch_entry_point
1:
    b 1b
.size _start, . - _start
