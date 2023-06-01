ENTRY(_start)

SECTIONS {
	. = 0x80010000;
	kernel_start = .;
	
	.text : { *(.text)}
	.rodata : { *(.rodata)}
	.data : { *(.data)}
	.bss : { *(.bss COMMON)}
	
	STACK_BASE = .;
	. += 0x10000;
	STACK_TOP = .;
	kernel_end = .;
}

