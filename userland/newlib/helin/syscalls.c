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
#define PATH_MAX 40
// Structure from src/thread.c from kernel source
typedef struct _pthread_str {
        int entry;
        void *arg;
} pthread_str;

static int globalMask = 0;
// TODO: Need to be rewritten as arch-specific code.
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
    int num_args = 0;

    // Count the number of arguments until we reach a NULL pointer in the argv array
    while (argv[num_args] != NULL) {
        num_args++;
    }

    int pid = helin_syscall(13, (int)name, num_args, (int)argv, (int)env, 0);
    if (pid < 0) return -1;
    return 0;
}
int fork() {
    return helin_syscall(49,0,0,0,0,0);
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
int fstat(int file, struct stat *st) {
    struct kernelStat krnStat;
    errno = helin_syscall(SYS_stat,file,(int)&krnStat,0,0,0);
    if (errno > 0) {
	return -1;
    }
    convertKrnStat(&krnStat,st);
    return 0;
}
int getpid(){
    return helin_syscall(4,0,0,0,0,0);
}
int getuid() {
	return helin_syscall(SYS_getuid,0,0,0,0,0);
}
int isatty(int file){
    return false;
}
int kill(int pid, int sig){
    helin_syscall(3,pid,sig,0,0,0);
    return 0;
}
int link(char *old, char *new){
	errno = helin_syscall(SYS_link,(int)old,(int)new,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int lseek(int file, int ptr, int dir){
    return helin_syscall(27,file,dir,ptr,0,0);
}
int open(const char *name, int flags, ...){
    return helin_syscall(7,(int)name,flags,0,0,0);
}
int read(int file, char *ptr, int len){
    return helin_syscall(9,file,0,len,(int)ptr,0);
}
caddr_t sbrk(int incr){
    return (caddr_t)helin_syscall(35,incr,0,0,0,0);
}
int stat(const char *file, struct stat *st){
	int fd = open(file,O_RDONLY);
	if (fd < 0) {
		return -1;
	}
	struct kernelStat krnStat;
	int ret = helin_syscall(SYS_stat,fd,(int)&krnStat,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	convertKrnStat(&krnStat,st);
	close(fd);
	return 0;
}
clock_t times(struct tms *buf){}
int unlink(char *name){
	errno = helin_syscall(SYS_unlink,(int)name,0,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}
int wait(int *status){
  return waitpid(-1,&status,0);
}
int write(int file, char *ptr, int len){
    int how = helin_syscall(10,file,0,len,(int)ptr,0);
    return how;
}
int gettimeofday(struct timeval *__restrict __p,
                          void *__restrict __tz){}
int ioctl(int fd,unsigned long request,...) {
    va_list args;
    va_start(args,request);
    int ret = helin_syscall(32,fd,request,(int)args,0,0);
    va_end(args);
    return ret;
}
void *mmap(void *addr,size_t len,int prot,int flags,int fd,off_t offset) {
    return (void *)helin_syscall(29,fd,(int)addr,len,offset,flags);
}
DIR *opendir(const char *path) {
    int fd = helin_syscall(18,(int)path,0,0,0,0);
    if (fd < 0) {
        return NULL;
    }
    DIR *ret = (DIR *)malloc(sizeof(DIR));
    memset(ret,0,sizeof(DIR));
    ret->_fd = fd;
    ret->_pos = 0;
    ret->pointer = (struct dirent *)malloc(sizeof(struct dirent));
    memset(ret->pointer,0,sizeof(struct dirent));
    return ret;
}

struct dirent *readdir(DIR *d) {
    if (!d) return NULL;
    // get platform specific dirent then convert it to system
    struct _helin_dirent *native = (struct _helin_dirent *)helin_syscall(20,d->_fd,d->_pos,0,0,0);
    if (native == NULL) {
        // End of directory
        return NULL;
    }
    // Increment the file position
    d->_pos++;
    struct dirent *dire = (struct dirent *)d->pointer;
    dire->d_name = native->name;
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
    close(dir->_fd);
    free(dir);
    return 0;
}
int chdir (const char *path) {
    return helin_syscall(17,(int)path,0,0,0,0);
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
    return (char *)helin_syscall(16,(int)__buf,__size,0,0,0);
}

int mount(const char *source,const char *target,
              const char *filesystemtype,unsigned long mountflags,
              const void *data) {
    int ret = helin_syscall(21,(int)source,(int)target,(int)filesystemtype,mountflags,(int)data);
    if (ret == -1) {
    	errno = ENOENT;
    } else if (ret == -2) {
    	errno = ENODEV;
    } else if (ret == -3) {
    	errno = EINVAL;
    }
    if (ret < 0) {
    	return -1;
    }
    return ret;
}
int getfsstat(struct statfs *buf,long bufsize,int mode) {
	int ret = helin_syscall(SYS_getfsstat,(int)buf,(int)bufsize,mode,0,0);
	return ret;
}
// waitpid
pid_t waitpid(pid_t pid,int *status,int options) {
    helin_syscall(22,pid,0,0,0,0);
}
// Need to port dash shell to our OS
int dup(int oldfd) {
	return helin_syscall(36,oldfd,0,0,0,0);
}
int dup2(int oldfd, int newfd) {
	return helin_syscall(36,oldfd,newfd,0,0,0);
}
int execvp(const char *file, char *const argv[]) {
	if (file == NULL || argv == NULL) return -1;
	return execve(file,argv,environ);
}
int execv(const char *path, char *const argv[]) {
	return execvp(path,argv);
}
int pipe(int pipefd[2]) {
  // Use IPC manager API.
  return helin_syscall(SYS_ipc,1,0x50,0,(int)pipefd,0);
}
void thread_entry(pthread_str *data) {
	// stderr,stdout,stdin are automatically is openned for us
	//printf("thread_entry: Received entry address: 0x%x, arg: 0x%x\n",data->entry,data->arg);
	void (*entry)(void *) = ((void (*)(void *))data->entry);
	entry(data->arg);
        _exit(0);
}
// Pthread
int pthread_equal(pthread_t t1, pthread_t t2) {
	return t1 == t2;
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
	int pid = helin_syscall(37,(int)thread_entry,(int)start_routine,(int)arg,0,0);
	if (pid > 0) {
		*thread = pid;
	}
	// Wait for start via sys_waitForStart
	helin_syscall(38,pid,0,0,0,0);
	return 0;
}

void pthread_exit(void* retval) {
	// Just exit
	exit(0);
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
	ioctl(fd,1,(int)tio);
	return 0;
}
int     tcsetattr(int fd, int type, const struct termios *term) {
	if (type != 0) {
		errno = ENOSYS;
		return -1;
	}
	ioctl(fd,2,(int)term);
	return 0;
}

int access(const char *pathname,int mode) {
	return helin_syscall(SYS_access,(int)pathname,mode,0,0,0);
}


void pthread_cleanup_push(void (*routine)(void *),
                                 void *arg) {}
void pthread_cleanup_pop(int execute) {}

int pthread_attr_init(pthread_attr_t *attr) {return 0;}
int pthread_attr_destroy(pthread_attr_t *attr) {return 0;}

int fcntl(int fd, int cmd, ...) {
	return 0; // We don't support :(
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
	int fd = open(pathname,O_RDWR);
	if (fd < 0) {
		errno = ENOENT;
		return -1;
	}
	errno = helin_syscall(SYS_chmod,fd,mode,0,0,0);
	if (errno > 0) {
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
	return helin_syscall(SYS_chown,2,(int)pathname,owner,group,0);
}

int rmdir(const char *pathname) {
	return helin_syscall(SYS_rm,2,(int)pathname,0,0,0);
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
	return 0;
}

int gethostname(char *name, size_t len) {
	if (name == NULL || len == 0) return -1;
	return helin_syscall(52,(int)name,len,0,0,0);
}

int sethostname(char *name,size_t len) {
	return helin_syscall(51,(int)name,len,0,0,0);
}

uid_t geteuid(void) {
	return getuid();
}

uint16_t htons(uint16_t hostshort) {return hostshort;}

uint32_t ntohl(uint32_t netlong) {return netlong;}

uint16_t ntohs(uint16_t netshort) { return netshort;}
int pclose(FILE *stream) {
	return 0;
}
static struct passwd empty;
static struct passwd *passwdRet;
struct passwd *getpwuid(uid_t uid) {
  // UNIX file structure :)
  FILE *passwd = fopen("/etc/passwd","r");
  if (!passwd) {
    errno = ENOENT;
    return errno;
  }
  // Parse passwd.
  if (passwdRet == NULL) {
    passwdRet = malloc(sizeof(struct passwd));
    memset(passwdRet,0,sizeof(struct passwd));
  }
  char internalBuffer[100];
  while(fgets(internalBuffer,100,passwd)) {
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
    if (passwdRet->pw_uid == uid) {
      fclose(passwd);
      return passwdRet;
    }
  }
  fclose(passwd);
  return NULL;
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
	errno = helin_syscall(SYS_chmod,fd,mode,0,0,0);
	if (errno > 0) {
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

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {return 0;}
//struct group g_empt;
struct group *getgrnam(const char *name) {
  FILE *groupFile = fopen("/etc/group","r");
  if (!groupFile) {
    return NULL;
  }
	return NULL;
}
struct passwd *getpwnam(const char *name) {
	return NULL;
}
struct group *getgrgid(gid_t gid) {return NULL;}
pid_t setsid(void) {
	return -1;
}
int mkdir(const char *path, mode_t mode) {
	return helin_syscall(53,(int)path,mode,0,0,0);
}
int creat(const char *path, mode_t mode) {
	int ret = helin_syscall(SYS_creat,(int)path,mode,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
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
	errno = helin_syscall(SYS_munmap,(int)addr,len,0,0,0);
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
    return helin_syscall(56,(int)target,0,0,0,0);
}

int reboot(int reason) {
	if (reason == RB_POWER_OFF) {
		errno = helin_syscall(14,0xfffc04,0,0,0,0);
		return -1;
	} else if (reason == RB_AUTOBOOT) {
		errno = helin_syscall(14,0,0,0,0,0);
		return -1;
	} else {
		errno = -ENOSYS;
		return -1;
	}
	return 0;
}

int uname(struct utsname *name) {
	if (name == NULL) {
		errno = EFAULT;
		return -1;
	}
	return helin_syscall(50,(int)name,0,0,0,0);
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
	helin_syscall(11,signum,(int)handler,0,0,0);
}
// Timer functions.
int clock_gettime(clockid_t clock_id, struct timespec *tp) {
	struct tm time;
	errno = helin_syscall(SYS_gettime,clock_id,(int)&time,0,0,0);
	if (errno > 0) {
		return -1;
	}
	// convert this to unix timestamp
	tp->tv_sec = mktime(&time);
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
	int ret = helin_syscall(SYS_getrlimit,resource,(int)rlp,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}
int setrlimit(int resource, const struct rlimit *rlp) {
	int ret = helin_syscall(SYS_setrlimit,resource,(int)rlp,0,0,0);
	if (ret > 0) {
		errno = ret;
		return -1;
	}
	return 0;
}

int chroot(const char *to) {
	errno =  helin_syscall(SYS_chroot,(int)to,0,0,0,0);
	if (errno > 0) {
		return -1;
	}
	return 0;
}

int confstr(int name,char *buff,size_t len) {
	if (name == 0) {
		// All utilities can be found at /initrd/(currently,alpha build)
		strncpy(buff,"/initrd",len);
		return 0;
	}
	errno = 38;
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

int fchownat(int _dirfd,char *path,int mode,int group,int flags) {
	// We need to use stat(2)
	errno = 38;
	return -1;
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
int pthread_atfork(void (*p1)(void), void (*p2)(void), void (*p3)(void)) {
	errno = 38;
	return -1;
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
	return setgid(to);
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
	errno = 38;
	return -1;
}
int	unlinkat(int p1, const char *p2, int p3) {
	errno = 38;
	return -1;
}
void sync() {
	helin_syscall(SYS_sync,0,0,0,0,0);
}
int symlink(const char *__name1, const char *__name2) {
	errno = helin_syscall(SYS_symlink,(int)__name1,(int)__name2,0,0,0);
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

