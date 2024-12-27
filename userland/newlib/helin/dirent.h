#ifndef _DIRENT_H_
#define _DIRENT_H_
#ifdef __cplusplus
extern "C" {
// Posix 2024....
#endif
#include <sys/cdefs.h>
#include <sys/dirent.h>

#if !defined(MAXNAMLEN) && __BSD_VISIBLE
#define MAXNAMLEN 1024
int alphasort(const struct dirent **a, const struct dirent **b);
DIR           *fdopendir(int);
DIR           *opendir(const char *);
struct dirent *readdir(DIR *);
int            readdir_r(DIR *restrict, struct dirent *restrict,
                   struct dirent **restrict);
void           rewinddir(DIR *);
int            scandir(const char *, struct dirent ***,
                   int (*)(const struct dirent *),
                   int (*)(const struct dirent **,
                   const struct dirent **));
void           seekdir(DIR *, long);
long           telldir(DIR *);
#define DT_DIR			1
#endif

#ifdef __cplusplus
}
#endif
#endif /*_DIRENT_H_*/
