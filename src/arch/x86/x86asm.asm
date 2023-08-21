; Defines all X86 assembly code here

[GLOBAL gdt_flush]
gdt_flush:              ; Define the Functions
    mov eax , [esp+4]   ; Get the First Parameter which is the GDT pointer
    lgdt[eax]           ; Load the GDT using our pointer

    mov ax, 0x10        ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax          ; Load all data segment selectors
    mov es, ax
    mov fs, ax
    mov gs, ax
    jmp 0x08:gdt_flush_end
gdt_flush_end:
	ret
[GLOBAL tss_flush]
tss_flush:
    mov ax,0x2B
    ltr ax
    ret
; @ File : interrupt.s -- Contains interrupt service routine wrappers.
;
; This macro creates a stub for an ISR which does NOT pass it's own
; error code (adds a dummy errcode byte).
extern isrNum
extern isrErrCode
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts firstly. 
    push dword 0
    push dword %1
    jmp irq_common_stub         ; Go to our common handler code.
%endmacro

; This macro creates a stub for an ISR which passes it's own
; error code.
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli                         ; Disable interrupts. 
    push dword %1
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

; In isr.cc
extern x86_irq_handler

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isr_common_stub:
   push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    mov ax,ds
    push eax
    mov ax, 0x10  ; load the kernel data segment descriptor
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    push esp
    call x86_irq_handler
    add esp,4
    mov esp,eax
    pop ebx
    mov ds, ebx
    mov es, ebx
    mov fs, ebx
    mov gs, ebx
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    add esp, 8     ; Cleans up the pushed error code and pushed ISR number
    iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP


;; IRQ Definitions
;; Define the IRQ Handlers used in idt.h
;;
;; This Macro Creates the Functions
extern irqNum
%macro IRQ 2
  [GLOBAL irq%1]  ; Use the First Parameter
  irq%1:
    push dword 0
    push dword %2
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


; Called when an  interrupt is raised by the PIC
;[EXTERN irq_handler]

;; Define irq_common_stub , similar to isr_common_stub
irq_common_stub:
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    mov ax,ds
    push eax
    mov ax, 0x10  ; load the kernel data segment descriptor
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    push esp
    call x86_irq_handler
    add esp,4
    mov esp,eax
    jmp irq_handler_exit ; Exit from this code using jmp
[EXTERN irq_handler_exit]    
irq_handler_exit:
    pop ebx
    mov ds, ebx
    mov es, ebx
    mov fs, ebx
    mov gs, ebx
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    add esp, 8     ; Cleans up the pushed error code and pushed ISR number
    iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP
[GLOBAL idt_flush]

idt_flush:
  mov eax , [esp+4]     ; Get the pointer to the idt struct
  lidt [eax]            ; Loads the IDT
  ret   

[GLOBAL syscall_irq]
extern syscall_handler
syscall_irq:
	push 0
 	push 128
 	jmp irq_common_stub
[GLOBAL scheduler_irq]
extern process_schedule
scheduler_irq:
    push 0
    push 32
    ; Check if runningTask isn't zero
    jmp irq_common_stub
[global x86_switchContext]
[extern runningTask]
x86_switchContext:
    push ebx
    push esi
    push edi
    push ebp
    mov edi,[runningTask]
    mov [edi+0],esp
    mov esi,[esp+((4+1)*4)]
    mov [runningTask],esi
    mov esp,[esi+0]
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
[global x86_switch]
x86_switch:
    mov ebp,[esp+4]
    mov esp,ebp
    jmp irq_handler_exit
[global x86_jumpToUser]
x86_jumpToUser:
     mov eax,[esp+4]
    mov ebx,[esp+8]
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push 0x23
    push ebx
    push 0x200
    ; code segment selector, rpl = 3
    push 0x1b
    push eax
    mov eax,0
    mov ebx,0
    sti
    iretd

global arch_fpu_save
global arch_fpu_restore

arch_fpu_save:
    pushf                   ; Save flags register
    fnstcw [eax]            ; Save FPU control word to memory
    fnsave [eax+2]          ; Save FPU status word and FPU state to memory
    popf                    ; Restore flags register
    ret

arch_fpu_restore:
    pushf                   ; Save flags register
    fldcw [eax]             ; Restore FPU control word from memory
    frstor [eax+2]          ; Restore FPU status word and FPU state from memory
    popf                    ; Restore flags register
    ret



[global read_eip]
read_eip:
	pop eax
	jmp eax

[GLOBAL copy_page_physical]
copy_page_physical:
    push ebx              ; According to __cdecl, we must preserve the contents of EBX.
    pushf                 ; push EFLAGS, so we can pop it and reenable interrupts
                          ; later, if they were enabled anyway.
    cli                   ; Disable interrupts, so we aren't interrupted.
                          ; Load these in BEFORE we disable paging!
    mov ebx, [esp+12]     ; Source address
    mov ecx, [esp+16]     ; Destination address
  
    mov edx, cr0          ; Get the control register...
    and edx, 0x7fffffff   ; and...
    mov cr0, edx          ; Disable paging.
  
    mov edx, 1024         ; 1024*4bytes = 4096 bytes
  
.loop:
    mov eax, [ebx]        ; Get the word at the source address
    mov [ecx], eax        ; Store it at the dest address
    add ebx, 4            ; Source address += sizeof(word)
    add ecx, 4            ; Dest address += sizeof(word)
    dec edx               ; One less word to do
    jnz .loop             
  
    mov edx, cr0          ; Get the control register again
    or  edx, 0x80000000   ; and...
    mov cr0, edx          ; Enable paging.
  
    popf                  ; Pop EFLAGS back.
    pop ebx               ; Get the original value of EBX back.
    ret
