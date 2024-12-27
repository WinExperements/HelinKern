// HelinOS bare coreutils package.
// Login package, rewriting of original code.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <fcntl.h>
// HelinOS specific stuff.
#define FLAG_ECHO 0000001
int uids[] = {0,1000};
extern char **environ;
// More POSIX portable :)
void runShell(char *whichOne) {
	char *sh_path = whichOne;
	char *sh_argv[] = {whichOne,NULL};
	int pid = fork();
	if (pid == 0) {
		execv(sh_path,sh_argv);
                perror("failed to run shell");
		exit(1);
	} else {
		while(1) {
			if (waitpid(pid,NULL,0) > 0) {
				break;
			}
			// Interrupted syscall?
		}
	}
}
void disableEcho() {
	struct termios term;
	tcgetattr(fileno(stdin),&term);
	term.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin),0,&term);
}
void enableEcho() {
	struct termios term;
	tcgetattr(fileno(stdin),&term);
	term.c_lflag |= ECHO;
	tcsetattr(fileno(stdin),0,&term);
}
int main(int argc,char **argv) {
  if (argc > 1) {
    int devFd = open(argv[1],O_RDWR);
    if (devFd < 0) {
      perror("device open error");
      return 1;
    }
    close(0);
    close(1);
    close(2);
    dup2(devFd,0);
    dup2(devFd,1);
    dup2(devFd,2);
    printf("login: Perfoming TTY login on %s\n",argv[1]);
    runShell("/bin/bash");
  }
	char login[100];
	char password[100];
  	char hostname[100];
  	gethostname(hostname,100);
	while(1) {
		printf("%s login: ",hostname);
		fflush(stdout);
		scanf("%s",login);
		printf("Password: ");
		fflush(stdout);
		disableEcho();
		scanf("%s",password);
		enableEcho();
		printf("\r\n");
		int logged = 0;
		struct passwd *p;
#if !defined(__x86_64__)
		while((p = getpwent()) > 0) {
			if (!strcmp(login,p->pw_name)) {
				if (!strcmp(password,p->pw_passwd)) {
					setuid(p->pw_uid);
                                        // open /etc/motd.
                                        int motd_fd = open("/etc/motd",O_RDONLY);
                                        if (motd_fd < 0) {
                                                goto runShell;
                                        }
                                        struct stat st;
                                        if (fstat(motd_fd,&st) < 0) {
                                                goto runShell;
                                        }
                                        char *buff = malloc(1024);
                                        int ho = st.st_size;
                                        while(ho > 0) {
                                                read(motd_fd,buff,1024);
                                                printf(buff);
                                                ho-=1024;
                                        }
                                        fflush(stdout);
                                        free(buff);
                                        close(motd_fd);
runShell:
					runShell("/bin/sh");
					logged = 1;
				}
			}
		}
#else
		if (!strcmp(login,"root") && !strcmp(password,"toor")) {
			logged = 1;
			runShell("/bin/bash");
		}
#endif
		if (!logged) {
			printf("Incorrect login\r\n");
		}
	}
	/*printf("Hello! GUI test!\r\n");
	sleep(5);
	printf("Running landmine\r\n");
	int f = fork();
	char *sh_path = "/mnt/usr/bin/landmine";
	char *sh_argv[] = {"/mnt/usr/bin/landmine",NULL};
	if (f == 0) {
		execv(sh_path,sh_argv);
		exit(1);
	}
	while(1) {}*/
	return 0;
}
