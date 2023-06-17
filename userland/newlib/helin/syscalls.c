/* Newlib port to HelinOS with HelinKern */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

int helin_syscall(int num,int p1,int p2,int p3,int p4,int p5) {
    // use asm macro for it
    int ret = 0;
    asm volatile("int %1\n"
                : "=a" (ret)
                : "i" (0x80),
                "a" (num),
                "d" (p1),
                "c" (p2),
                "b" (p3),
                "D" (p4),
                "S" (p5)
                : "cc", "memory");
    return ret;
}
void _exit() {
    helin_syscall(2,0,0,0,0,0);
}
int close(int file) {
	if (file <= 2) return 0;
    return helin_syscall(8,file,0,0,0,0);
}
char **environ; /* pointer to array of char * strings that define the current environment variables */
int execve(char *name, char **argv, char **env) {
    int pid = helin_syscall(13,(int)name,sizeof(argv)/sizeof(argv[1]),argv,0,0);
    if (pid < 0) return -1;
    helin_syscall(22,pid,0,0,0,0);
    return 0;
}
int fork() {
    return -1;
}
int fstat(int file, struct stat *st) {
    return -1;
}
int getpid(){
    return helin_syscall(4,0,0,0,0,0);
}
int isatty(int file){
    return false;
}
int kill(int pid, int sig){
    helin_syscall(3,pid,0,0,0,0);
    return 0;
}
int link(char *old, char *new){}
int lseek(int file, int ptr, int dir){}
int open(const char *name, int flags, ...){
    return helin_syscall(7,(int)name,flags,0,0,0);
}
int read(int file, char *ptr, int len){
    return helin_syscall(9,file,0,len,(int)ptr,0);
}
caddr_t sbrk(int incr){
    return (caddr_t)helin_syscall(35,incr,0,0,0,0);
}
int stat(const char *file, struct stat *st){}
clock_t times(struct tms *buf){}
int unlink(char *name){}
int wait(int *status){}
int write(int file, char *ptr, int len){
    return helin_syscall(10,file,0,len,(int)ptr,0);
}
int gettimeofday(struct timeval *__restrict __p,
                          void *__restrict __tz){}
int ioctl(int fd,unsigned long request,...) {
    return 0;
}
void *mmap(void *addr,size_t len,int prot,int flags,int fd,off_t offset) {
    return (void *)helin_syscall(29,fd,addr,len,offset,flags);
}
