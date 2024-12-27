# RISC-V64 Low-level init code.

.section .text
.globl _start
.extern arch_entry_point

_start:
  la sp,stack_top
  mv a0,a1
  call arch_entry_point  # Call bldr_main as a function
1:
	j 1b

.section .data
stack_bottom:
  .skip 16384
stack_top:
# End of _start
