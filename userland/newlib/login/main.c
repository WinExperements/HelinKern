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

int main() {
	char login[100];
	char password[100];
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
		if (!strcmp(login,"root")) {
			if (!strcmp(password,"toor")) {
				// Yes, it's VERY stupid to leave it here.
				//int child = fork();
				//if (child == 0) {
					char *sh_path = "/initrd/sh";
					char *sh_argv[] = {"/initrd/sh",NULL};
					execv(sh_path,sh_argv);
					exit(1);
				//} else {
					//waitpid(child,NULL,0);

				//}
			}
		}
	}
	return 0;
}
