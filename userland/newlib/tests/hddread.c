#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
int main(int argc,char **argv) {
	printf("Hard drive reading test\r\n");
	if (argc < 2) {
		printf("%s: <read,on,off>\r\n","a");
		return 1;
	}
	int hd = open("/dev/sata0",O_RDONLY);
	if (hd < 0) {
		perror("Failed to open hard drive.");
		hd = open("/dev/satapi0",O_RDWR);
		if (hd < 0) {
			printf("Failed to open even the CDROM\r\n");
			return 1;
		}
	}
	if (!strcmp(argv[1],"read")) {
		ioctl(hd,0x30);
		printf("Readed\r\n");
	} else if (!strcmp(argv[1],"on")) {
		ioctl(hd,0x20);
	} else if (!strcmp(argv[1],"off")) {
		ioctl(hd,0x10);
	} else if (!strcmp(argv[1],"exp")) {
		void *buff = malloc(512);
		ioctl(hd,0x30,buff);
		free(buff);
	} else if (!strcmp(argv[1],"eject")) {
		close(hd);
		hd = open("/dev/satapi0",O_RDWR);
		if (hd < 0) {
			perror("Failed to access CDROM");
			return 1;
		}
		// This ioctl sends ATAPI packet to device(PACKET must be non-data)
		int cmd = 0x1b;
		ioctl(hd,0x31,cmd);
	} else if (!strcmp(argv[1],"readmbr")) {
		if (0) {
			printf("Partitions are already detected\r\n");
		} else {
			char *gpt = malloc(512);
			read(hd,gpt,512);
			printf("Dumping MBR(buffer is %p): ",gpt);
			fflush(stdout);
			for (int i = 0; i < 512; i++) {
				printf("0x%x ",gpt[i]);
				fflush(stdout);
			}
			free(gpt);
		}
	} else if (!strcmp(argv[1],"readext")) {
		int fd = open("/fat/large_file",O_RDONLY);
		if (fd < 0) {
			printf("Failed to open large file\r\n");
			return 0;
		}
		void *buff = malloc(20480);
		read(fd,buff,20480);
		printf("Readed\r\n");
		free(buff);
		close(fd);
	} else {
		printf("Unknown command: %s\r\n",argv[1]);
	}
	close(hd);
	return 0;
}
