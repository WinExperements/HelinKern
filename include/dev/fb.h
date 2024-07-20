#ifndef FB_H
#define FB_H
#include <typedefs.h>
//#define GFX_MEMORY 0x01400000
typedef struct fbinfo {
    void *vaddr;
    void *paddr;
    int width;
    int height;
    int bpp;
    int pitch;
} fbinfo_t;
typedef struct {
	uint16_t magic;
	uint8_t mode;
	uint8_t glp_size;
} Psf1;
void fb_init(fbinfo_t *info);
void fb_enable();
void fb_disable();
void fb_putchar(unsigned short int c,int x,int y,int tc,int bg);
void fb_putc(char c);
void fb_map();
void fb_clear(int color);
void fb_setpos(int x,int y);
void fb_disableCursor();
void fb_enableCursor();
void fbdev_init();
int fb_getX();
int fb_getY();
#endif
