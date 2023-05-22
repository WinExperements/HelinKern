#include "sys/mman.h"
#include "syscall.h"
void *mmap(FILE *node,int addr,int size,int offset,int flags) {
	return (void *)helin_syscall(29,(int)node,addr,size,offset,flags);
}
