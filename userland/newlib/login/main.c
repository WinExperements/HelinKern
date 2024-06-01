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

// HelinOS specific stuff.
#define FLAG_ECHO 0000001
#define SH_PATH "/bin/sh"

char *users[] = {"root","user"};
char *passwords[] = {"toor","yes"}; // when libc add support for the shadow password storing, then i delete this.
int uids[] = {0,1000};
extern char **environ;
// More POSIX portable :)

void runShell() {
	char *sh_path = SH_PATH;
	char *sh_argv[] = {SH_PATH,NULL};
	int pid = fork();
	if (pid == 0) {
		execv(sh_path,sh_argv);
		exit(1);
	} else {
		waitpid(pid,NULL,0);
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
int main() {
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
		for (int i = 0; i < 2; i++) {
			if (!strcmp(login,"poweroff")) {
				kill(1,SIGINT);
			}
			if (!strcmp(login,users[i])) {
				if (!strcmp(password,passwords[i])) {
					setuid(uids[i]);
					runShell();
					logged = 1;
				}
			}
		}
		if (!logged) {
			printf("Incorrect login\r\n");
		}
	}
	return 0;
}
