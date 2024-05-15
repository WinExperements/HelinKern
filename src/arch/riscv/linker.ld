ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  kernel_start = .;
  .text :
  {
    *(.text*)
    *(.rodata)
  }

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

