#include <syscall.h>

struct MallocHeader
{
    unsigned long size:31;
    unsigned long used:1;
} __attribute__ ((packed));

void *sbrk(int increment) {
    return (void *)helin_syscall(35,increment,0,0,0,0);
}

typedef struct MallocHeader MallocHeader;
static char *m_kheap = NULL;
static int heap_used;

void allocator_init() {
    m_kheap = sbrk(4096);
}
