#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

typedef struct {
    uint8_t r, g, b;
} RGBColor;

int main(int argc,char **argv) {
    if (argc < 2) {
        printf("Use %s <file path to P6 PPM> <width> <height>\r\n",argv[1]);
        return 1;
    }
    FILE *file = fopen(argv[1],"r");
    if (file < 0 || file > 100) {
        printf("Failed to open PPM image!\r\n");
        return 0;
    }
    char magicNumber[3];
    fread(magicNumber,1,2,file);
    if (magicNumber[0] != 'P' || magicNumber[1] != '6') {
        printf("Invalid PPM file format.\n");
        exit(1);
    }
    int width, height;
    int maxColorValue;
    char widthStr[4];
    char heightStr[4];
    char maxColorStr[5];
    fread(heightStr,1,1,file);
    fread(widthStr,1,sizeof(widthStr),file);
    fread(maxColorStr,1,1,file);
    fread(heightStr,1,sizeof(heightStr),file);
    widthStr[3] = 0;
    heightStr[3] = 0;
    width = atou(widthStr);
    height = atou(heightStr);
    fread(maxColorStr,1,5,file);
    maxColorStr[3] = 0;
    maxColorValue = atou(maxColorStr);
    /*int width = atou(argv[2]);
    int height = atou(argv[3]);*/
    RGBColor *colors = (RGBColor *)helin_syscall(35,width * height * sizeof(RGBColor),0,0,0,0);
    fread(colors,sizeof(RGBColor),width * height,file);
    fclose(file);
    // FB part
    FILE *fb = fopen("/dev/fb0","r");
	if (fb < 0) {
		printf("windowserver: no FB support detected or no access to it\n");
		return 1;
	}
    int w = 0;
    int h = 0;
    int p = 0;
    ioctl(fb,1,&w);
    ioctl(fb,2,&h);    
    ioctl(fb,3,&p);
	int *fb_b = mmap(fb,0,w*p,0,0);
	if (fb_b == 0) {
		printf("Failed to mmap FB\n");
	} else {
		for (int y =0; y <  height; y++) {
			for (int x =0; x < width; x++) {
                RGBColor color = colors[y * width + x];
                uint32_t pixel = (color.r << 16) | (color.g << 8) | color.b;
				fb_b[y * w + x] = pixel;
			}
		}
	}
    printf("Processed\r\n");
	fclose(fb);
    return 0;
}
