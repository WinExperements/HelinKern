#include "stdio.h"
#include "unistd.h"

int fork() {
	return helin_syscall(49,0,0,0,0,0);
}

int main(int argc,char **argv) {
    printf("TEST!\n");
    int pid = fork();
    if (pid == 0) {
        printf("CHILD!\n");
    } else {
        printf("Not child, ret = 0x%X\n",pid);
        while(1) {}
    }
    return 0;
}
