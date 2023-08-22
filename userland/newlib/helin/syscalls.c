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
// Structure from src/thread.c from kernel source
typedef struct _pthread_str {
        int entry;
        void *arg;
} pthread_str;

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

    int pid = helin_syscall(13, (int)name, num_args, (int)argv, 0, 0);
    if (pid < 0) return -1;
    return 0;
}
int fork() {
    return helin_syscall(49,0,0,0,0,0);
}
int fstat(int file, struct stat *st) {
    return -1;
}
int getpid(){
    return helin_syscall(4,0,0,0,0,0);
}
int getuid() {
	return 0; // HelinKern does have sys_getuid, but i don't remember actuall syscall number for it :(
	// FIXME: Call actual sys_getuid from kernel
}
int isatty(int file){
    return false;
}
int kill(int pid, int sig){
    helin_syscall(3,pid,0,0,0,0);
    return 0;
}
int link(char *old, char *new){}
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
int stat(const char *file, struct stat *st){}
clock_t times(struct tms *buf){}
int unlink(char *name){}
int wait(int *status){}
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
    helin_syscall(26,0,0,0,0,0);
}
char *getcwd (char *__buf, size_t __size) {
    return (char *)helin_syscall(16,(int)__buf,__size,0,0,0);
}

int mount(const char *source,const char *target,
              const char *filesystemtype,unsigned long mountflags,
              const void *data) {
    return helin_syscall(21,(int)source,(int)target,(int)filesystemtype,mountflags,(int)data);
}

// waitpid
pid_t waitpid(pid_t pid,int *status,int options) {
    if (pid <= 0) {
        return -1; // doesn't supported currently
    }
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
	return execve(file,argv,NULL);
}
int execv(const char *path, char *const argv[]) {
	return execvp(path,argv);
}
int pipe(int pipefd[2]) {
	return -1; // doesn't supported
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
	return 0;
}
int tcsetattr(int fd,struct termios* tio) {
	return 0;
}

int access(const char *pathname,int mode) {
	return 0;
}

int clock_gettime (clockid_t clock_id, struct timespec *tp) {
	if (tp == NULL) return 1;
	return 0;
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
	return 0;
}

mode_t umask(mode_t mask) {
	return 0;
}

int chmod(const char *pathname, mode_t mode) {
	return 0;
}

long sysconf(int name) {
	return 0;
}

int utime(const char *filename, const struct utimbuf *times) {
	return 0;
}

int chown(const char *pathname, uid_t owner, gid_t group) {
	return 0;
}

int rmdir(const char *pathname) {
	return 0;
}


extern struct mntent *getmntent (FILE *__stream) {
	return NULL;
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
	return 0;
}

int sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact) {
	return 0;
}

int gethostname(char *name, size_t len) {
	if (name == NULL || len == 0) return -1;
	snprintf(name,len,"Helin");
	return 0;
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
struct passwd *getpwuid(uid_t uid) {
	return &empty;
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
int daemon(int nochdir, int noclose) {
	return 0;
}

int fchmod(int fd, mode_t mode) {
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
	if (name == NULL) return NULL;
	return NULL;
}
struct passwd *getpwnam(const char *name) {
	return &empty;
}
struct group *getgrgid(gid_t gid) {return NULL;}
pid_t setsid(void) {
	return -1;
}
int mkdir(const char *path, mode_t mode) {
	return 0;
}
int creat(const char *path, mode_t mode) {
	return open(path,7);
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
	int pid = fork();
	if (pid != 0) {
		waitpid(pid,NULL,0);
	}
	return pid;
}
