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
extern char _binary_font_psf_start;
extern char _binary_font_psf_end;
uint16_t *unicode;
static void psf_init();
static uint32_t cursor_x,cursor_y;
static void printCursor(int x,int y);
static bool mapped;
static uint32_t ws_row,ws_col;
static void scroll();
static int width,height,pitch,addr,fcolor,bcolor;
static bool enableCursor = true;
void fb_init(fbinfo_t *fb) {
    if (!fb) return;
    psf_init();
    width = fb->width;
    height = fb->height;
    pitch = fb->pitch;
    addr = (uint32_t)fb->addr;
    ws_row = height/16;
 	ws_col = width/9;
    output_write("FB initialized\r\n");
}
void fb_enable() {
    // Must disable UART output and enable this FB output
    if (width == 0) return;
    fb_clear(BLACK);
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
    PSF_font *font = (PSF_font*)&_binary_font_psf_start;
    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* unicode translation */
    if(unicode != NULL) {
        c = unicode[c];
    }
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)&_binary_font_psf_start +
     font->headersize +
     (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->height * pitch) +
        (cx * (font->width+1) * 4);
    /* finally display pixels according to the bitmap */
    uint32_t x,y, line,mask;
    for(y=0;y<font->height;y++){
        /* save the starting position of the line */
        line=offs;
        mask=1<<(font->width-1);
        /* display a row */
        for(x=0;x<font->width;x++)
        {
            if (c == 0)
            {
                *((uint32_t*)((uint8_t*)addr + line)) = bg;
            }
            else
            {
                *((uint32_t*)((uint8_t*)addr + line)) = ((int)*glyph) & (mask) ? fg : bg;
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
    while((int)s>_binary_font_psf_end) {
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
  if (ch == 0x08 && cursor_x)
    {
      fb_putchar(' ',cursor_x,cursor_y,WHITE,BLACK);
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
      fb_putchar(' ',cursor_x,cursor_y,WHITE,BLACK);
      cursor_x = 0;
      cursor_y++;
    }

  // IF all the above text fails , print the Character
  if (ch >= ' ')
    {
      // Calculate the Address of the Cursor Position
      fb_putchar(ch,cursor_x,cursor_y,fcolor,bcolor);
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
}
static void printCursor(int x,int y) {
    if (enableCursor)fb_putchar(' ',x,y,bcolor,fcolor);
}
/* Called when the FB need to be mapped to the memory, usually at startup and when creating new process */
void fb_map() {
	if (mapped) return;
    if (addr != NULL) {
        // Map the FB
        uint32_t p_address = (uint32_t)addr;
	uint32_t v_address = (uint32_t)GFX_MEMORY;
	uint32_t size = pitch * height;
	uint32_t pages = size / PAGESIZE_4K;
	for (uint32_t i = 0; i < pages; i++) {
		uint32_t offset = i * PAGESIZE_4K;
		arch_mmu_mapPage(NULL,v_address + offset,p_address + offset,7);
	}
	addr = GFX_MEMORY;
	mapped = true;
    }
    //kprintf("FB mapped: from 0x%x to 0x%x\r\n",->addr,(->addr)+(->pitch*->width));
}
void fb_clear(int color) {
    if (color == WHITE) {
        fcolor = color;
        bcolor = BLACK;
    } else if (color == BLACK) {
        bcolor = color;
        fcolor = WHITE;
    }
    uint32_t *fb = (uint32_t *)addr;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fb[x+y*width] = color;
        }
    }
    cursor_x = cursor_y = 0;
}
static void scroll() {
    if (cursor_y >= ws_row) {
        arch_cli();
        for (int y = 16; y != height; ++y)
        {
            void *dest = (void *)(((uintptr_t)addr) + ( y - 16) * pitch);
            const void* src = (void *)(((uintptr_t)addr) + y * pitch);
            memcpy(dest,src,width * 4);
        }
        for (int y = height - 16; y != height; ++y)
        {
            uint32_t* dest = (uint32_t *)(((uintptr_t)addr) + y * pitch);
            for (int i = 0; i != width; ++i)
            {
                *dest++ = bcolor;
            }
        }
        cursor_y = ws_row-1;
        arch_sti();
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
