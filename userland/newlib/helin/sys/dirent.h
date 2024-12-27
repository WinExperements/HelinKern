// Returned by readdir and other thinks. All source code will be packed to
// syscalls.c

#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H
#include <stdint.h>
#include <sys/types.h>
#define DT_BLK	1
#define DT_CHR	2
#define DT_DIR	3
#define DT_FIFO 4
#define DT_LNK 8
#define DT_REG 	6
#define DT_SOCK 7
#define DT_UNKNOWN 0
struct _helin_dirent {
    char name[128];
    uint32_t index;
    int type;
};

struct dirent
{
   long d_ino;
   off_t d_off;
   unsigned short d_reclen;
   unsigned char d_type;
   char d_name[256];
};

typedef struct _dir {
    int _fd;
    int _pos;
    struct dirent *pointer;
    struct _helin_dirent *native_ptr;
} DIR;
#ifdef __cplusplus
extern "C" {
#endif
// linux/sys/dirent.h
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void rewinddir(DIR *);
int closedir(DIR *);
#if !defined(MAXNAMLEN) && __BSD_VISIBLE
#define MAXNAMLEN 1024
int alphasort(const struct dirent **a, const struct dirent **b);
DIR           *fdopendir(int);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *, struct dirent *,
                   struct dirent **);
void           rewinddir(DIR *);
int            scandir(const char *, struct dirent ***,
                   int (*)(const struct dirent *),
                   int (*)(const struct dirent **,
                   const struct dirent **));
void           seekdir(DIR *, long);
long           telldir(DIR *);
#endif

#ifdef __cplusplus
}
#endif
#endif
