#include "syscall.h"
#include "sys/ioctl.h"

int ioctl(int fd, unsigned long request, void *arg) {
    return helin_syscall(32,fd,request,(int)arg,0,0);
}
