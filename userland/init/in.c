/* in.c - userspae program test */
#include "stdio.h"
#include "unistd.h"
#include "wait.h"
#include "syscall.h"
int main() {
	char *path = "/bin/test";
	int pid = getpid();
	int w = 0;
	printf("in.c - an test program for HelinOS project\n");
	/*if (execv(path,0,NULL) == 1) {
		//waitpid(pid,0,NULL);
	} else {
		printf("execv: returned non positive value! Exit!\n");
		return 1;
	}
	// BUG: The same program, cannot be runned twice, got #PF
	if (execv(path,0,NULL) == 1) {
                //waitpid(pid,0,NULL);
        } else {
                printf("execv: returned non positive value! Exit!\n");
                return 1;
        }*/
	helin_syscall(21,0,0,0,0,0);
	//helin_syscall(21,0,0,0,0,0);
    for (;;) {}
	return 0;
}
