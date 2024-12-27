ENTRY(_start)

SECTIONS
{
  /*. = 0x8EF00000;*/
  . = 0x40400000;
  kernel_start = .;
  .text :
  {
    *(.text*)
    *(.rodata)
    *(.data)
    *(.bss)
  }

  kernel_end = .;
}
