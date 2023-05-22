#ifndef UNISTD_H
#define UNISTD_H
#include "wait.h"
typedef int uid_t;
char *pwd(char *path,int len);
void chdir(char *path);
int execv(char *file,int argc,char *argv[]);
pid_t getppid();
uid_t getuid();
int setuid(uid_t uid);
#endif
