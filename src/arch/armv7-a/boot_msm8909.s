.section .bss
.align 4
stack_bottom:
.skip 16384     @ 16 KiB
stack_top:

.section .text
.global _start
.type _start, %function
_start:
    ldr sp, =stack_top
    mov r0,r1
    bl arch_entry_point
1:
    b 1b

.size _start, . - _start
