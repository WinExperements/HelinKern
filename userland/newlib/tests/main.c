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
	int fd = open("/dev/satapi0",O_RDONLY);
	if (fd < 0) {
		perror("Failed to open the SATA ATAPI Device 0 Node\r\n");
		return 1;
	}
	int eject = 1;
	int stop = 0;
	int start = 0;
	int cmd = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i],"stop")) {
			stop = 1;
			eject = 0;
			break;
		} else if (!strcmp(argv[i],"start")) {
			start = 1;
			eject = 0;
			break;
		} else if (!strcmp(argv[i],"close")) {
			eject = 0;
			cmd = 0x1a;
		} else if (!strcmp(argv[1],"ftok")) {
			key_t key = ftok(argv[0],'A');
			printf("Key: %d\r\n",key);
			return 0;
		}
	}
	if (eject) {
		cmd = 0x1b;
	}
	if (stop) {
		cmd = 0x1c;
	}
	if (start) {
		cmd = 0x1f;
	}
	ioctl(fd,0x31,cmd);
	close(fd);
	return 0;
}
