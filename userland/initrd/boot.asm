section .text
global _start
_start:
	mov eax,10
	mov edx,1
	mov ecx,0
	mov edi,msg
	mov ebx,msglen
	int 0x80
	mov eax,2
	mov edx,0
	int 0x80
section .rodata
msg: db "Hello World from NASM Assembly!",10
msglen: equ $ - msg
