// HelinOS bare coreutils package.
// Login package, rewriting of original code.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

// HelinOS specific stuff.
#define FLAG_ECHO 0000001
#define SH_PATH "/bin/sh"

char *users[] = {"root","user"};
char *passwords[] = {"toor","yes"}; // when libc add support for the shadow password storing, then i delete this.
int uids[] = {0,1000};
extern char **environ;
int ttyFlags = 0;
void disableEcho() {
	int stfd = fileno(stdout);
	ioctl(stfd,1,&ttyFlags);
	ttyFlags = ttyFlags &~ FLAG_ECHO;
	ioctl(stfd,2,&ttyFlags);
}
void enableEcho() {
	int stfd = fileno(stdout);
	ttyFlags = ttyFlags | FLAG_ECHO;
	ioctl(stfd,2,&ttyFlags);
}

void runShell() {
	char *sh_path = SH_PATH;
	char *sh_argv[] = {SH_PATH,NULL};
	execv(sh_path,sh_argv);
	exit(1);
}

int main() {
	char login[100];
	char password[100];
//  printf("environ[0]: %s\r\n",environ[0]);
	while(1) {
		printf("login: ");
		fflush(stdout);
		scanf("%s",login);
		printf("Password: ");
		fflush(stdout);
		disableEcho();
		scanf("%s",password);
		enableEcho();
		printf("\r\n");
		for (int i = 0; i < 2; i++) {
			if (!strcmp(login,users[i])) {
				if (!strcmp(password,passwords[i])) {
					setuid(uids[i]);
					runShell();
				}
			}
		}
		printf("login: incorrect login\r\n");
	}
	return 0;
}
