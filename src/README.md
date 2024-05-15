# Some documentation
## Process managmnent
Generally we use the brendans multitasking, but if the process is user mode, we just jumps to the user mode with the entry point
## Porting
To port the kernel to a new architecture you need to implement all functions that defined in `arch.h`,`arch/mmu.h`,`arch/elf.h`. Also you need to implement timer interrupt redirection in your IRQ handler. Also you need to define some variables to make allocator working, see [this](arch/x86/arch.c) file for more information.
## `Headless` platform
`Headless` platform is created to test if the kernel is bootable on new platforms. The platforms firstly used when i was started the riscv port.

