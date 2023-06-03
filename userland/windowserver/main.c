// Window server for HelinOS
#include "stdio.h"
#include "stdlib.h"
#include "sys/mman.h"
int main(int argc,char **argv) {
	if (argc > 1) {
		printf("windowserver: no arguments support\n");
		return 1;
	}
	// try to open the FB
	FILE *fb = fopen("/dev/fb0","r");
	if (!fb) {
		printf("windowserver: no FB support detected or no access to it\n");
		return 1;
	}
	int *fb_b = mmap(fb,0,1336*768*32,0,0);
	if (fb_b == 0) {
		printf("Failed to mmap FB\n");
	} else {
		for (int y =0; y <  768; y++) {
			for (int x =0; x < 1336; x++) {
				fb_b[x + y * 1336] = 0xFFFFFFFF;
			}
		}
		printf("AHAHA\n");
	}
	fclose(fb);
	return 0;
}
