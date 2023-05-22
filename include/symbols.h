#ifndef SYMBOLS_H
#define SYMBOLS_H
#include<multiboot.h>
/* This file provides functions for loading and searching symbols names/values via GRUB Multiboot elf_sec entry in multiboot structure */
void symbols_init(multiboot_info_t *);
char *symbols_findName(int value);
int symbols_findValue(char *name);
void symbols_print();
void symbols_registerSymbolFromModule(char *name,int val);
void symbols_removeSymbol(char *name);
#endif
