#ifndef IOCTL_H
#define IOCTL_H
#include <stdarg.h>
int ioctl(int fd, unsigned long request, ...);
#endif
