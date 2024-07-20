/*
    Framebuffer driver, aka FB console driver
*/
#include <dev/fb.h>
#include <typedefs.h>
#include <output.h>
#include <arch/mmu.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <arch.h>
#include <thread.h>
#include <dev.h>
#include <debug.h>
extern char _binary_font_psf_start;
extern char _binary_font_psf_end;
uint16_t *unicode;
static void psf_init();
static int cursor_x,cursor_y;
static void printCursor(int x,int y);
static bool mapped;
static int ws_row,ws_col;
static void scroll();
static int width,height,pitch,fcolor,bcolor;
static paddr_t addr;
static bool enableCursor = true;
static uint64_t fbdev_write(vfs_node_t *node,uint64_t off,uint64_t size,void *buff);
static void *fbdev_mmap(struct vfs_node *node,int addr,int size,int offset,int flags);
static dev_t *fbdev;
static int paddr;
static char *charBuff;
static void syncFB(); // draw all characters from charBuff
static uint64_t fb_ioctl(struct vfs_node *node,int request,va_list args);
static uintptr_t GFX_MEMORY = 0;
extern bool disableOutput;
static uintptr_t fontAddr = (uintptr_t)&_binary_font_psf_start;
/* ==== PSF v1 Support to FBDEV driver ===== */

void fb_init(fbinfo_t *fb) {
    if (!fb) return;
    psf_init();
    width = fb->width;
    height = fb->height;
    pitch = fb->pitch;
    addr = (size_t)fb->vaddr;
    paddr = (size_t)fb->paddr;
    ws_row = height/16;
    ws_col = width/9;
    output_write("FB initialized\r\n");
}
void fb_enable() {
    // Must disable UART output and enable this FB output
    if (width == 0) return;
    // Don't clear the framebuffer, because i want to see the butifull black screen or device manufactorer logo(Hello Android!)
    // Прапор України обов'язково
    cursor_x = cursor_y = 10;
    bcolor = WHITE;
    fcolor = BLACK; // UNIX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (charBuff != NULL) {
	    // At init
	    memset(charBuff,0,ws_col*ws_row);
    }
    DEBUG("ws_col: %d, ws_row: %d\r\n",ws_col,ws_row);
}
void fb_putchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and 0 0s, say 0xFFFFFF and 0x000000 */
    int fg, int bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)fontAddr;
    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)fontAddr +
     font->headersize +
     (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->height * pitch) +
        (cx * (font->width+1) * 4);
    /* finally display pixels according to the bitmap */
    uint32_t x,y, line,mask;
    int fnt_height = font->height;
    int fnt_width = font->width;
    uintptr_t limit = pitch * width;
    for(y=0;y<fnt_height;y++){
        /* save the starting position of the line */
        line=offs;
        mask=1<<(fnt_width-1);
        /* display a row */
        for(x=0;x<fnt_width;x++)
        {
            if (c == 0)
            {
                *((uint32_t*)((uint8_t*)addr + line)) = bg;
            }
            else if (c != 0)
	    {
		if (line >= limit) continue;
                *((uint32_t*)((uint8_t*)addr + line)) = ((int)*glyph) & (mask) ? fg : bg;
		//*((uint32_t*)((uint8_t*)addr + line)) = 0xff0000;
            }

            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += pitch;
    }
}
static void psf_init()
{
    uint16_t glyph = 0;
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_font_psf_start;
    /* is there a unicode table? */
    if (font->bytesperglyph == 0) {
	    kprintf("PSF1? \r\n");
	    Psf1 *psf1 = (Psf1 *)&_binary_font_psf_start;
	    kprintf("0x%x, mode -> 0x%x\r\n",psf1->magic,psf1->mode);
	    PANIC("A");
	}
    if (font->flags) {
        unicode = NULL;
        return; 
    }
	unicode = kmalloc(font->numglyph);
	memset(unicode,0,font->numglyph);
    /* get the offset of the table */
    char *s = (char *)(
    (unsigned char*)&_binary_font_psf_start +
      font->headersize +
      font->numglyph * font->bytesperglyph
    );
    /* allocate memory for translation table */
    while((size_t)s>_binary_font_psf_end) {
        uint16_t uc = s[0];
        if(uc == 0xFF) {
            glyph++;
            s++;
            continue;
        } else if(uc & 128) {
            /* UTF-8 to unicode */
            if((uc & 32) == 0 ) {
                uc = ((s[0] & 0x1F)<<6)+(s[1] & 0x3F);
                s++;
            } else
            if((uc & 16) == 0 ) {
                uc = ((((s[0] & 0xF)<<6)+(s[1] & 0x3F))<<6)+(s[2] & 0x3F);
                s+=2;
            } else
            if((uc & 8) == 0 ) {
                uc = ((((((s[0] & 0x7)<<6)+(s[1] & 0x3F))<<6)+(s[2] & 0x3F))<<6)+(s[3] & 0x3F);
                s+=3;
            } else
                uc = 0;
        }
        /* save translation */
        unicode[uc] = glyph;
        s++;
    }
}
void fb_putc(char ch) {
    // Handle a backspace, by moving the cursor back one space
    //arch_cli();
  if (ch == 0x08 && cursor_x)
    {
      fb_putchar(' ',cursor_x,cursor_y,0xffffff,BLACK);
      cursor_x--;
    }

  // Handle a tab by increasing the cursor's X, but only to a point
  // where it is divisible by 8.
  else if (ch == 0x09)
    {
      cursor_x = (cursor_x+8) & ~(8-1);
    }

  // Handle newline by moving cursor back to left and increasing the row
  else if (ch == '\n')
    {
      fb_putchar(' ',cursor_x,cursor_y,0xffffff,BLACK);
      cursor_x = 0;
      cursor_y++;
    }

  // IF all the above text fails , print the Character
  if (ch >= ' ')
    {
      // Calculate the Address of the Cursor Position
      fb_putchar(ch,cursor_x,cursor_y,fcolor,bcolor);
      if (charBuff != NULL )charBuff[cursor_y * ws_col  + cursor_x] = ch;
      cursor_x++;
    }

  // IF after all the printing we need to insert a new line
  if (cursor_x >= ws_col)
    {
      cursor_x = 0;
      cursor_y++;
    }
    scroll();
    printCursor(cursor_x,cursor_y);
    //arch_sti();
}
static void printCursor(int x,int y) {
    if (enableCursor)fb_putchar(' ',x,y,bcolor,fcolor);
}
/* Called when the FB need to be mapped to the memory, usually at startup and when creating new process */
void fb_map() {
    //if (mapped) return;
    if (width != 0 && height != 0) {
        GFX_MEMORY = /*0x02800000*/addr;
        // Map the FB
	    uint32_t p_address = (uint32_t)paddr;
	    uint32_t v_address = (uint32_t)GFX_MEMORY;
	    uintptr_t size = pitch * width;
	    uintptr_t pages = size / PAGESIZE_4K;
	    for (uintptr_t i = 0; i < pages; i++) {
		    uintptr_t offset = i * PAGESIZE_4K;
		    arch_mmu_mapPage(NULL,v_address + offset,p_address + offset,7);
	    }
	    addr = v_address;
	    mapped = true;
	    if (!mapped) cursor_x = cursor_y = 0; // why not?
    }
    //kprintf("FB mapped: from 0x%x to 0x%x\r\n",addr,(addr)+(pitch*width));
}
void fb_clear(int color) {
    if (color == 0xffffff) {
        fcolor = color;
        bcolor = BLACK;
    } else if (color == BLACK) {
        bcolor = color;
        fcolor = 0xffffff;
    }
    if (!disableOutput) {
    	uint32_t *fb = (uint32_t *)addr;
    	for (int y = 0; y < height; y++) {
        	for (int x = 0; x < width; x++) {
            		fb[x+y*width] = color;
        	}
    	}
    }
    cursor_x = cursor_y = 0;
    // Clear character buffer
    if (charBuff != NULL) {
	    // At init
	    memset(charBuff,0,ws_col*ws_row);
    }
}
static void scroll() {
	if (cursor_y >= ws_row-1 && charBuff == NULL) {
		cursor_x = cursor_y = 0;
		return;
	}
    if (charBuff == NULL) return;
    if (cursor_y >= ws_row-1) {
	for (int i = 0; i < ws_row*(ws_col-1); i++) {
		charBuff[i] = charBuff[i+ws_col];
	}
    for (int i = (ws_row-2)*ws_col; i < ws_row*ws_col-1; i++) {
        charBuff[i] = 0;
    }
	syncFB();
	cursor_y = ws_row - 2;
    }
}
void fb_setpos(int x,int y) {
    cursor_x = x;
    cursor_y = y;
}
void fb_disableCursor() {
    enableCursor = false;
}
void fb_enableCursor() {
    enableCursor = true;
}
static uint64_t fbdev_write(vfs_node_t *node,uint64_t off,uint64_t size,void *buff) {
    // Nothing!
    return size;
}
static void *fbdev_mmap(struct vfs_node *node,int _addr,int size,int offset,int flags) {
    // First allocate requested space
    unsigned long vstart = arch_mmu_query(NULL,USER_MMAP_START,size);
    if (vstart == -1) {
	    // No free virtual memory?
	    return (void *)-1;
    }
    for (int i = 0; i < size/4096; i++) {
        // Map
        // TODO: Fix  this shit
        int pag = (i*4096);
        arch_mmu_mapPage(NULL,vstart+pag,paddr+pag,7);
    }
    kprintf("Now found at 0x%x\r\n",arch_mmu_query(NULL,USER_MMAP_START,size));
    return (void *)vstart;
}
void fbdev_init() {
	if (width == 0 || height == 0) return;
	fbdev = kmalloc(sizeof(dev_t));
    	memset(fbdev,0,sizeof(dev_t));
    	fbdev->name = "fb0";
    	fbdev->buffer_sizeMax = pitch * height;
    	fbdev->write = fbdev_write;
    	fbdev->mmap = fbdev_mmap;
    	fbdev->ioctl = fb_ioctl;
    	dev_add(fbdev);
    	charBuff = kmalloc(ws_row*ws_col);
    	memset(charBuff,0,ws_row*ws_col);
    	cursor_x = cursor_y = 0;
}
int fb_getX() {
    return cursor_x;
}
int fb_getY() {
    return cursor_y;
}
static void syncFB() {
    // Ми не чіпаємо рамки тому все починається з 1
	for (int y = 0; y < ws_row; y++) {
		for (int x =0; x <  ws_col; x++) {
			fb_putchar(charBuff[y * ws_col + x],x,y,fcolor,bcolor);
		}
	}
}
static uint64_t fb_ioctl(struct vfs_node *node,int request,va_list args) {
    int *arg = va_arg(args,int *);
    switch(request) {
        case 1:
        // get width
        *arg = width;
        break;
        case 2:
        *arg = height;
        break;
        case 3:
        *arg = pitch;
        break;
    }
    return 0;
}
