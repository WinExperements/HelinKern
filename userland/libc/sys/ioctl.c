#include "syscall.h"
#include "sys/ioctl.h"
#include <stdarg.h>

int ioctl(int fd, unsigned long request,...) {
    va_list args;
    va_start(args,request);
    int ret = helin_syscall(32,fd,request,(int)args,0,0);
    va_end(args);
    return ret;
}
