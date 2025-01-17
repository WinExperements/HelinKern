// Shell program that use newlib
/*
* Temple Shell program for HelinOS and other systems that hasn't have full POSIX support
* to port more modern shell programs.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h> // poweroff and reboot shell commands.
#include <sys/mount.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <pwd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>
char path[100];
char oldpath[100];

void parse(int argc,char **argv);
extern char **environ;
int main(int pargc,char **pargv) {
	if (pargc > 1) {
		if (!strcmp(pargv[1],"-c")) {
			// Execute command.
			char *cmdArgv[100];
			int cmdArgc = 0;
			cmdArgv[cmdArgc] = strtok(pargv[2]," ");
			while(cmdArgv[cmdArgc]) {
				cmdArgc++;
				cmdArgv[cmdArgc] = strtok(NULL," ");
			}
			if (cmdArgc > 0) {
				parse(cmdArgc,cmdArgv);
			}
			return 1;
		}
	}
    char line[100];
    bool exit = false;
    int argc = 0;
    char *argv[100];
    int uid = getuid();
    struct passwd *crUser = getpwuid(uid);
    char hostname[100];
    gethostname(hostname,100);
    char *usrName = strdup(crUser->pw_name); // Somehow printf overwrites the pw_name
    char usC = '$';
    if (getuid() == 0) {
      usC = '#';
    }
    while(!exit) {
        //putc('>',stdout);
        getcwd(path,100);
        printf("%s@%s:%s%c ",usrName,hostname,path,usC);
        fflush(stdout);
        fgets(line,100,stdin);
        line[strcspn(line,"\n")] = 0;
	if (line[0] == '#') continue;
        if (!strcmp(line,"exit")) {
            exit = true;
            break;
        }
        argv[argc] = strtok(line," ");
        while(argv[argc]) {
            argc++;
            argv[argc] = strtok(NULL," ");
        }
        if (argc > 0) {
            parse(argc,argv);
            argc = 0;
        }
    }
   // printf("argv[0]: %s, 0x%x",environ[0],environ[0]);
    return 0;
}
struct prcInfo {
	char name[20];
	int pid;
	int status;
  uintptr_t mem;
};
bool executeCommand(int argc,char **argv,bool wait) {
    // Try to find the file in argv[0]
    char execPath[100];
    // Format the string with given exec pathes.
    char *foundedPath = NULL;
    if (argv[0][0] == '/') {
	    strncpy(execPath,argv[0],100);
	    foundedPath = argv[0];
    } else {
      // We need to find executable somewhere in the PATH environment variable.
      char *where = strtok(strdup(getenv("PATH")),":");
      while(where) {
    	  snprintf(execPath,100,"/%s/%s",where,argv[0]);
        int f = 0;
        if ((f = open(execPath,O_RDONLY)) > 0) {
          foundedPath = where;
          break;
        }
        where = strtok(NULL,":");
      }
      free(where);
    }
    if (foundedPath == NULL) {
      return false;
    }
    // Fork-exec model on UNIX systems.
    int child = fork();
    if (child == 0) {
	    int origFD = fileno(stdout);
	    int inFD = fileno(stdin);
	    int errFD = fileno(stderr);
	    for (int i = 0; i < argc; i++) {
		    if (argv[i][0] == '>') {
			    int newFd = open(argv[i+1],O_RDWR | O_CREAT);
			    if (newFd < 0) {
				    perror(argv[0]);
				    exit(1);
				} else {
					dup2(newFd,0);
					dup2(newFd,1);
					dup2(newFd,2);
				}
			    argv[i] = NULL;
			    break;
			}
		}
	    // Execv
	    execv(execPath,argv);
	    perror(argv[0]);
	    exit(1);
	} else {
		if (wait) {
                        int r = 0;
                        waitpid(child,&r,0);
                        // Check if child killed with abonormal return value.
                        if (WIFSIGNALED(r)) {
                                int sig = WTERMSIG(r);
                                psignal(sig,"");
                                fflush(stdout);
                        }
                }
		return 1;
	}
    return 0;
}

void parse(int argc,char **argv) {
    if (!strcmp(argv[0],"ls")) {
	DIR *d = NULL;
	if (argc > 1) {
		// try to open the specified path.
		d = opendir(argv[1]);
	} else {
		d = opendir(path);
	}
        if (!d) {
            printf("Failed to open %s",(argc > 1 ? argv[1] : path));
	    fflush(stdout);
	    perror(" ");
            return;
        }
        struct dirent *dir = readdir(d);
        while(dir) {
            printf("%s ",dir->d_name);
            fflush(stdout);
            dir = readdir(d);
        }
        closedir(d);
        printf("\n");
    } else if (!strcmp(argv[0],"cd")) {
	    // Sanity check.
	    if (argc < 2) return;
	    if (!strcmp(argv[1],"..")) {
		    if (chdir(oldpath) < 0) {
			    perror("chdir: Failed to switch to old pwd: ");
			    return;
			}
		    return;
	    } else if (argv[1][0] == '.') {
		    return;
	    }
	    // HelinOS currently doesn't support stat.
	    // TOOD: Check if given path is folder.
	    if (chdir(argv[1]) < 0) {
		    perror("chdir fail: ");
		    return;
		}
	    strcpy(oldpath,path);
    } else if (!strcmp(argv[0],"pwd")) {
	    printf("%s\r\n",path);
    } else if (!strcmp(argv[0],"help")) {
	    printf("HelinOS newlib shell version 0.1\r\n");
	    printf("Supported built-in commands: ls,cd,pwd,help,poweroff,reboot,mount. All other commands will be tried to be runned from /initrd/<command name>\r\n");
    } else if (!strcmp(argv[0],"poweroff")) {
      kill(1,SIGINT);
    } else if (!strcmp(argv[0],"reboot")) {
	    if (reboot(RB_AUTOBOOT) < 0) {
		perror("Operation failed");
	    }
    } else if (!strcmp(argv[0],"mount")) {
	    if (argc < 3) {
		    // BSD style!
#ifdef __helin__
		    int how = getfsstat(NULL,0,0);
		    struct statfs *ar = malloc(sizeof(struct statfs) * how);
		    memset(ar,0,sizeof(struct statfs) * how);
		    if (getfsstat(ar,sizeof(struct statfs) * how,0) < 0) {
			    perror("Failed to get list of mounted file systems from kernel");
			    return;
		    }
		    for (int i = 0; i < how; i++) {
			    printf("%s on %s (%s)\r\n",ar[i].f_mntfromname,ar[i].f_mnttoname,ar[i].f_fstypename);
		    }
		    free(ar);
#endif
		} else {
			if (mount(argv[1],argv[2],argv[3],0,NULL) < 0) {
				perror("Mount failed. Check kernel logs based on your current system");
				return;
			}
		}
    } else if (!strcmp(argv[0],"uname")) {
	    struct utsname osname;
	    uname(&osname);
	    printf("%s %s %s %s\r\n",osname.sysname,osname.machine,osname.version,osname.release);
#ifdef __helin__
	    printf("Number of implemented in kernel syscalls: %d\r\n",NUM_SYSCALLS);
#endif
    } else if (!strcmp(argv[0],"id")) {
	printf("%d\r\n",getuid());
    } else if (!strcmp(argv[0],"insmod")) {
	    if (argc < 2) {
		    printf("insmod <mod path>\r\n");
		    return;
	    }
	    if (syscall(SYS_insmod,(int)argv[1],0,0,0,0) < 0) {
		    perror("insmod fail");
	    }
    } else if (!strcmp(argv[0],"env")) {
      int a = 0;
      while(environ[a]) {
        printf("%s\r\n",environ[a]);
        a++;
      }
    } else if (!strcmp(argv[0],"dmesg")) {
      int ringBuffSize = syscall(SYS_syslog,0,0,0,0,0);
      char *buff = malloc(ringBuffSize);
      syscall(SYS_syslog,0,(int)buff,ringBuffSize,0,0);
      buff[ringBuffSize] = 0;
      printf(buff);
    } else if (!strcmp(argv[0],"ps")) {
	    int psList = syscall(SYS_ipc,2,'P',0,0,0);
	    struct prcInfo *lst = malloc(sizeof(struct prcInfo) * psList);
	    memset(lst,0,sizeof(struct prcInfo) * psList);
	    if (syscall(SYS_ipc,2,'P',1,(int)lst,0) < 0) {
		    perror("Retrive fail");
		}
	    for (int i = 0; i < psList; i++) {
		    char *state = "running";
		    switch(lst[i].status) {
			    case 1:
				    break;
			    case 2:
				    state = "sleeping";
				    break;
			    case 3:
				    state = "waiting";
				    break;
			   case 4:
				    state = "zombie";
				    break;
			   case 5:
				    state = "waitpid";
				    break;
			   default:
				    state = "unknown";
				    break;
		    }
		    printf("%d tty0 mem:%dKiB %s %s\r\n",lst[i].pid,lst[i].mem/1024,state,lst[i].name);
		}
	    free(lst);
    } else if (!strcmp(argv[0],"krndbg")) {
	    // trigger kernel debug.
	    syscall(SYS_ipc,2,'P',2,0,0);
    } else if (!strcmp(argv[0],"clear")) {
	    ioctl(0,3);
    } else if (!strcmp(argv[0],"nanox")) {
	    // Start some nano-X examples!
	    int test = open("/bin/nanox",O_RDONLY);
	    if (test < 0) {
		    perror("Mount please ISO drive, error");
		    return;
	    }
	    close(test);
	    test = open("/bin/landmine",O_RDONLY);
	    if (test < 0) {
		    perror("Landmine failed to find in CD-DRIVE :(\r\n");
		    return;
		}
	    char *ag[] = {"/bin/nanox","-p",NULL};
	    executeCommand(1,ag,false);
	    sleep(2);
	    char *og[] = {"/bin/landmine",NULL};
	    executeCommand(1,og,false);
    } else if (!strcmp(argv[0],"kill")) {
	    if (argc < 2) {
		    printf("kill <pid> <sig>, if <sig> not specified, sigkill used\r\n");
		    return;
		}
	    int pid = atoi(argv[1]);
	    int sig = 9;
	    if (argc > 2) {
		    sig = atoi(argv[2]);
		}
	    if (kill(pid,sig) < 0) {
        perror("kill");
      }
    } else if (!strcmp(argv[0],"free")) {
	    uintptr_t used = syscall(75,1,0,0,0,0);
	    uintptr_t kheap_phys = syscall(SYS_sysconf,2,0,0,0,0);
	    uintptr_t kheap_usage = syscall(SYS_sysconf,3,0,0,0,0);
	    uintptr_t reservedSize = syscall(SYS_sysconf,4,0,0,0,0);
            uintptr_t nextAlloc = syscall(SYS_sysconf,5,0,0,0,0);
            unsigned long total = syscall(SYS_sysconf,6,0,0,0,0);
	    printf("Used physical memory(KiB): %d(%d MiB)/%d MiB\r\n",used/1024,used/1024/1024,total/1024/1024);
	    printf("Used physical memory for the kernel heap: %d KiB(%d MiB)\n",(kheap_phys*4096)/1024,(kheap_phys*4096)/1024/1024);
	    printf("Used kernel heap memory %d KiB\n",kheap_usage/1024);
	    printf("Reserved size: %d KiB(%d MiB)\n",reservedSize/1024,reservedSize/1024/1024);
      printf("Next unallocated align: %dMiB\n",nextAlloc/1024/1024);
    } else if (!strcmp(argv[0],"touch")) {
	    if (argc < 2) {
		    printf("touch <filename>\n");
		    return;
		}
	    if (creat(argv[1],0) < 0) {
		    perror("Failed");
		}
    } else if (!strcmp(argv[0],"cp")) {
	    if (argc < 3) {
		    printf("cp: <source file> <destination file>\n");
		    return;
		}
	    int sourceFd = open(argv[1],O_RDONLY);
	    if (sourceFd < 0) {
		    perror("Failed to open source file");
		    return;
		}
	    int destFd = open(argv[2],O_RDWR);
	    if (destFd < 0) {
		    perror("Failed to open destination");
		    close(sourceFd);
		    return;
		}
	    // Retrive file size;
	    struct stat st;
	    if (fstat(sourceFd,&st)) {
		    perror("Failed to retrive file information");
		    close(sourceFd);
		    close(destFd);
		    return;
		}
	    // we copy data using 1K blocks(very very long)
	    int blksize = 4096;
	    void *block = malloc(blksize);
	    int blocks = st.st_size / blksize;
	    int i = 0;
            int written = 0;
	    for (int b = 0; b < blocks; b++) {
		    int ret = read(sourceFd,block,blksize);
		    if (ret <= 0) break;
		    printf("writing block: %d of %d\n",i,blocks);
		    if (write(destFd,block,blksize) <= 0) {
			    perror("Write-back error");
			    break;
			}
		    i++;
                    written += 4096;
		}
            if (written < st.st_size) {
                    int rem = st.st_size - written;
                    read(sourceFd,block,rem);
                    write(destFd,block,rem);
                    printf("remaining %d bytes written successfully?\n",rem);
            }
	    // Check blocks.
	    lseek(sourceFd,0,SEEK_SET);
	    lseek(destFd,0,SEEK_SET);
	    close(sourceFd);
	    close(destFd);
	    free(block);
    } else  if (!strcmp(argv[0],"swm")) {
	    if (fork() == 0) {
		    // we can start wm.
		    printf("Starting WM\n");
		    char *wm_argv[] = {"wm",NULL};
		    execv("/mnt/wm",wm_argv);
		    printf("Failure for wm to be online\n");
		    exit(1);
		}
	    printf("Sleeping to take wm some time to initialize itself\n");
	    sleep(4);
	    printf("Okay. starting first client\n");
	    if (fork() == 0) {
		    char *helloworld_argv[] = {"hl",NULL};
		    execv("/mnt/hal",helloworld_argv);
		    printf("Start failure\n");
		    exit(1);
		}
    } else if (!strcmp(argv[0],"stat")) {
	    if (argc < 2) {
		    printf("stat: enter filename to print statistic about.\n");
		    return;
		}
	    struct stat statBuff;
	    if (stat(argv[1],&statBuff) < 0) {
		    perror("stat");
		    return;
		}

	    printf("Inode ID: %d\r\n \
			    Mode: %d\r\n \
			    UID: %d\r\n \
			    GID: %d\r\n \
			    Creation time: %s\r\n \
			    Last modification time: %s\r\n \
			    Last access time: %s\r\n",
			    statBuff.st_ino,
			    statBuff.st_mode,
			    statBuff.st_uid,
			    statBuff.st_gid,
			    ctime(&statBuff.st_ctime),
			    ctime(&statBuff.st_mtime),
			    ctime(&statBuff.st_atime));
    } else if (!strcmp(argv[0],"date")) {
	    time_t ti;
	    if (time(&ti) < 0) {
		    perror("time");
		    return;
		}
	    printf("System time: %s\n",ctime(&ti));
    } else if (!strcmp(argv[0],"test")) {
	    int fd = open("/mnt/b",O_RDWR);
	    if (fd < 0) {
		    perror("open");
		    return;
	    }
	    lseek(fd,10,SEEK_SET);
	    char b[] = "a";
	    write(fd,&b,1);
	    close(fd);
    } else if (!strcmp(argv[0],"umount")) {
      if (argc < 2) {
        fprintf(stderr,"specify mount point\n");
        return;
      }
      if (umount(argv[1]) < 0) {
        perror("umount");
      }
    } else {
	if (!executeCommand(argc,argv,true)) {
        	printf("Unknown command: %s\n",argv[0]);
	}
    }
}
