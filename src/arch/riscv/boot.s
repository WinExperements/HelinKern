# RISC-V32 Low-level init code.

.section .text
.globl _start
.extern arch_entry_point

_start:
  la sp,stack_top
  call arch_entry_point  # Call bldr_main as a function


.section .data
stack_bottom:
  .skip 16384
stack_top:
# End of _start
