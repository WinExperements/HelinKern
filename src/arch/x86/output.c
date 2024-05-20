#include <arch/x86/io.h>
#include <output.h>
#include <dev/fb.h>
//#define PORT 0x3f8
static int PORT = 0x3f8;
static bool fbUsed = false;
static uint16_t *vgaBuff = (uint16_t *)0xB8000; // alternative output
static uint16_t vga_x,vga_y;
static void vga_scroll();
static void vga_putc(char c);
void vga_change();
static bool vgaUsed = false;
static uint8_t fg = 0xff;
static uint8_t bg = 0x0;
static uint16_t vga_getPos();
static void update_cursor(int x, int y);
static void disable_cursor();
static void draw_ramka();
static bool disableCursor = false;
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
   // fb_map();
    
    fb_clear(0x0);
}
void output_putc(char c) {
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
static void vga_putchar(char c,int x,int y) {
     uint8_t attributeByte = (bg << 4) | (fg & 0x0f);
	 uint16_t attrib = attributeByte << 8;
	 uint16_t *location = vgaBuff + (y * 80 + x);
	 *location = c | attrib;
}
static void vga_putc(char c) {
	if (c == 0x08 && vga_x) {
        	if (!disableCursor) vga_putchar(' ',vga_x,vga_y);
		vga_x--;
		vgaBuff[vga_y * 80 + vga_x] = 0x20;
	} else if (c == '\n') {
        	if (!disableCursor) vga_putchar(' ',vga_x,vga_y);
		vga_x = 1;
		vga_y++;
	}
	if (c >= ' ') {
        vga_putchar(c,vga_x,vga_y);
        vga_x++;
	}
	if (vga_x >= 78) {
		vga_x = 1;
		vga_y++;
	}
	if (vga_y >= 24) {
		vga_scroll();
	}
        if (!disableCursor) update_cursor(vga_x,vga_y);
}
static void vga_scroll() {
	for (int y = 1; y < 23; y++) {
		for (int x = 1; x < 78; x++) {
			int i = y * 80 + x;
			vgaBuff[i] = vgaBuff[i+80];
		}
	}
	// Стрем лінію
	for (int x = 1; x < 77; x++) {
		vgaBuff[23 * 80 + x] = 0x20;
	}
	vga_y = 23;
}

void vga_change() {
    uint16_t pos = vga_getPos();
    vga_x = pos % 80;
    vga_y = pos / 80;
	// Clear screen using memset
	fbUsed = false;
	vgaUsed = true;
    if (vga_x < 0) vga_x = 0;
    disable_cursor();
    draw_ramka(); // Я забув як буде рамка на англійській :(
}
void output_clear() {
    if (fbUsed) {
        fb_clear(BLACK);
        return;
    }
    if (vgaUsed) {
        memset(vgaBuff,0,80*25*2);
        vga_x  = vga_y = 0;
    }
}
static uint16_t vga_getPos() {
     uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;
    return pos;
}
static void update_cursor(int x, int y)
{
	uint8_t attributeByte = (fg << 4) | (bg & 0x0f);
	 uint16_t attrib = attributeByte << 8;
	 uint16_t *location = vgaBuff + (y * 80 + x);
	 *location = 0x20 | attrib;
}
static void disable_cursor()
{
	outb(0x3D4,0x0A);
	outb(0x3D5,0x20);
}
static void draw_ramka() {
	// Малюємо рамку для красивого та стильного вивіду :)
	// Бокова сторона
	// Колір: блакитний, як у старих та класичних BIOS :)
	bg = BLUE;
	for (int i = 0; i < 25; i++) {
		vga_putchar('|',0,i);
		vga_putchar('|',79,i);
	}
	for (int i = 0; i < 80; i++) {
		vga_putchar('-',i,0);
		vga_putchar('-',i,24);
	}
	//int tmp_x = vga_x++;
	//int tmp_y = vga_y++;
	// Напишемо назву ядра!
	vga_x = 35;
	vga_y = 0;
	disableCursor = true;
	output_write("HelinKern");
	bg = BLACK;
	vga_x = 1;
	vga_y = 1;
	disableCursor = false;
	//while(1) {}
}
bool output_secondary_avail() {
    return (fbUsed || vgaUsed ? 1 : 0);
}
