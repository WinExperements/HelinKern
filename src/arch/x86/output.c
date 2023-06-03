#include <arch/x86/io.h>
#include <output.h>
#include <dev/fb.h>
#define PORT 0x3f8
static bool fbUsed = false;
static uint16_t *vgaBuff = (uint16_t *)0xB8000; // alternative output
static uint16_t vga_x,vga_y;
static void vga_scroll();
static void vga_putc(char c);
void vga_change();
static bool vgaUsed = false;
static uint8_t fg = 0xff;
static uint8_t bg = 0x0;
void output_uart_init() {
   // Prepare UART output before primary output is begin initialized
   outb(PORT + 1, 0x00);
   outb(PORT + 3, 0x80);
   outb(PORT + 0, 0x03);
   outb(PORT + 1, 0x00);
   outb(PORT + 3, 0x03);
   outb(PORT + 2, 0xC7);
   outb(PORT + 4, 0x0B);
   outb(PORT + 4, 0x1E);
   outb(PORT + 0, 0xAE);
   if(inb(PORT + 0) != 0xAE) {
      return;
   }
   outb(PORT + 4, 0x0F);
}
int serial_received() {
   return inb(PORT + 5) & 1;
}
 
char read_serial() {
   while (serial_received() == 0);
 
   return inb(PORT);
}
int is_transmit_empty() {
   return inb(PORT + 5) & 0x20;
}
 
void write_serial(char a) {
    if (fbUsed) {
        fb_putc(a);
        return;
    }
   while (is_transmit_empty() == 0);
 
   outb(PORT,a);
}

void output_write(char *msg) {
    int i = 0;
    while(msg[i]) {
        putc(msg[i]);
        i++;
    }
}
void output_changeToFB() {
    if (vgaUsed) return;
    fbUsed = true;
}
void putc(char c) {
    if (fbUsed) {
        fb_putc(c);
        return;
    }
    if (vgaUsed) {
	vga_putc(c);
	return;
    }
    write_serial(c);
}
static void vga_putc(char c) {
	if (c == 0x08 && vga_x) {
		vgaBuff[vga_y * 80 + vga_x] = 0x20;
        vga_x--;
	} else if (c == '\n') {
		vga_x = 0;
		vga_y++;
	}
	if (c >= ' ') {
		vga_x++;
        uint8_t attributeByte = (bg << 4) | (fg & 0x0f);
	    uint16_t attrib = attributeByte << 8;
	    uint16_t *location = vgaBuff + (vga_y * 80 + vga_x);
	    *location = c | attrib;
	}
	if (vga_x >= 80) {
		vga_x = 0;
		vga_y++;
	}
	if (vga_y >= 25) {
		vga_scroll();
	}
}
static void vga_scroll() {
	for (int i = 0*80; i < 24*80; ++i) {
     		 vgaBuff[i] = vgaBuff[i+80];
    	}
    	for (int i = 24*80; i < 25*80; ++i) {
      		vgaBuff[i] = 0x20;
    	}
	vga_y = 24;
}
void vga_change() {
	// Clear screen using memset
	memset(vgaBuff,0,80*25);
	fbUsed = false;
	vgaUsed = true;
    vga_x = vga_y = 0;
}
