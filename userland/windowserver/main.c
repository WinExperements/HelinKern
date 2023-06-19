// Window server for HelinOS
#include "stdio.h"
#include "stdlib.h"
#include "sys/mman.h"
#include "sys/ioctl.h"
int main(int argc,char **argv) {
	if (argc > 1) {
		printf("windowserver: no arguments support,  passed: %d arguments!\n");
		return 1;
	}
	// try to open the FB
	FILE *fb = fopen("/dev/fb0","r");
	if (fb < 0) {
		printf("windowserver: no FB support detected or no access to it\n");
		return 1;
	}
    int w = 0;
    int h = 0;
    int p = 0;
    printf("ioctl\r\n");
    ioctl(fb,1,&w);
    ioctl(fb,2,&h);    
    ioctl(fb,3,&p);
    printf("mmap: %u, %u, %u, 0x%x,0x%x\r\n",w,h,p,&w,&h);
	int *fb_b = mmap(fb,0,w*p,0,0);
	if (fb_b == 0) {
		printf("Failed to mmap FB\n");
	} else {
		for (int y =0; y <  h; y++) {
			for (int x =0; x < w; x++) {
				fb_b[x + y * w] = 0xff0000;
			}
		}
		printf("AHAHA\n");
	}
	fclose(fb);
	return 0;
}
