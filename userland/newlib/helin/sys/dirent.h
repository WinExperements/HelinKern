// Returned by readdir and other thinks. All source code will be packed to
// syscalls.c

#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
struct _helin_dirent {
    char name[128];
    uint32_t index;
};

struct dirent
{
   int _pos;
    // POSIX
    char *d_name; // actually an pointer
    int d_fileno;
    unsigned char d_namelen;
    unsigned char d_type;
};

typedef struct _dir {
    int _fd;
    int _pos;
    struct dirent *pointer;
    struct _helin_dirent *native_ptr;
} DIR;

// linux/sys/dirent.h
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int readdir_r(DIR *__restrict, struct dirent *__restrict,
              struct dirent **__restrict); // not supported, i don't know what is this
void rewinddir(DIR *);
int closedir(DIR *);
#ifdef __cplusplus
}
#endif
#endif
