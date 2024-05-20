#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <complex.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/wait.h>
int main(int argc,char **argv) {
	getpwuid(0);
	return 0;
}
