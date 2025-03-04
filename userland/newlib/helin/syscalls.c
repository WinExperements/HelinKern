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
#include <sys/dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <stdarg.h>
#include <termios.h> // Hide and show user input
#include <time.h>
#include <mntent.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/syscall.h> // syscall table, for better code style.
#include <sys/resource.h>
/* I finally managed to open the POSIX specifications */
#include <spawn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <grp.h>
#define PATH_MAX 40
// Structure from src/thread.c from kernel source

static int globalMask = 0;
// TODO: Need to be rewritten as arch-specific code.
int helin_syscall(int num,uintptr_t p1,uintptr_t p2,uintptr_t p3,uintptr_t p4,uintptr_t p5) {
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
void (*prep_forkHandler)(void);
void (*parent_forkHandler)(void);
void (*child_forkHandler)();
void _exit(int retcode) {
    helin_syscall(2,retcode,0,0,0,0);
}
int close(int file) {
    int ret =  helin_syscall(8,file,0,0,0,0);
    if (ret < 0) {
	    errno = ret * -1;
	    return -1;
	}
    return 0;
}
char **environ; /* pointer to array of char * strings that define the current environment variables */
int execve(char *name, char **argv, char **env) {
    int num_args = 0;

    // Count the number of arguments until we reach a NULL pointer in the argv array
    while (argv[num_args] != NULL) {
        num_args++;
    }
    // We are UNIX system. In this space exec() is replacing caller.
    errno = helin_syscall(13, (uintptr_t)name, num_args, (uintptr_t)argv, (uintptr_t)env, 0);
    return -1;
}
int fork() {
	if (prep_forkHandler != NULL && (uintptr_t)prep_forkHandler > 0x1000 &&
			(uintptr_t)prep_forkHandler <= 0x50000000) {
		prep_forkHandler();
	}
    	int ret = helin_syscall(49,0,0,0,0,0);
	if (ret == 0) {
		if (child_forkHandler != NULL && (uintptr_t)child_forkHandler > 0x1000 &&
				(uintptr_t)child_forkHandler <= 0x50000000) {
			child_forkHandler();
		}
	} else if (parent_forkHandler != NULL && (uintptr_t)parent_forkHandler > 0x1000 &&
			(uintptr_t)parent_forkHandler < 0x50000000) {
		parent_forkHandler();
	}
	return ret;
}
struct kernelStat {
	int           	st_dev;
  	int           	st_ino;
  	int          	st_mode;
  	int         	st_nlink;
  	int           	st_uid;
  	int           	st_gid;
  	int          	st_rdev;
  	int         	st_size;
	int        	st_atime;
  	long          	st_spare1;
  	int        	st_mtime;
  	long          	st_spare2;
  	int        	st_ctime;
  	long          	st_spare3;
	int 		st_blksize;
  	int	      st_blocks;
  	long  st_spare4[2];
};
static void convertKrnStat(struct kernelStat *from,struct stat *to) {
	if (from == NULL || to == NULL) return;
	to->st_dev = from->st_dev;
	to->st_ino = from->st_ino;
	to->st_mode = from->st_mode;
	to->st_nlink = from->st_nlink;
	to->st_uid = from->st_uid;
	to->st_gid = from->st_gid;
	to->st_rdev = from->st_rdev;
	to->st_size = from->st_size;
	to->st_atime = from->st_atime;
	to->st_mtime = from->st_mtime;
	to->st_ctime = from->st_ctime;
	to->st_blksize = from->st_blksize;
	to->st_blocks = from->st_blocks;
}
static struct kernelStat *krnStat;
int fstat(int file, struct stat *st) {
    errno = helin_syscall(SYS_fstat,file,(uintptr_t)krnStat,0,0,0);
    if (errno > 0) {
	return -1;
    }
    convertKrnStat(krnStat,st);
    return 0;
}
// 14.09.2024: Extension to add full sys/stat.h support.
int utimes(const char *name,const struct timeval *times) {
	// HelinKern only accepts file descriptors....
	int fd = open(name,O_RDWR);
	if (fd < 0) {
		return -1;
	}
	int ret = helin_syscall(SYS_utime,fd,(uintptr_t)times,0,0,0);
	if (ret < 0) {
		close(fd);
		errno = ret * -1;
		return -1;
	}
	close(fd);
	return 0;
}
int futimens(int fd,const struct timespec *times) {
	int ret = helin_syscall(SYS_utime,fd,(uintptr_t)times,0,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
int utimensat(int dirfd,const char *name,const struct timespec *times,int flags) {
	int fd = openat(dirfd,name,O_RDWR);
	if (fd < 0) {
		return -1;
	}
	int ret = helin_syscall(SYS_uname,fd,(uintptr_t)&times,flags,0,0);
	close(fd);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
int getpid(){
    return helin_syscall(4,0,0,0,0,0);
}
int getuid() {
	return helin_syscall(SYS_getuid,0,0,0,0,0);
}
int isatty(int file){
	int checksum;
	if (ioctl(file,4,&checksum) < 0) {
		errno = ENOTTY;
		return -1;
	}
	if (checksum != 10) {
		errno = ENOTTY;
		return -1;
	}
	return 1;
}
int kill(int pid, int sig){
    int ret = helin_syscall(3,pid,sig,0,0,0);
    if (ret < 0) {
      errno = ret * -1;
      return -1;
    }
    return 0;
}
int link(char *old, char *new){
	errno = helin_syscall(SYS_link,(uintptr_t)old,(uintptr_t)new,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int lseek(int file, int ptr, int dir){
    return helin_syscall(27,file,dir,ptr,0,0);
}
int open(const char *name, int flags, ...){
	int ret = helin_syscall(SYS_open,(uintptr_t)name,flags,0,0,0);
	if (ret < 0) {
		ret = ret * -1;
		errno = ret;
		if (errno != ret) {
			printf("WHATAHEL");
		}
		return -1;
	}
	return ret;
}
int read(int file, char *ptr, int len){
	int how = helin_syscall(9,file,0,len,(uintptr_t)ptr,0);
	if (how < 0) {
		errno = how * -1;
		return -1;
	}
	return how;
}
caddr_t sbrk(int incr){
    caddr_t ret = (caddr_t)helin_syscall(35,incr,0,0,0,0);
    if ((uintptr_t)ret < 0) {
      errno = (int)ret * -1;
      return -1;
    }
    return ret;
}
int stat(const char *file, struct stat *st){
	int ret = helin_syscall(SYS_stat,(uintptr_t)file,(uintptr_t)krnStat,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	convertKrnStat(krnStat,st);
	return 0;
}
clock_t times(struct tms *buf){}
int unlink(char *name){
	errno = helin_syscall(SYS_unlink,(uintptr_t)name,0,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int wait(int *status){
  return waitpid(-1,status,0);
}
int write(int file, char *ptr, int len){
    int how = helin_syscall(10,file,0,len,(uintptr_t)ptr,0);
    if (how < 0) {
	    errno = how * -1;
	    return -1;
    }
    return how;
}
int gettimeofday(struct timeval *__restrict __p,
                          void *__restrict __tz){
	// same as clock_gettime.
	struct timespec *t = malloc(sizeof(struct timespec));
	int ret = clock_gettime(CLOCK_REALTIME,t);
	if (ret < 0) {
		return -1;
	}
	__p->tv_sec = t->tv_sec;
	__p->tv_usec = t->tv_nsec * 1000000;
	return 0;
}
int ioctl(int fd,unsigned long request,...) {
    va_list args;
    va_start(args,request);
    int ret = helin_syscall(32,fd,request,(uintptr_t)args,0,0);
    va_end(args);
    return ret;
}
void *mmap(void *addr,size_t len,int prot,int flags,int fd,off_t offset) {
    int ret = helin_syscall(29,fd,(uintptr_t)addr,len,offset,flags);
    if (ret < 0 && ret > -255) {
	    errno = ret * -1;
	    return (void*)-1;
	  }
    void *ptr = (void *)(uintptr_t)(uint32_t)ret;
    return ptr;
}
DIR *opendir(const char *path) {
    int fd = helin_syscall(18,(uintptr_t)path,0,0,0,0);
    if (fd < 0) {
	    errno = fd * -1;
        return NULL;
    }
    DIR *ret = (DIR *)malloc(sizeof(DIR));
    memset(ret,0,sizeof(DIR));
    ret->_fd = fd;
    ret->_pos = 0;
    ret->pointer = (struct dirent *)malloc(sizeof(struct dirent));
    memset(ret->pointer,0,sizeof(struct dirent));
    ret->native_ptr = (struct _helin_dirent *)malloc(sizeof(struct _helin_dirent));
    memset(ret->native_ptr,0,sizeof(struct _helin_dirent));
    return ret;
}

struct dirent *readdir(DIR *d) {
    if (!d) return NULL;
    // get platform specific dirent then convert it to system
    int ret = helin_syscall(20,d->_fd,d->_pos,(uintptr_t)d->native_ptr,0,0);
    if (ret < 0) {
	    errno = ret * -1;
	    return NULL;
    } else if (ret == 0) {
	    errno = 0;
	    return NULL;
	}
    // Increment the file position
    d->_pos++;
    struct dirent *dire = (struct dirent *)d->pointer;
    // Copy the name
    strncpy(dire->d_name,d->native_ptr->name,256);
    dire->d_type = d->native_ptr->type;
    dire->d_off = d->_pos;
    //printf("helinlibc: readdir: ok\n");
    return dire;
}

int closedir(DIR *dir) {
    // The closedir syscall is redirected to sys_close in HelinKern source code, so just call
    // the close syscall and free the structure
    if (dir == NULL) {
        return -1;
    }
    // Free our pointer of struct dirent
    free(dir->pointer);
    free(dir->native_ptr);
    close(dir->_fd);
    free(dir);
    return 0;
}
DIR *fdopendir(int fd) {
	DIR *ret = (DIR *)malloc(sizeof(DIR));
    	memset(ret,0,sizeof(DIR));
    	ret->_fd = fd;
	ret->_pos = 0;
	ret->pointer = (struct dirent *)malloc(sizeof(struct dirent));
    	memset(ret->pointer,0,sizeof(struct dirent));
    	ret->native_ptr = (struct _helin_dirent *)malloc(sizeof(struct _helin_dirent));
    	memset(ret->native_ptr,0,sizeof(struct _helin_dirent));
    	return ret;
}
int readdir_r(DIR *dir,struct dirent *entry,struct dirent **result) {
	if (!dir) {
		errno = 9;
		return -1;
	}
	struct dirent *r = readdir(dir);
	if (r == NULL) {
		*result = NULL;
		return 0;
	}
	memcpy(entry,r,sizeof(struct dirent));
	memcpy(*result,r,sizeof(struct dirent));
	return 0;
}
void rewinddir(DIR *dir) {
	if (!dir) {
		errno = 9;
		return -1;
	}
	// Reset the pointer.
	dir->_pos = 0;
}
void seekdir(DIR *dir,long pos) {
	if (!dir) return;
	dir->_pos = pos;
}
long telldir(DIR *dir) {
	if (!dir) {
		return;
	}
	return dir->_pos;
}
int dirfd(DIR *d) {
	if (!d) { errno = 9; return -1;}
	return d->_fd;
}
int chdir (const char *path) {
    return helin_syscall(17,(uintptr_t)path,0,0,0,0);
}
int getgid (void) {
    return helin_syscall(34,0,0,0,0,0);
}
int setgid(int uid) {
    helin_syscall(33,uid,0,0,0,0);
}
int setuid(int uid) {
    helin_syscall(SYS_setuid,uid,0,0,0,0);
}
char *getcwd (char *__buf, size_t __size) {
	// glib extension
	if (__buf == NULL) {
		__buf = malloc(__size);
	}
    	int ret = helin_syscall(16,(uintptr_t)__buf,__size,0,0,0);
    	if (ret < 0) {
	    	errno = ret * -1;
	    	return NULL;
	}
    	return __buf;
}

int mount(const char *source,const char *target,
              const char *filesystemtype,unsigned long mountflags,
              const void *data) {
    int ret = helin_syscall(21,(uintptr_t)source,(uintptr_t)target,(uintptr_t)filesystemtype,mountflags,(uintptr_t)data);
    if (ret < 0) {
	    errno = ret * -1;
    	return -1;
    }
    return ret;
}
int getfsstat(struct statfs *buf,long bufsize,int mode) {
	int ret = helin_syscall(SYS_getfsstat,(uintptr_t)buf,(uintptr_t)bufsize,mode,0,0);
	return ret;
}
// waitpid
pid_t waitpid(pid_t pid,int *status,int options) {
    return helin_syscall(22,pid,(uintptr_t)status,options,0,0);
}
// Need to port dash shell to our OS
int dup(int oldfd) {
	return helin_syscall(36,oldfd,0,1,0,0);
}
int dup2(int oldfd, int newfd) {
	return helin_syscall(36,oldfd,newfd,2,0,0);
}
int execvp(const char *file, char *const argv[]) {
  // Read manual carefully!
  // exevp types of libc calls acts like shell programs, trying to search the executable by it's name.
  char *pat = strdup(getenv("PATH"));
  // Now go.
  char *start = strtok(pat,":");
  char *execBuff = malloc(100);
  struct stat st;
  while(start != NULL) {
    snprintf(execBuff, 100, "%s/%s",start,file);
    // check if file exist.
    if (stat(execBuff,&st) == 0) {
      return execv(execBuff,argv);
    }
    start = strtok(NULL,":");
  }
  free(pat);
  free(execBuff);
  errno = ENOENT;
  return -1;
}
int execv(const char *path, char *const argv[]) {
	return execve(path,argv,environ);
}
int pipe(int pipefd[2]) {
  // Use IPC manager API.
  return helin_syscall(SYS_ipc,1,0x50,0,(uintptr_t)pipefd,0);
}
// Well...sys_clone literaly clones the process...so the arguments must be like in crt0(argc is set 1, argv is to pthread_str and environ to NULL at start)
// Pthread
int pthread_equal(pthread_t t1, pthread_t t2) {
	return t1 == t2;
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
	// allocate new stack.
	int pid = helin_syscall(37,0,0,0,0,0);
	if (pid > 0) {
		*thread = pid;
	} else {
		// we are child that running on separate stack?
		if (attr != NULL) {
			// Change stack if required.
			if (attr->is_initialized && attr->stackaddr != NULL && attr->stacksize != 0) {
				uintptr_t newPtr = (uintptr_t)attr->stackaddr+attr->stacksize;
#if defined(__x86_64__)
				asm volatile("mov %0, %%rsp\n" \
						"call *%1"
						: : "r"(newPtr),"r"(start_routine));
#endif
			}
		}
		start_routine(arg);
		_exit(1);
	}
	// Wait for start via sys_waitForStart
	helin_syscall(38,pid,0,0,0,0);
	return 0;
}

void pthread_exit(void* retval) {
	// Just exit
	// WE ARE THREADS! WE CAN'T USE 'exit' BECAUSE IT WILL BROKE EVERYTHING!
	_exit(0);
}

int pthread_join(pthread_t thread, void** retval) {
	waitpid(thread,NULL,0);
}

int usleep(int ms) {
	helin_syscall(39,ms,0,0,0,0);
	return 0;
}
int sleep(int sec) {
	return usleep(sec * 1000);
}

int tcgetattr(int fd,struct termios* tio) {
	// Return success
	ioctl(fd,1,(uintptr_t)tio);
	return 0;
}
int     tcsetattr(int fd, int type, const struct termios *term) {
	ioctl(fd,2,(uintptr_t)term);
	return 0;
}
int tcflow(int fd,int action) {
	errno = ENOSYS;
	return -1;
}
int access(const char *pathname,int mode) {
	return helin_syscall(SYS_access,(uintptr_t)pathname,mode,0,0,0);
}


void pthread_cleanup_push(void (*routine)(void *),
                                 void *arg) {}
void pthread_cleanup_pop(int execute) {}

int pthread_attr_init(pthread_attr_t *attr) {
	if (attr == NULL) {
		return -1;
	}
	attr->is_initialized = 1;
	return 0;
}
int pthread_attr_destroy(pthread_attr_t *attr) {
	free(attr); // ?
}

int fcntl(int fd, int cmd, ...) {
	va_list args;
	va_start(args,cmd);
	int parameter = va_arg(args,int);
	va_end(args);
	int ret = helin_syscall(SYS_fcntl,fd,cmd,(int)parameter,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return ret; // We don't support :(
}

int lstat(const char *pathname, struct stat *statbuf) {
	return stat(pathname,statbuf);
}

mode_t umask(mode_t mask) {
	globalMask = mask;
	return 0;
}

int chmod(const char *pathname, mode_t mode) {
	// SYS_chmod require at least the file descriptor, so first open
  int ret = helin_syscall(SYS_chmod,(uintptr_t)pathname,mode,0,0,0);
	if (ret < 0) {
    errno = ret * -1;
		return -1;
	}
	return 0;
}

long sysconf(int name) {
	return 0;
}

int utime(const char *filename, const struct utimbuf *times) {
	return 0;
}

int chown(const char *pathname, uid_t owner, gid_t group) {
	return helin_syscall(SYS_chown,2,(uintptr_t)pathname,owner,group,0);
}

int rmdir(const char *pathname) {
	return helin_syscall(SYS_rm,2,(uintptr_t)pathname,0,0,0);
}


extern struct mntent *getmntent (FILE *__stream) {
  // We need to have at least
	return 0;
}
int truncate(const char *path, off_t length) {
	// We have this syscall but corrently just return postive
	int fd = open(path,0);
	if (fd < 0) return -1;
	int ret = ftruncate(fd,length);
	close(fd);
	return ret;
}
int ftruncate(int fd, off_t length) {
	return helin_syscall(40,fd,length,0,0,0);
}

uint32_t htonl(uint32_t hostlong) {return hostlong;}
dev_t makedev(unsigned int maj, unsigned int min) {
	dev_t blank;
	return blank;
}
int getegid() {
	return getgid();
}

int sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact) {
        int r = helin_syscall(SYS_sigaction,signum,(uintptr_t)act,(uintptr_t)oldact,0,0);
        if (r > 0) {
                errno = r;
                return -1;
        }
	return 0;
}

int gethostname(char *name, size_t len) {
	if (name == NULL || len == 0) return -1;
	return helin_syscall(52,(uintptr_t)name,len,0,0,0);
}

int sethostname(char *name,size_t len) {
	return helin_syscall(51,(uintptr_t)name,len,0,0,0);
}

uid_t geteuid(void) {
	return getuid();
}

uint16_t htons(uint16_t hostshort) {return hostshort;}

uint32_t ntohl(uint32_t netlong) {return netlong;}

uint16_t ntohs(uint16_t netshort) { return netshort;}
int pclose(FILE *stream) {
	if (stream == NULL) return -1;
	int fd = fileno(stream);
	return close(fd);
}
static struct passwd empty;
static struct passwd *passwdRet;
static FILE *passwdFile = NULL;
static struct group *groupRet;
static FILE *groupFile = NULL;
struct passwd *getpwuid(uid_t uid) {
  // UNIX file structure :)
  if (passwdFile == NULL) {
	passwdFile = fopen("/etc/passwd","r");
  	if (!passwdFile) {
    		errno = ENOENT;
    		return errno;
  	}
  }
  // Parse passwd.
  if (passwdRet == NULL) {
    passwdRet = malloc(sizeof(struct passwd));
    memset(passwdRet,0,sizeof(struct passwd));
  }
  char internalBuffer[100];
  while(fgets(internalBuffer,100,passwdFile)) {
    /*if (passwdRet->pw_name != NULL) {
      // Free all element that previously filled.
      free(passwdRet->pw_name);
      free(passwdRet->pw_passwd);
      free(passwdRet->pw_gecos);
      free(passwdRet->pw_shell);
    }*/
    char *element = strtok(internalBuffer,":");
    passwdRet->pw_name = element;
    passwdRet->pw_passwd = strtok(NULL,":");
    passwdRet->pw_uid = atoi(strtok(NULL,":"));
    passwdRet->pw_gid = atoi(strtok(NULL,":"));
    passwdRet->pw_gecos = strtok(NULL,":");
    passwdRet->pw_dir = strtok(NULL,":");
    passwdRet->pw_shell = strtok(NULL,":");
    // Remove newline from latest strtok.
    passwdRet->pw_shell[strlen(passwdRet->pw_shell)-1] = 0;
    if (passwdRet->pw_uid == uid) {
	    fseek(passwdFile,0,SEEK_SET); // restore the pointer.
      return passwdRet;
    }
  }
  fseek(passwdFile,0,SEEK_SET);
  return NULL;
}
void endpwent() {
	if (passwdFile != NULL) {
		fclose(passwdFile);
		passwdFile = NULL;
	}
}
struct passwd *getpwent() {
	if (passwdFile == NULL) {
		passwdFile = fopen("/etc/passwd","r");
		if (!passwdFile) {
			return -1;
		}
	}
	if (passwdRet == NULL) {
		passwdRet = malloc(sizeof(struct passwd));
		memset(passwdRet,0,sizeof(struct passwd));
	}
	char internalBuffer[100];
	while(fgets(internalBuffer,100,passwdFile)) {
		char *element = strtok(internalBuffer,":");
		passwdRet->pw_name = element;
		passwdRet->pw_passwd = strtok(NULL,":");
		passwdRet->pw_uid = atoi(strtok(NULL,":"));
		passwdRet->pw_gid = atoi(strtok(NULL,":"));
		passwdRet->pw_gecos = strtok(NULL,":");
		passwdRet->pw_dir = strtok(NULL,":");
		passwdRet->pw_shell = strtok(NULL,":");
		passwdRet->pw_shell[strlen(passwdRet->pw_shell)-1] = 0;
		return passwdRet;
	}
	// Okay, restore the pointer.
	fseek(passwdFile,0,SEEK_SET);
	return NULL;
}
struct passwd *getpwnam(const char *name) {
	// just like getpwuid.
	if (passwdFile == NULL) {
        passwdFile = fopen("/etc/passwd","r");
        if (!passwdFile) {
                errno = ENOENT;
                return errno;
        }
  }
  // Parse passwd.
  if (passwdRet == NULL) {
    passwdRet = malloc(sizeof(struct passwd));
    memset(passwdRet,0,sizeof(struct passwd));
  }
  char internalBuffer[100];
  while(fgets(internalBuffer,100,passwdFile)) {
    char *element = strtok(internalBuffer,":");
    passwdRet->pw_name = element;
    passwdRet->pw_passwd = strtok(NULL,":");
    passwdRet->pw_uid = atoi(strtok(NULL,":"));
    passwdRet->pw_gid = atoi(strtok(NULL,":"));
    passwdRet->pw_gecos = strtok(NULL,":");
    passwdRet->pw_dir = strtok(NULL,":");
    passwdRet->pw_shell = strtok(NULL,":");
    // Remove newline from latest strtok.
    passwdRet->pw_shell[strlen(passwdRet->pw_shell)-1] = 0;
    if (!strcmp(passwdRet->pw_name,name)) {
            fseek(passwdFile,0,SEEK_SET); // restore the pointer.
      return passwdRet;
    }
  }
  fseek(passwdFile,0,SEEK_SET);
  return NULL;
}
int getpwnam_r(const char *name, struct passwd *pwd, char *buff,
                   size_t bufflen, struct passwd **result) {
	errno = ENOSYS;
	return -1;
}
void setpwent() {
	if (passwdFile == NULL) {
		// Okay. Maybe this is first call or the endpwent called before us.
		passwdFile = fopen("/etc/passwd","r");
		if (passwdFile == NULL) return;
	}
	fseek(passwdFile,0,SEEK_SET);
}
// grp.h
// fill the group structure based on the source string. We just want clearer code.
static void makeGroupEntry(struct group *to,char *source) {
	if (to == NULL) return;
	to->gr_name = strtok(source,":");
	// Skip the unused password entry.
	strtok(NULL,":");
	to->gr_gid = atoi(strtok(NULL,":"));
	// Now. Calculate count of members in this group.
	char *members = strtok(NULL,":");
	char *ptr = members;
	int count = 1;
	while(*ptr) {
		if (*ptr == ',') {
			count++;
		}
		*ptr++;
	}
	// Now...allocate the array? But firstly we need to free previous entry array.
	if (to->gr_mem != NULL) {
		free(to->gr_mem);
		to->gr_mem = NULL;
	}
	to->gr_mem = malloc(sizeof(char *) * count);
	to->gr_mem[0] = strtok(members,",");
	for (int i = 1; i < count; i++) {
		to->gr_mem[i] = strtok(NULL,",");
	}
}
void endgrent() {
	// ...close and free the return structure?
	if (groupFile != NULL) {
		fclose(groupFile);
	}
	if (groupRet != NULL) {
		free(groupRet->gr_mem);
		free(groupRet);
	}
}
struct group *getgrent() {
	// Open group file if not done already.
	if (groupFile == NULL) {
		groupFile = fopen("/etc/group","r");
		if (groupFile == NULL) {
			return NULL;
		}
	}
	// Allocate return group structure if doesn't done already.
	if (groupRet == NULL) {
		groupRet = malloc(sizeof(struct group));
		memset(groupRet,0,sizeof(struct group));
	}
	char buff[100];
	if (fgets(buff,100,groupFile) != NULL) {
		// Okay, we still can't read something. Convert buffer into actual group structure.
		makeGroupEntry(groupRet,buff);
		return groupRet;
	}
	return NULL;
}
struct group *getgrgid(gid_t gid) {
	if (groupFile == NULL) {
                groupFile = fopen("/etc/group","r");
                if (groupFile == NULL) {
                        return NULL;
                }
        }
        // Allocate return group structure if doesn't done already.
        if (groupRet == NULL) {
                groupRet = malloc(sizeof(struct group));
                memset(groupRet,0,sizeof(struct group));
        }
	char buff[100];
	while(fgets(buff,100,groupFile)) {
		makeGroupEntry(groupRet,buff);
		if (groupRet->gr_gid == gid) {
			fseek(groupFile,0,SEEK_SET);
			return groupRet;
		}
	}
	fseek(groupRet,0,SEEK_SET);
	return NULL;
}
struct group *getgrnam(const char *name) {
	if (groupFile == NULL) {
                groupFile = fopen("/etc/group","r");
                if (groupFile == NULL) {
                        return NULL;
                }
        }
        // Allocate return group structure if doesn't done already.
        if (groupRet == NULL) {
                groupRet = malloc(sizeof(struct group));
                memset(groupRet,0,sizeof(struct group));
        }
        char buff[100];
        while(fgets(buff,100,groupFile)) {
                makeGroupEntry(groupRet,buff);
                if (!strcmp(groupRet->gr_name,name)) {
                        fseek(groupFile,0,SEEK_SET);
                        return groupRet;
                }
        }
        fseek(groupRet,0,SEEK_SET);
        return NULL;
}
void setgrent() {
	if (groupFile == NULL) {
		groupFile = fopen("/etc/group","r");
		if (groupFile == NULL) {
			return;
		}
	}
	fseek(groupFile,0,SEEK_SET);
}
FILE *popen(const char *command, const char *type) {
	return NULL; // no pipe support :(
}

// Non syscalls related
const char *basename(const char *path) {
    if (path == NULL) {
        return NULL;
    }

    // Find the last directory separator
    const char *lastSlash = strrchr(path, '/');
    if (lastSlash == NULL) {
        return path; // No slashes, return the original path
    }

    // Return the portion of the path after the last slash
    return lastSlash + 1;
}
// written in nano
int daemon(int nochdir, int noclose) {
	if (nochdir == 0) {
		chdir("/");
	}
	if (noclose == 0) {
		// redirect the all input-output to /dev/null(BSD function)
		int nulldev = open("/dev/null",O_RDWR);
		if (nulldev < 0) {
			errno = 13;
			return -1;
		}
		dup2(0,nulldev);
		dup2(1,nulldev);
		dup2(2,nulldev);
	}
	return 0;
}

int fchmod(int fd, mode_t mode) {
	int ret = helin_syscall(SYS_fchmod,fd,mode,0,0,0);
	if (errno < 0) {
    errno = ret * -1;
		return -1;
	}
	return 0;
}
int alphasort(const struct dirent **a, const struct dirent **b) {return 0;}
unsigned int alarm(unsigned int seconds) {
	return seconds-1;
}
unsigned int major(dev_t dev) {return 2;}

int scandir(const char *dirp, struct dirent ***namelist,
              int (*filter)(const struct dirent *),
              int (*compar)(const struct dirent **, const struct dirent **)) {
	return 0;
}


unsigned int minor(dev_t dev) {return 2;}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
	int ret = helin_syscall(SYS_readlink,(uintptr_t)pathname,(uintptr_t)buf,bufsiz,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
ssize_t readlinkat(int dirfd,const char *pathname,char *buff,int buffsize) {
	int ret = helin_syscall(SYS_readlinkat,(uintptr_t)pathname,dirfd,(uintptr_t)buff,buffsize,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
pid_t setsid(void) {
	return -1;
}
int mkdir(const char *path, mode_t mode) {
	int ret =  helin_syscall(SYS_creat,(uintptr_t)path,DT_DIR | mode,0,0,0);
  if (ret > 0) {
    errno = ret;
    return -1;
  }
  return 0;
}
int creat(const char *path, mode_t mode) {
	int ret = helin_syscall(SYS_creat,(uintptr_t)path,mode,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int execlp(const char *file, const char *arg, ...) {
	char *args[64]; // Maximum 64 arguments (adjust as needed)

    args[0] = (char *)file; // The first argument is the file name
    args[1] = (char *)arg;  // The second argument is the initial argument

    va_list ap;
    va_start(ap, arg);

    int arg_count = 2; // Start from index 2 for additional arguments

    // Read additional arguments from the variable argument list
    while (1) {
        char *next_arg = va_arg(ap, char *);
        if (next_arg == NULL) {
            break;
        }
        args[arg_count++] = next_arg;
    }
    va_end(ap);

    // NULL-terminate the arguments array
    args[arg_count] = NULL;

    // Call execv
    execv(file, args);
    return 0;
}


int munmap(void* addr, size_t len) {
	// Насправді, ядро на даний момент не підтримує функцію "arch_mmu_unmap", тому ми повернемо позитивний результат нічого не роблючи
	errno = helin_syscall(SYS_munmap,(uintptr_t)addr,len,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}

int getpagesize() {
	/*
	 * Все ядро використовує 4 КБ як сторінку оперативної пам'яті
	*/
	return 4096;
}


// Network syscalls
int     socket(int domain, int type, int protocol) {
	return helin_syscall(41,domain,type,protocol,0,0);
}
int     bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return helin_syscall(42,sockfd,addr,addrlen,0,0);
}
int     listen(int sockfd, int backlog) {
	return helin_syscall(43,sockfd,backlog,0,0,0);
}
int     accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	return helin_syscall(44,sockfd,addr,addrlen,0,0);
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout)
{
  if (readfds == NULL && writefds == NULL && exceptfds == NULL) {
    errno = ENOSYS;
    return -1;
  }
	for (int i = 0; i < FD_SETSIZE; i++) {
		if (FD_ISSET(i,readfds)) {
			// check if it socket and if it is ready to accept
			int type = helin_syscall(48,i,0,0,0,0);
			if (type == 2) {
				FD_CLR(i,readfds);
			}
		}
	}
	return nfds;
}
int nanosleep(const struct timespec *req, struct timespec *rem) {
	return 0;

}

int connect(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen) {
	return helin_syscall(45,sockfd,addr,addrlen,0,0);
}
int send(int sockfd,const void *buf,int len,int flags) {
    return helin_syscall(46,sockfd,buf,len,flags,0);
} // yeah, hahaha
int recv(int sockfd,void *buf,int len,int flags) {
    return helin_syscall(47,sockfd,buf,len,flags,0);
}
pid_t vfork() {
	/*
	 * By documentation we need to do fork then if fork return isn't zero call waitpid, so we do it
	*/
	return fork();
}

int umount(char *target) {
    int ret = helin_syscall(SYS_umount,(uintptr_t)target,0,0,0,0);
    if (ret < 0) {
      errno = ret * -1;
      return -1;
    }
    return 0;
}

int reboot(int reason) {
	if (reason == RB_POWER_OFF) {
		errno = helin_syscall(14,0xfffc04,0,0,0,0);
		return -1;
	} else if (reason == RB_AUTOBOOT) {
		errno = helin_syscall(14,0,0,0,0,0);
		return -1;
	} else {
		errno = ENOSYS;
		return -1;
	}
	return 0;
}

int uname(struct utsname *name) {
	if (name == NULL) {
		errno = EFAULT;
		return -1;
	}
	return helin_syscall(50,(uintptr_t)name,0,0,0,0);
}
int execl(const char *path, const char *arg, ...) {
    // Count the number of arguments passed
    va_list args;
    va_start(args, arg);
    int num_args = 1; // Start from 1 to include the initial arg
    while (va_arg(args, const char *) != NULL) {
        num_args++;
    }
    va_end(args);

    // Create an array to hold the arguments
    char *argv[num_args + 1]; // Add 1 for the NULL terminator

    // Fill the array with the arguments
    argv[0] = (char *)arg; // Set the initial argument
    va_start(args, arg); // Restart the va_list
    for (int i = 1; i < num_args; i++) {
        argv[i] = va_arg(args, char *);
    }
    va_end(args);
    argv[num_args] = NULL; // NULL-terminate the array

    // Call execve with the constructed argument array
    return execve(path, argv, environ);
}
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum,sighandler_t handler) {
	// Because i erased this build, rewrite!
	helin_syscall(11,signum,(uintptr_t)handler,0,0,0);
}
int sigwait(const sigset_t *set,int *sig) {
	int r = helin_syscall(SYS_sigwait,(uintptr_t)set,(uintptr_t)sig,0,0,0);
	if (r < 0) {
		errno = r * -1;
		return -1;
	}
	return 0;
}
int pthread_sigmask(int how,const sigset_t *set,sigset_t *oset) {
	errno = ENOSYS;
	return -1;
}
int sigprocmask(int how,const sigset_t *set,sigset_t *oset) {
	errno - ENOSYS;
	return -1;
}
// Timer functions.
int clock_gettime(clockid_t clock_id, struct timespec *tp) {
	uint64_t time;
	errno = helin_syscall(SYS_clock,clock_id,(uintptr_t)&time,0,0,0);
	if (errno > 0) {
		return -1;
	}
	// convert this to unix timestamp
	tp->tv_sec = (time_t)time;
	tp->tv_nsec = tp->tv_sec * 1000000000;
	return 0;
}
int clock_getres(clockid_t clock_id,struct timespec *tp) {
  return clock_gettime(clock_id,tp);
}
int syscall(int num,int p1,int p2,int p3,int p4,int p5) {
	return helin_syscall(num,p1,p2,p3,p4,p5);
}

int killpg(int grID,int sig) {
	errno = 13;
	return -1;
}
int getrlimit(int resource, struct rlimit *rlp) {
	int ret = helin_syscall(SYS_getrlimit,resource,(uintptr_t)rlp,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int setrlimit(int resource, const struct rlimit *rlp) {
	int ret = helin_syscall(SYS_setrlimit,resource,(uintptr_t)rlp,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int getrusage(int resID,struct rusage *to) {
	// Make sure that the structure is clean by itself.
	memset(to,0,sizeof(struct rusage));
	int ret = helin_syscall(SYS_getrusage,resID,(uintptr_t)to,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int chroot(const char *to) {
	errno =  helin_syscall(SYS_chroot,(uintptr_t)to,0,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}

int confstr(int name,char *buff,size_t len) {
	if (name == 0) {
		strncpy(buff,getenv("PATH"),len);
		return 0;
	}
	return -1;
}

char *crypt(const char *key,const char *salt) {
	// Crypto things doesn't supported currently at kernel
	errno = 38;
	return NULL;
}

char *ctermid(char *s) {
	if (s == NULL) {
		return "/dev/tty";
	} else {
		strcpy(s,"/dev/tty");
	}
	return s;
}

char *getlogin() {
	// parse /etc/passwd.
	int passwdFd = open("/etc/passwd",O_RDONLY);
	if (passwdFd < 0) {
		return "noname";
	}
	int id = getuid();
	if (id == 0) {
		return "root";
	} else if (id == 1000) {
		return "user";
	}
	return NULL;
}
int getlogin_r(char *buff,size_t len) {
	strncpy(buff,getlogin(),len); // genius :)
	return 0;
}

char *cuserid(char *s) {
	if (s == NULL) {
		return getlogin();
	} else {
		strcpy(s,getlogin());
	}
	return s;
}

int dup3(int oldfd,int newfd,int flags) {
	return dup2(oldfd,newfd); // Linux standard, we need to implement currently only POSIX.
}

int eaccess(char *path,int mode) {
	return access(path,mode);
}

void encrypt(char *block,int flags) {}
char *getusershell(void) {
	return "/initrd/sh";
}
void endusershell() {}
void setusershell() {}
int euidaccess(const char *path,int mode) {
	return access(path,mode);
}

void execle(const char *path,const char *arg,...) {
	errno = 38;
}
void execlpe(const char *path,const char *arg,...) {
	errno = 38;
}
// Not defined in POSIX
void execvpe(const char *__file, char * const __argv[], char * const __envp[]) {
	errno = 38;
}
int faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	errno = 38;
	return -1;
}

int fchdir(int fd) {
	errno = helin_syscall(SYS_fchdir,fd,0,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int fchown(int fd,int owner,int group) {
	errno = helin_syscall(SYS_fchown,fd,owner,group,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int openat(int dirfd,const char *path,int mode,...) {
	int ret = helin_syscall(SYS_openat,dirfd,(uintptr_t)path,mode,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return ret;
}
int fchownat(int _dirfd,char *path,int mode,int group,int flags) {
	// We need to use stat(2)
	// open the file.
	int fd = openat(_dirfd,path,mode);
	if (fd < 0) {
		return -1;
	}
	errno = helin_syscall(SYS_fchown,fd,group,flags,0,0);
	close(fd);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
void fexecve(int __fd, char * const __argv[], char * const __envp[]) {
	/* Never seen this function before */
}

long fpathconf(int fd,int name) {
	errno = 38; // sorry
	return -1;
}
long pathconf(const char *path,int name) {
	errno = 38;
	return -1;
}
int fsync(int fd) {
	errno = helin_syscall(SYS_sync,fd,0,0,0,0);
	return -1;
}
int fdatasync(int fd) {
	return fsync(fd);
}
char *get_current_dir_name() {
	char path[PATH_MAX];
	getcwd(path,PATH_MAX);
	return path;
}
int getdomainname(char *name,size_t len) {
	strncpy(name,"localhost",len);
	return 0;
}
int getentropy(void *ptr,size_t len) {
	errno = 38;
	return -1;
}

int getgroups(int gidsetsize, gid_t grouplist[]) {
	// this require to process /etc/group
	errno = 38;
	return -1;
}
long gethostid() {
	return 0;
}
int sethostid(int id) {
	errno = 13;
	return -1;
}
char *getpass(char *prompt) {
	char *password = malloc(200);
	if (password == NULL) return NULL;
	printf(prompt);
	int flags = 0;
	ioctl(0,1,&flags);
	flags = flags &~ ECHO;
	ioctl(0,2,&flags);
	scanf("%s",password);
	flags = flags | ECHO;
	ioctl(0,2,&flags);
	return password;
}
int getpgrp() {
	return getgid();
}
int getpgid(int pid) {
	return helin_syscall(SYS_getpgid,0,0,0,0,0);
}

int getppid() {
	return helin_syscall(SYS_getppid,0,0,0,0,0);
}
int getsid() {
	return 0;
}
char *getwd(char *s) {
	return getcwd(s,PATH_MAX);
}
int issetugid(void) {
	return 0;
}
int lchown(const char *__path, uid_t __owner, gid_t __group) {
	// syscall :((
	int fd = open(__path,O_RDWR);
	if (fd < 0) {
		return -1;
	}
	errno = helin_syscall(SYS_chown,fd,__owner,__group,0,0);
	if (errno > 0) {
		return -1;
	}
	close(fd);
	return 0;
}
int linkat(int __dirfd1, const char *__path1, int __dirfd2, const char *__path2, int __flags) {
	errno = 38;
	return -1;
}
int nice(int priority) {
	errno = helin_syscall(SYS_nice,priority,0,0,0,0);
	return -1;
}
int lockf(int __fd, int __cmd, off_t __len) {
	errno = 38;
	return -1;
}
int pause() {
	errno = 38;
	return -1;
}
int pthread_atfork(void (*prep)(void), void (*parent)(void), void (*child)(void)) {
	prep_forkHandler = prep;
	parent_forkHandler = parent;
	child_forkHandler = child;
	return 0;
}
int pipe2(int pipefd[2],int flags) {
	errno = 38;
	return -1;
}
ssize_t pread(int fd, void *buf, size_t count,
                     off_t offset) {
	// get current offset of file
	int cur = lseek(fd,0,SEEK_CUR);
	if (cur < 0) return -1;
	if (lseek(fd,offset,SEEK_SET) < 0) return -1;
	if (read(fd,buf,count) < 0) return -1;
	if (lseek(fd,cur,SEEK_SET) < 0) return -1;
	return 0;
}
ssize_t pwrite(int fd, const void *buf, size_t count,
                     off_t offset) {
	int cur = lseek(fd,0,SEEK_CUR);
	if (cur < 0) return -1;
	if (lseek(fd,offset,SEEK_SET) < 0) return -1;
	if (write(fd,buf,count) < 0) return -1;
	if (lseek(fd,cur,SEEK_SET) < 0) return -1;
	return -1;
}
int setegid(int to) {
	return setgid(to);
}
int seteuid(int to) {
	return setuid(to);
}
int setgroups(int ngroups, const gid_t *grouplist ) {
	errno = 38;
	return -1;
}
int setpgid(int pid,int to) {
	return 0;
}
void setpgrp() {}
int setregid(gid_t __rgid, gid_t __egid) {
	errno = 38;
	return -1;
}
int setreuid(uid_t __ruid, uid_t __euid) {
	errno = 38;
	return -1;
}
pid_t tcgetpgrp(int fd) {
	errno = 38;
	return -1;
}
int tcsetpgrp(int fd, pid_t pgrp) {
	errno = 38;
	return -1;
}
char *ttyname(int fd) {
	return "tty";
}
int ttyname_r(int p1, char *p2, size_t p3) {
	errno = 38;
	return -1;
}
int vhangup() {
	errno = 38;
	return -1;
}
int _close(int fd) {
	return close(fd);
}
pid_t _fork() {
	return fork();
}
int _getpid() {
	return getpid();
}
int _isatty(int fd) {
	return isatty(fd);
}
int _link(int p1,int p2) {
	return link(p1,p2);
}
int _lseek(int __fildes, _off_t __offset, int __whence) {
	return lseek(__fildes,__offset,__whence);
}
int _read(int fd,char *buff,size_t nbyte) {
	return read(fd,buff,nbyte);
}
void *_sbrk(int how) {
	return sbrk(how);
}
int _unlink(const char *path) {
	return unlink(path);
}
int _write(int __fd, const void *__buf, size_t __nbyte ) {
	return write(__fd,__buf,__nbyte);
}
int _execve(const char *__path, char * const __argv[], char * const __envp[]) {
	return execve(__path,__argv,__envp);
}
int getdtablesize() {
	return 200;
}
int ualarm(useconds_t __useconds, useconds_t __interval) {
	errno = 38;
	return -1;
}
int setdtablesize(int p1) {
	errno = 38; // currently not possible due to STATIC define of FD.
	return -1;
}
int symlinkat(const char *p1, int p2, const char *p3) {
	//int ret = helin_syscall(SYS_symlinkat,
	return -1;
}
int	unlinkat(int dirfd, const char *path, int flags) {
  int ret = helin_syscall(SYS_unlinkat,dirfd,(uintptr_t)path,flags,0,0);
  if (ret < 0) {
    errno = ret * -1;
    return -1;
  }
  return 0;
}
void sync() {
	helin_syscall(SYS_sync,0,0,0,0,0);
}
int symlink(const char *__name1, const char *__name2) {
	errno = helin_syscall(SYS_symlink,(uintptr_t)__name1,(uintptr_t)__name2,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int posix_spawn(pid_t *restrict pid, const char *restrict path,
          const posix_spawn_file_actions_t *spawnActions,
          const posix_spawnattr_t *restrict attributes, char *const *argv,
          char *const *envp) {
  int ret = execve(path,argv,envp);
  if (ret > 0) {
    *pid = ret;
  }
  return ret;
}
// time.h
int clock_nanosleep(clockid_t clock, int flags, const struct timespec *to,
               struct timespec *remain) {
  // Currently kernel only support millisecond sleeps.
  errno = 38;
  return -1;
}
// shed.h
int sched_get_priority_max() {
  return 6;
}
int sched_get_priority_min() {
  return 1;
}
int sched_getparam(pid_t pid,struct sched_param *to) {
  return 0;
}
int sched_getscheduler(pid_t pid) {
  return 0;
}
int sched_rr_get_interval(pid_t pid,struct timespec *time) {
  errno = 38;
  return -1;
}
static speed_t io_speed = 100;
speed_t cfgetispeed(const struct termios *unsued) {
	return io_speed;
}
speed_t cfgetospeed(const struct termios *us) {
	return io_speed;
}
int     cfsetispeed(struct termios *what, speed_t sp) {
	io_speed = sp;
	return 0;
}
int     cfsetospeed(struct termios *abama, speed_t sp) {
	io_speed = sp;
	return 0;
}
// IPC, finally.
key_t ftok(const char *path,int id) {
	struct stat buf;
	if (stat(path,&buf) < 0) {
		return -1;
	}
	return (key_t)((buf.st_ino & 0xFFFF) | ((buf.st_dev & 0xFF) << 16) | ((id & 0xFF) << 24));
}
struct shm_cmd {
	int key;
	const void *mapAddr;
	int size;
	int flags;
};
int   shmget(key_t key, size_t size, int shmflags) {
	struct shm_cmd *cmd = (struct shm_cmd *)malloc(sizeof(struct shm_cmd));
	memset(cmd,0,sizeof(struct shm_cmd));
	cmd->key = key;
	cmd->size = size;
	cmd->flags = shmflags;
	int ret = syscall(SYS_ipc,1,'S',0,(uintptr_t)cmd,0);
	free(cmd);
	return ret;
}
void *shmat(int shmid, const void *to, int flags) {
	struct shm_cmd *cmd = (struct shm_cmd *)malloc(sizeof(struct shm_cmd));
	memset(cmd,0,sizeof(struct shm_cmd));
	cmd->key = shmid;
	cmd->flags = flags;
	cmd->mapAddr = to;
	int ret = syscall(SYS_ipc,2,'S',2,(uintptr_t)cmd,0);
	free(cmd);
	return (void *)(uintptr_t)(uint32_t)ret;
}
int shmdt(const void *ptr) {
	return syscall(SYS_ipc,2,'S',3,(uintptr_t)ptr,0);
}
int shmctl(int shmid,int cmd,struct shmid_ds *buf) {
	struct shm_cmd *pcmd = (struct shm_cmd *)malloc(sizeof(struct shm_cmd));
	memset(pcmd,0,sizeof(struct shm_cmd));
	pcmd->key = shmid;
	pcmd->mapAddr = buf;
	int ret = syscall(SYS_ipc,2,'S',cmd,(uintptr_t)pcmd,0);
	free(pcmd);
	return ret;
}

// Extended POSIX threads support.
int pthread_attr_getdetachstate(pthread_attr_t *atrr,int *to) {
	return -1;
}
int pthread_attr_getschedparam(const pthread_attr_t *attr,struct sched_param *to) {
	if (attr == NULL || !attr->is_initialized) {
		return -1;
	}
	*to = attr->schedparam;
	return 0;
}
int pthread_attr_getschedpolicy(const pthread_attr_t *attr,int *to) {
	if (attr == NULL | !attr->is_initialized) return -1;
	*to = attr->schedpolicy;
	return 0;
}
int pthread_attr_getscope(const pthread_attr_t *attr,int *to) {
	if (attr == NULL || !attr->is_initialized) return -1;
	*to = 0;
	return 0;
}

int pthread_attr_getstack(const pthread_attr_t *attr,void **stack,size_t *stackSize) {
	if (attr == NULL || !attr->is_initialized) return -1;
	*stack = attr->stackaddr;
	*stackSize = attr->stacksize;
	return 0;
}
int pthread_attr_getstacksize(const pthread_attr_t *attr,int *size) {
	if (attr == NULL || !attr->is_initialized) return -1;
	*size = attr->stacksize;
	return 0;
}
int pthread_attr_setdetachstate(pthread_attr_t *attr,int state) {
	if (attr == NULL || attr->is_initialized) return -1;
	attr->detachstate = state;
	return 0;
}
int pthread_attr_setguardsize(pthread_attr_t *attr,size_t quardSize) {
	errno = 38;
	return -1;
}
int pthread_attr_setschedparam(pthread_attr_t *attr,const struct sched_param *param) {
	errno = 38;
	return -1;
}
int pthread_attr_setschedpolicy(pthread_attr_t *attr,int policy) {
	if (attr == NULL || !attr->is_initialized) return -1;
	attr->schedpolicy = policy;
	return 0;
}
int pthread_attr_setscope(pthread_attr_t *attr,int scope) {
	errno = 38;
	return -1;
}
int pthread_attr_setstack(pthread_attr_t *attr,void *stack,size_t stackSize) {
	if (attr == NULL || !attr->is_initialized) return -1;
	attr->stackaddr = stack;
	attr->stacksize = stackSize;
	return 0;
}
int pthread_attr_setstacksize(pthread_attr_t *attr,int stackSize) {
	if (attr == NULL || !attr->is_initialized) return -1;
	attr->stacksize = stackSize;
	return 0;
}
// Pthread mutexes.
// Kernel currently doesn't support in-kernel mutexes. So software only.
typedef struct _software_mutex {
	int type;
	int val;
	int owner;
	int locked;
} SoftwareMutex;
typedef struct _software_cond {
	int owner;
	int signaled;
	pthread_mutex_t *mutex;
} SoftwareCond;
int pthread_mutex_init(pthread_mutex_t *mutex,
          const pthread_mutexattr_t *parameters) {
	// mutex is an int
	if (mutex == NULL) {
		errno = EINVAL;
		return -1;
	}
	SoftwareMutex *mtx = malloc(sizeof(SoftwareMutex));
	memset(mtx,0,sizeof(SoftwareMutex));
	if (mutex != NULL && *mutex != 0) {
		mtx->type = *mutex;
	}
	*mutex = (int)mtx;
	return 0;
}
pthread_t pthread_self(void);
int pthread_mutex_lock(pthread_mutex_t *mutex) {
	SoftwareMutex *mtx = (SoftwareMutex *)mutex;
	if (mtx == NULL) {
		errno = EINVAL;
		return -1;
	}
	switch(mtx->type) {
		case 0:
		case _PTHREAD_MUTEX_INITIALIZER: {
			if (mtx->locked) {
				while(mtx->locked) {
					usleep(500);
				}
				mtx->locked = 1;
			}
		} break;
		case 2:
			if (mtx->locked) {
				errno = EDEADLK;
				return -1;
			}
			mtx->locked = 1;
			break;
		case 4:
			mtx->val++;
			break;
	}
	mtx->owner = pthread_self();
	return 0;
}
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	SoftwareMutex *mtx = (SoftwareMutex *)mutex;
	if (mtx == NULL) {
		errno = EINVAL;
		return -1;
	}
	switch(mtx->type) {
		case 0:
		case _PTHREAD_MUTEX_INITIALIZER:
			mtx->locked = 0;
			break;
		case 2:
			// Check if the thread owner is the same as the caller.
			if (mtx->owner != pthread_self())  {
				return;
			}
			break;
	}
	return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *mt) {
	free(mt);
	return 0;
}
int pthread_detach(pthread_t pth) {
	return 0;
}
int pthread_cancel(pthread_t pth) {
	// pthread_t in HelinKern is actually the process id.
	kill(pth,SIGKILL);
}
int pthread_cond_init(pthread_cond_t *cond,const pthread_condattr_t *attrs) {
	SoftwareCond *cnd = malloc(sizeof(SoftwareCond));
	memset(cnd,0,sizeof(SoftwareCond));
	cnd->mutex = malloc(sizeof(int *));
	cnd->owner = pthread_self();
	pthread_mutex_init(cnd->mutex,NULL);
	*cond = (pthread_cond_t *)cnd;
	return 0;
}
int pthread_cond_broadcast(pthread_cond_t *cond) {
	return 0;
}
int pthread_cond_destroy(pthread_cond_t *cnd) {
	// Destroy mutex.
	SoftwareCond *cond = (SoftwareCond *)cnd;
	pthread_mutex_destroy(cond->mutex);
	free(cond);
	return 0;
}
int pthread_cond_signal(pthread_cond_t *cnd) {
	return 0;
}
int pthread_cond_timedwait(pthread_cond_t *cnd,pthread_mutex_t *mtx,const struct timespec *time) {
	return 0;
}
int pthread_cond_wait(pthread_cond_t *cnd,pthread_mutex_t *mtx) {
	return 0;
}
// message queue!
typedef struct _usrMsg {
	int key; // unique key.
	void *buffer; // data buffer.
	size_t reqSize;
} UserMsgObj;
int msgget(key_t key,int flags) {
	UserMsgObj *obj = malloc(sizeof(UserMsgObj));
	memset(obj,0,sizeof(UserMsgObj));
	obj->key = key;
	int ret = helin_syscall(SYS_ipc,1,'m',0,(uintptr_t)obj,0);
	free(obj);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return ret;
}
int       msgctl(int msgQueueID, int cmd, struct msqid_ds *fromOrTo) {
	UserMsgObj *obj = malloc(sizeof(UserMsgObj));
        memset(obj,0,sizeof(UserMsgObj));
	obj->key = msgQueueID;
	int ret = helin_syscall(SYS_ipc,2,'m',3,(uintptr_t)obj,0);
	free(obj);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return ret;
}
ssize_t   msgrcv(int msgQueueID, void *buffer, size_t size, long msgType, int msgFlags) {
	UserMsgObj *obj = malloc(sizeof(UserMsgObj));
        memset(obj,0,sizeof(UserMsgObj));
        obj->key = msgQueueID;
	obj->buffer = buffer;
	obj->reqSize = size;
        int ret = helin_syscall(SYS_ipc,2,'m',2,(uintptr_t)obj,0);
        free(obj);
        if (ret < 0) {
                errno = ret * -1;
                return -1;
        }
        return ret;
}
int       msgsnd(int msgQueueID, const void *buffer, size_t size, int msgFlags) {
	UserMsgObj *obj = malloc(sizeof(UserMsgObj));
        memset(obj,0,sizeof(UserMsgObj));
        obj->key = msgQueueID;
        obj->buffer = buffer;
        obj->reqSize = size;
        int ret = helin_syscall(SYS_ipc,2,'m',1,(uintptr_t)obj,0);
        free(obj);
        if (ret < 0) {
                errno = ret * -1;
                return -1;
        }
        return ret;
}
// sys/stat.h extension.
int fchmodat(int _dirfd,const char *name,mode_t mode,int flags) {
	int ret = helin_syscall(SYS_fchmodat,_dirfd,(uintptr_t)name,mode,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
int mkdirat(int dirfd,const char *name,mode_t mode) {
	int ret = helin_syscall(SYS_mkdirat,dirfd,(uintptr_t)name,mode,0,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
int fstatat(int dirfd,const char *name,struct stat *buff,int flags) {
	int fd = openat(dirfd,name,O_RDONLY);
	if (fd < 0) {
		return -1;
	}
	int ret = fstat(fd,buff);
	close(fd);
	return ret;
}
int mknod(const char *name,mode_t mode,dev_t dev) {
	int res = helin_syscall(SYS_mknod,(uintptr_t)name,mode,dev,0,0);
	if (res < 0) {
		errno = res * -1;
		return -1;
	}
	return 0;
}
int mknodat(int dirfd,const char *name,mode_t mode,dev_t dev) {
	int ret = helin_syscall(SYS_mknodat,dirfd,(uintptr_t)name,mode,dev,0);
	if (ret < 0) {
		errno = ret * -1;
		return -1;
	}
	return 0;
}
int mkfifo(const char *path,mode_t mode) {
	return 0;
}
int mkfifoat(int dirfd,const char *path,mode_t mode) {
	return 0;
}
void helin_syscalls_init() {
	prep_forkHandler = NULL;
	child_forkHandler = NULL;
	parent_forkHandler = NULL;
	krnStat = malloc(sizeof(struct kernelStat));
	memset(krnStat,0,sizeof(struct kernelStat));
}
pid_t wait3(int wstatus, int options,
                   struct rusage rusage) {
	return waitpid(-1,wstatus,options);
}
int sigsuspend(const sigset_t *mask) {
	return 0;
}
// Realtime.
/*int posix_memalign(void **memptr,size_t align,size_t size) {
	if (memptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*memptr = memalign(align,size);
	if (*memptr == NULL) {
		return -1;
	}
	return 0;
}*/
pthread_t pthread_self(void) {
	return getpid();
}
int pthread_kill(pthread_t th,int sig) {
        return kill(th,sig);
}
int sigaltstack(const stack_t *ss,stack_t *old_ss) {
        int r = helin_syscall(SYS_signalstack,(uintptr_t)ss,(uintptr_t)old_ss,0,0,0);
        if (r > 0) {
                errno = r;
                return -1;
        }
        return 0;
}
