#ifndef SYSCALL_H
#define SYSCALL_H
#define POWEROFF_MAGIC 0xfffc04 // random number
#include <typedefs.h>

// Syscall specific includes.
struct stat {
	int st_dev;
	int st_mode;
	int st_ino;
	int st_nlink;
	int st_uid;
	int st_gid;
	int st_blksize;
	int st_size;
	int st_blocks;
	int st_ctime;
	int st_atime;
	int st_mtime;
};
void syscall_init();
int syscall_get(int n);
#endif
