#ifndef VFS_H
#define VFS_H
#include<typedefs.h>
#include <stdarg.h>
#include <syscall.h> // syscall sync for sys_stat and other VFS syscall specific things.
#define VFS_DIRECTORY 0x02
#define VFS_NONBLOCK  0x3
#define VFS_SYMLINK 0x4
#define VFS_SOCKET 0x5
#define VFS_BLOCK 0x1
#define VFS_CHR 0x7
/* File System Mount options */
#define VFS_MOUNT_RO	0x14	// Read-Only File system.
// 28.06.2024: File permission security support(Finally)
// Defined in POSIX
#define S_IRWXU		0700
#define S_IRUSR		0400
#define S_IWUSR		0200
#define S_IXUSR		0100
#define S_IRWXG		070
#define S_IRGRP		040
#define S_IWGRP		020
#define S_IXGRP		010
#define S_IRWXO		07
#define S_IROTH		04
#define S_IWOTH		02
#define S_IXOTH		01
#define S_ISUID		04000
#define S_ISGID		02000
#define S_ISVTX		01000
// End of POSIX section.
// HelinKern Specific parameters to vfs_hasPerm.
#define PERM_READ	1
#define PERM_WRITE	2
#define PERM_EXEC	3
#define	FD_RDWR		2
#define FD_RDONLY	0
typedef struct vfs_node {
    char *name;
    int mask;
    int gid;
    int uid;
    int flags;
    int inode;
    int size;
    /* Time */
    time_t ctime;	// creation time.
    time_t mtime;	// modification time.
    time_t atime;	// last access time.
    struct vfs_node *prev;
    struct vfs_node *first_child;
    struct vfs_node *next_child;
    struct vfs_fs *fs,*orig_fs;
    /* VFS specific */
    int mount_flags;
    void *fs_mountOptions; // FS specific options.
    void *device;
    void *priv_data;
} vfs_node_t;
struct dirent {
    char name[128];
    uint32_t node;
    int type; // type of file.
};
typedef struct vfs_fs {
    char *fs_name;
    bool (*mount)(struct vfs_node *dev,struct vfs_node *mountpoint,void *);
    uint64_t (*read)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    uint64_t (*write)(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
    void (*open)(struct vfs_node *node,bool w,bool r);
    void (*close)(struct vfs_node *node);
    struct vfs_node *(*finddir)(struct vfs_node *in,char *name);
    int (*readdir)(struct vfs_node *dir,unsigned int index,struct dirent *to);
    struct vfs_node *(*creat)(struct vfs_node *in,char *name,int flags);
    void (*truncate)(struct vfs_node *in,int size);
    bool (*readBlock)(struct vfs_node *node,int blockN,int how,void *buff);
    bool (*writeBlock)(struct vfs_node *node,int blockN,int how,void *buff);
    /* ioctls can return address pointers.... */
    uint64_t (*ioctl)(struct vfs_node *node,int request,va_list args);
    void *(*mmap)(struct vfs_node *node,int addr,int size,int offset,int flags);
    bool (*rm)(struct vfs_node *node);
    bool (*isReady)(struct vfs_node *node);
    bool (*umount)(struct vfs_node *node);
    int (*stat)(struct vfs_node *node,struct stat *stat);
    // Some file systems requires to change some data inside it's specific node
    int (*writeMetadata)(struct vfs_node *node);
    // Symlinks!
    int (*symlink)(vfs_node_t *from,const char *to);
    int (*readlink)(vfs_node_t *from,char *buffer,int size);
    // Extensions.
    int (*unmap)(vfs_node_t *node);
   struct vfs_fs *next;
} vfs_fs_t;
typedef struct file_descriptor {
	int pid; // owner of the FD
	vfs_node_t *node; // address of node
	int offset;
	int flags; // open flags.
} file_descriptor_t;
/* Virtual File System Mount Entry */
typedef struct vfsmnt {
	vfs_node_t *device;
	vfs_node_t *target;
	char *device_path;
	char *target_path;
	vfs_fs_t *fs;
	// Well, because all FS may have it's metadata, that is stored as priv_data.
	void *priv_data;
	struct vfsmnt *next;
  struct vfsmnt *prev;
} vfs_mount_t;
void vfs_init();
void vfs_addFS(vfs_fs_t *fs);
vfs_fs_t *vfs_findFS(char *name);
int vfs_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
int vfs_write(vfs_node_t *node,uint64_t offset,uint64_t how,void *buf);
void vfs_open(vfs_node_t *node,bool w,bool r);
void vfs_close(vfs_node_t *node);
int vfs_readdir(vfs_node_t *in,unsigned index,struct dirent *to);
vfs_node_t *vfs_finddir(vfs_node_t *in,char *name);
vfs_node_t *vfs_getRoot();
bool vfs_mount(vfs_fs_t *fs,vfs_node_t *dev,char *mountPoint);
vfs_node_t *vfs_creat(vfs_node_t *in,char *name,int flags);
void rootfs_init();
void rootfs_mount(char *to);
vfs_node_t *vfs_find(char *path);
void vfs_truncate(vfs_node_t *node,int size);
bool vfs_readBlock(vfs_node_t *node,int blockN,int how,void *buff);
bool vfs_writeBlock(vfs_node_t *node,int blockN,int how,void *buff);
int vfs_ioctl(vfs_node_t *node,int request,...);
void vfs_node_path(vfs_node_t *node,char *path,int size);
void rootfs_insertModuleData(vfs_node_t *node,int size,char *addr);
void *vfs_mmap(struct vfs_node *node,int addr,int size,int offset,int flags);
bool vfs_rm(struct vfs_node *node);
bool vfs_isReady(struct vfs_node *node);
bool vfs_umount(struct vfs_node *node);
vfs_mount_t *vfs_getMntList();
int vfs_writeMetadata(vfs_node_t *node);
bool vfs_hasPerm(vfs_node_t *node,int permType,int gr,int us);
int vfs_symlink(vfs_node_t *from,const char *to);
int vfs_readlink(vfs_node_t *from,char *buffer,int size);
int vfs_createFile(char *to,int type);
#endif
