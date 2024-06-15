#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H
#define MNT_MAX_NAME 90
#ifdef __cplusplus
extern "C" {
#endif
struct statfs {
	char f_mntfromname[MNT_MAX_NAME];
	char f_mnttoname[MNT_MAX_NAME];
	char f_fstypename[MNT_MAX_NAME];
};
// yeah mount!
int mount(const char *source,const char *target,
        const char *filesystemtype,unsigned long mountflags,
        const void *data);

// umount
int umount(char *target);
int getfsstat(struct statfs *buf,long bufsize,int mode);
#ifdef __cplusplus
}
#endif
#endif
