#include "syscall.h"
#include "unistd.h"
char *pwd(char *buff,int len) {
    helin_syscall(16,(int)buff,len,0,0,0);
    return buff;
}
void chdir(char *path) {
    helin_syscall(17,(int)path,0,0,0,0);
}
int execv(char *path,int argc,char *argv[]) {
    return helin_syscall(13,(int)path,argc,(int)argv,0,0);
}
pid_t getppid() {
    return helin_syscall(23,0,0,0,0,0);
}
uid_t getuid() {
    return helin_syscall(25,0,0,0,0,0);
}
int setuid(uid_t uid) {
    return helin_syscall(26,uid,0,0,0,0);
}
