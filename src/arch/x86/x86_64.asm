; x86_64 Assembly Helper file.

[global gdt_flush]
gdt_flush:
	lgdt [rdi]
	mov ax, 0x10
   	mov ds, ax
    	mov es, ax
    	mov fs, ax
    	mov gs, ax
    	mov ss, ax
    	pop rdi
    	mov rax, 0x08
    	push rax
    	push rdi
    	retfq

extern x86_irq_handler		; Yes. We need to do some magic before calling independ part of IRQ processing.
extern stack_top
irq_common_stub:
	; Here we are...
	push rax
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	mov ax,ds
	push rax
	mov ax,0x10
	mov ds,ax
	mov es,ax
	mov fs,ax
	mov gs,ax
	cld
	mov rdi,rsp
	call x86_irq_handler
	mov rsp,rax
	jmp irq_common_exit
[global irq_common_exit]
irq_common_exit:
	pop rbx
	mov ds,bx
	mov es,bx
	mov fs,bx
	mov gs,bx
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax	
	add rsp,16	; two 64-bit values.
	iretq

; Taken from original X86 assembly code(see src/arch/x86/x86asm.asm:29)
extern isrNum
extern isrErrCode
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts firstly. 
    push qword 0
    push qword %1
    jmp irq_common_stub         ; Go to our common handler code.
%endmacro

; This macro creates a stub for an ISR which passes it's own
; error code.
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts. 
    push qword %1
    jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
; IRQs
extern irqNum
%macro IRQ 2
  [GLOBAL irq%1]  ; Use the First Parameter
  irq%1:
    push qword 0
    push qword %2
    jmp irq_common_stub
%endmacro

;; Define the Functions using the macro
IRQ	0	,	32
IRQ	1	,	33
IRQ	2	,	34
IRQ	3	,	35
IRQ	4	,	36
IRQ	5	,	37
IRQ	6	,	38
IRQ	7	,	39
IRQ	8	,	40
IRQ	9	,	41
IRQ	10	,	42
IRQ	11	,	43
IRQ	12	,	44
IRQ	13	,	45
IRQ	14	,	46
IRQ	15	,	47
; Some other stuff.
[GLOBAL syscall_irq]
extern syscall_handler
syscall_irq:
	push qword 0
 	push qword 128
 	jmp irq_common_stub
[GLOBAL scheduler_irq]
[extern interrupt_sendEOI]
extern process_schedule
scheduler_irq:
	push qword 0
	push qword 32
	jmp irq_common_stub
; Flush IDT
[global idt_flush]
idt_flush:
	lidt [rdi]
	ret

[global x86_64_enablePG]
x86_64_enablePG:
  ; Page table sits in rdi.
  ret
[extern runningTask]
[global x86_switchContext]
x86_switchContext:
	; Argument will be in rdi.
	mov rsi,[runningTask]
	; Check if the RSI isn't zero(usually happends only if previous process killed)
	cmp rsi,0
	je .swap
	mov rsi,[rsi+0]
	;push rbx
	;push rbp
	;push r12
	;push r13
	;push r14
	;push r15
	;push rdi
	;mov [rsi+0],rsp
	;mov [runningTask],rdi
	;mov rsp,[rdi+0]
	;pop rdi
	;pop r15
	;pop r14
	;pop r13
	;pop r12
	;pop rbp
	;pop rbx
	; Save current process registers.
	mov [rsi+0],rsp
	mov [rsi+64],rax
	mov [rsi+56],rdi
	mov [rsi+48],r15
	mov [rsi+40],r14
	mov [rsi+32],r13
	mov [rsi+24],r12
	mov [rsi+16],rbp
	mov [rsi+8],rbx
	; Switch running task? Nah, why? but, stop, actually yes!
.swap:
	mov [runningTask],rdi
	; Load new task stack structure
	mov rdi,[rdi+0]
	; Now restore this shit.
	mov rsp,[rdi+0]
	mov rbx,[rdi+8]
	mov rbp,[rdi+16]
	mov r12,[rdi+24]
	mov r13,[rdi+32]
	mov r14,[rdi+40]
	mov r15,[rdi+48]
	mov rax,[rdi+64]
	mov rdi,[rdi+56]
	; The return address must be saved onto the stack.
	ret

[extern tss_flush]
tss_flush:
	push rax
	mov rax,0x28
	ltr ax
	pop rax
	ret

[global usermode_test]
usermode_test:
	mov rdi,50
	.1b:
	jmp .1b

[global x86_switch]
x86_switch:
	mov rsp,rdi
	jmp irq_common_exit

[global copy_page_physical_asm]
copy_page_physical_asm:
	push rbp
	mov rsp,rbp
	push rbx
	; Source is in the rdi and destination in rsi.
	mov rdx,512	; 512*8 = 4096
	.loop:
	mov r11,[rdi]	; Acording to SysV r11 is temporary register.
	; Store
	mov [rsi],r11
	mov rax,[rsi]
	add rdi,8
	add rsi,8
	dec rdx
	jnz .loop
	; We done.
	leave
	ret

