#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <complex.h>
#include <spawn.h>
int main(int argc,char **argv) {
	printf("HelinOS POSIX compatability all in one test suite.\r\n");
	double cos = ccos(10);
  posix_spawn(NULL,NULL,NULL,NULL,NULL,NULL);
	return 0;
}
