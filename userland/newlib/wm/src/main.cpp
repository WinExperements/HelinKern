#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <software_render.h>
#include <sys/mman.h>
int main() {
	printf("HelinOS C++ Window Compositor\r\n");
	int fd = open("/dev/fb0",O_RDWR);
	if (fd < 0) {
		perror("Failed to open framebuffer!!! Error");
		return 1;
	}
	int w = 0;
	int h = 0;
	ioctl(fd,1,&w);
	ioctl(fd,2,&h);
	printf("Screen width -> %d, height -> %d\r\n",w,h);
	void *buffer = mmap(NULL,w*h*4,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
	if (buffer == (void *)-1) {
		perror("Main framebuffer access failure");
		close(fd);
		return 1;
	}
	RenderCore *rend = CreateSoftwareRender();
	if (!rend) {
		fprintf(stderr,"Failed to create software renderer!\r\n");
		return 1;
	}
	if (!rend->Init(w,h,buffer)) {
		fprintf(stderr,"Failed to initialize software rendrerer\r\n");
		return 1;
	}
	rend->ClearScreen(255,255,255);
	delete rend;
	munmap(buffer,w*h*4);
	close(fd);
	return 0;
}
