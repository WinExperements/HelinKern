#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

// yeah mount!
int mount(const char *source,const char *target,
        const char *filesystemtype,unsigned long mountflags,
        const void *data);

#endif