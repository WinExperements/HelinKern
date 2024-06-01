#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <complex.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <libgen.h>
int main(int argc,char **argv) {
	for (int i = 0; i < argc; i++) {
		printf("%s\r\n",argv[i]);
	}
	return 0;
}
