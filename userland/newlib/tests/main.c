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
#include <sys/ioctl.h>
#include <string.h>
#include <sys/ipc.h>
int main(int argc,char **argv) {
	void *a = mmap(0,0,0,0,0,0);
  perror("mmap should fail");
	return 0;
}
