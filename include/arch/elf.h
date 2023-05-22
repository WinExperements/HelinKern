#ifndef ARCH_ELF_H
#define ARCH_ELF_H
#include <elf.h>
bool arch_elf_check_header(Elf32_Ehdr *hdr);
#endif
