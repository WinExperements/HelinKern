ENTRY(_start)

SECTIONS
{
  . = 0x8EF00000;
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
