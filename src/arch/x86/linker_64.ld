ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
  . = 0xffffffff80000000 + SIZEOF_HEADERS;
  kernel_start = .;
  .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.rodata .rodata.*)
    }
  .text ALIGN(CONSTANT(MAXPAGESIZE)):
  {
    *(.multiboot)
    *(.text*)
    *(.rodata)
  }

  .data ALIGN(CONSTANT(MAXPAGESIZE)):
  {
    *(.data)
    kernel_symbols_start = .;
    *(.symtab)
    kernel_symbols_end = .;
    KEEP(*(.requests_start_marker))
    KEEP(*(.requests))
    KEEP(*(.requests_end_marker))
  }

  .dynamic : {
	  *(.dynamic)
	}

  .bss ALIGN(CONSTANT(MAXPAGESIZE)):
  {
    *(.bss)
  }
  
  kernel_end = .;
}

