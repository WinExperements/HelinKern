# Here you can see all my problems in code
target remote localhost:1234
#b vga_scroll
#b arch_getFBInfo
#b arch_detect
b src/dev/fb.c:289
c
