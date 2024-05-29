#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <complex.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/wait.h>
int main(int argc,char **argv) {
	printf("Creating file in read-only file system\r\n");
	int ret = creat("abama.txt",0);
	if (ret < 0) {
		perror("Failed to create(right): ");
		return 0;
	} else {
		printf("Creating successfull?\r\n");
		return 1;
	}
	return 0;
}
