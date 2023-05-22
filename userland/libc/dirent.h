#ifndef DIRENT_H
#define DIRENT_H
#include "stdio.h"
struct dirent
{
    char name[128];
    uint32_t index;
};

typedef struct _dir {
    FILE *file;
    int index;
} DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif