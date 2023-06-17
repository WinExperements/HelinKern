#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

// Used LyOS newlib headers

#include <sys/cdefs.h>
#include <sys/types.h>

// Copied from the LyOS newlib port

/* Warrning: This copy-paste is only invoked to support mmap feature, the defines is begin changes as soon the kernel is added fully mmap support */

/* There next defines kernel didn't support currently */

#define PROT_NONE  0x00 /* no permissions */
#define PROT_READ  0x01 /* pages can be read */
#define PROT_WRITE 0x02 /* pages can be written */
#define PROT_EXEC  0x04 /* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define MAP_SHARED  0x0001 /* share changes */
#define MAP_PRIVATE 0x0002 /* changes are private */

/*
 * Mapping type
 */
#define MAP_ANONYMOUS 0x0004 /* anonymous memory */
#define MAP_ANON      MAP_ANONYMOUS

#define MAP_FIXED    0x0008
#define MAP_POPULATE 0x0010
#define MAP_CONTIG   0x0020

/*
 * Error indicator returned by mmap(2)
 */
#define MAP_FAILED ((void*)-1) /* mmap() failed */

/*
 * Linux extension
 */
#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED   2

__BEGIN_DECLS

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t len);

void* mremap(void*, size_t, size_t, int, ...);

__END_DECLS

#endif
