section .text
; We are using NASM syntax
extern main
global _start
extern exit
extern __env
extern _stdio_init
extern _stdio_exit
_start:
    call main
    .exit:
    push eax
    call exit
