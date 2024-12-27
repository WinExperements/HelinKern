ENTRY(_start)

SECTIONS
{
  . = 0x80200000;
  kernel_start = .;
  .text :
  {
    *(.text*)
    *(.rodata)
  }
   .text.interrupt : ALIGN(4) {}
  .data  :
  {
    *(.data)
    kernel_symbols_start = .;
    *(.symtab)
    kernel_symbols_end = .;
  }

  .bss  :
  {
    *(.bss)
  }
  
  kernel_end = .;
}

