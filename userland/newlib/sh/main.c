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
char path[100];
char oldpath[100];

void parse(int argc,char **argv);
extern char **environ;
int main() {
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
      char *where = strtok(getenv("PATH"),":");
      while(where) {
    	  snprintf(execPath,100,"/bin/%s",argv[0]);
        int f = 0;
        if ((f = open(execPath,O_RDONLY)) > 0) {
          foundedPath = where;
          break;
        }
        where = strtok(NULL,":");
      }
    }
    if (foundedPath == NULL) {
      return false;
    }
    // Fork-exec model on UNIX systems.
    int child = fork();
    if (child == 0) {
	    // Execv
	    execv(execPath,argv);
	    perror(argv[0]);
	    exit(1);
	} else {
		if (wait) {waitpid(child,NULL,0);}
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
            printf("Failed to open %s\n",(argc > 1 ? argv[1] : path));
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
	    int fd = open(argv[1],O_RDONLY);
	    if (fd < 0) {
		    perror("cwd fail: ");
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
		} else {
			if (mount(argv[1],argv[2],argv[3],0,NULL) < 0) {
				perror("Mount failed. Check kernel logs based on your current system\r\n");
				return;
			}
		}
    } else if (!strcmp(argv[0],"uname")) {
	    struct utsname osname;
	    uname(&osname);
	    printf("%s %s %s %s\r\n",osname.sysname,osname.machine,osname.version,osname.release);
	    printf("Number of implemented in kernel syscalls: %d\r\n",NUM_SYSCALLS);
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
		    printf("%d tty0 notaval %s\r\n",lst[i].pid,lst[i].name);
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
    } else {
	if (!executeCommand(argc,argv,true)) {
        	printf("Unknown command: %s\n",argv[0]);
	}
    }
}
