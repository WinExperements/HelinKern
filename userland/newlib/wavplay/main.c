#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libwav/libwav.h"
#include <fcntl.h>
#include <sys/ioctl.h>
typedef struct _drv {
	char driver_name[50];
} DriverInfo;
int main(int argc,char **argv) {
	if (argc < 1) {
		fprintf(stderr,"Specify RAW file\n");
		return 1;
	}
	int fd = open(argv[1],O_RDWR);
	if (fd < 0) {
		perror("open");
	}
	int snd_fd = open("/dev/snd0",O_RDWR);
	if (snd_fd < 0) {
		perror("failed to open sound device");
		return 1;
	}
	DriverInfo drvInf;
	ioctl(snd_fd,20,&drvInf);
	printf("wavplay: using driver => %s\n",drvInf.driver_name);
	printf("wavplay: setting volume to 100%\n");
	if (ioctl(snd_fd,13,2,100) < 0) {
		printf("wavplay: error of setting volume\n");
	}
	void *buffer = malloc(0x1000);
	ioctl(snd_fd,10);
	while(1) {
		if (read(fd,buffer,0x1000) <= 0) {
			// No more data here!
			break;
		}
		write(snd_fd,buffer,0x1000);
	}
	close(fd);
	close(snd_fd);
	return 0;
}
