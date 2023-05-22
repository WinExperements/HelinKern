#include "stdio.h"
#include "syscall.h"
#include "dirent.h"
#include "stdlib.h"

DIR *opendir(const char *path) {
    FILE *f = (FILE *)helin_syscall(18,(int)path,0,0,0,0);
    if (f == NULL) {
        return 0;
    }
    DIR *d = malloc(sizeof(DIR));
    if (d == NULL) {
        printf("dirent: cannot malloc memory. Strange\n");
    }
    d->file = f;
    d->index = 0;
    return d;
}
struct dirent *readdir(DIR *dir) {
    struct dirent *ret = (struct dirent *)helin_syscall(20,(int)dir->file,dir->index,0,0,0);
    dir->index++;
    return ret;
}
int closedir(DIR *dir) {
    int ret = helin_syscall(19,(int)dir->file,0,0,0,0);
    free(dir);
    return ret;
}
