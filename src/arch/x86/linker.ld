ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

SECTIONS
{
  . = 0x100000;
  kernel_start = .;
  .text :
  {
    *(.multiboot)
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

