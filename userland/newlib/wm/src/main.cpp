#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <software_render.h>
#include <sys/mman.h>
#include <png.h>
int main() {
	/*printf("HelinOS C++ Window Compositor\r\n");
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
	close(fd);*/
	printf("Testing png\r\n");
	FILE *img = fopen("/home/user/HelinKern/iso/usr/share/test.png","rb");
	if (img == nullptr) {
		fprintf(stderr,"Failed to open test image\r\n");
		return 1;
	}
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fprintf(stderr,"Failed to create PNG structure\r\n");
		return 1;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		fprintf(stderr,"Failed to init info\r\n");
		return 1;
	}
	if(setjmp(png_jmpbuf(png))) abort();
	png_init_io(png,img);
	png_read_info(png,info);
	int width = png_get_image_width(png, info);
	int height = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth  = png_get_bit_depth(png, info);
	png_bytep *row_pointers = NULL;
	if(bit_depth == 16)
    png_set_strip_16(png);

  if(color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if(color_type == PNG_COLOR_TYPE_RGB ||
     color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if(color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  if (row_pointers) abort();

  row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
  for(int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
  }

  png_read_image(png, row_pointers);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(img);
	printf("First byte: 0x%x\r\n",row_pointers[0][0]);
	return 0;
}
