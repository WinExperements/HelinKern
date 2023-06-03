#ifndef FB_H
#define FB_H
#define GFX_MEMORY 0x01000000
typedef struct fbinfo {
    void *addr;
    int width;
    int height;
    int bpp;
    int pitch;
} fbinfo_t;
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
