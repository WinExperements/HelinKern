// Shell program that use newlib
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
char path[100];
char oldpath[100];

void parse(int argc,char **argv);

int main() {
    char line[100];
    bool exit = false;
    int argc = 0;
    char *argv[100];
    while(!exit) {
        //putc('>',stdout);
        getcwd(path,100);
        printf("[%s]> ",path);
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
    return 0;
}

bool executeCommand(int argc,char **argv) {
    // Try to find the file in argv[0]
    char execPath[100];
    // Format the string with given exec pathes.
    snprintf(execPath,100,"/initrd/%s",argv[0]);
    // Sanity check.
    int fileFd = open(execPath,O_RDONLY);
    if (fileFd < 0) {
	    return 0;
	}
    close(fileFd);
    // Fork-exec model on UNIX systems.
    int child = fork();
    if (child == 0) {
	    // Execv
	    execv(execPath,argv);
	    exit(1);
	} else {
		waitpid(child,NULL,0);
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
	    reboot(RB_POWER_OFF);
    } else if (!strcmp(argv[0],"reboot")) {
	    reboot(RB_AUTOBOOT);
    } else if (!strcmp(argv[0],"mount")) {
	    if (argc < 3) {
		    printf("Usage: mount <source> <target> <filesystem type>\r\n");
		    return;
		} else {
			if (mount(argv[1],argv[2],argv[3],0,NULL) < 0) {
				perror("Mount failed. Check kernel logs based on your current system\r\n");
				return;
			}
		}
    } else {
	if (!executeCommand(argc,argv)) {
        	printf("Unknown command: %s\n",argv[0]);
	}
    }
}
