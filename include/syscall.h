#ifndef SYSCALL_H
#define SYSCALL_H
#define POWEROFF_MAGIC 0xfffc04 // random number
#include <typedefs.h>
typedef int ino_t;
typedef int mode_t ;
typedef int nlink_t;
typedef int uid_t;
typedef int gid_t;
typedef int time_t;
typedef int blksize_t;
typedef int blkcnt_t;
typedef int off_t;
struct stat {
	int           st_dev;
  	ino_t         st_ino;
  	mode_t        st_mode;
  	nlink_t       st_nlink;
  	uid_t         st_uid;
  	gid_t         st_gid;
  	int         st_rdev;
  	off_t         st_size;
	time_t        st_atime;
  	long          st_spare1;
  	time_t        st_mtime;
  	long          st_spare2;
  	time_t        st_ctime;
  	long          st_spare3;
  	blksize_t     st_blksize;
  	blkcnt_t      st_blocks;
  	long  st_spare4[2];
};
// Syscall specific includes.
void syscall_init();
uintptr_t syscall_get(int n);
#endif
