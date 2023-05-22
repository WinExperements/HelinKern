#ifndef SYS_MMAN
#define SYS_MMAN
#include "stdio.h"
void *mmap(FILE *node,int addr,int size,int offset,int flags);
#endif
