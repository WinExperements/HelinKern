#include <arch/x86/io.h>
#include <output.h>
#include <dev/fb.h>
#define PORT 0x3f8
static bool fbUsed = false;
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
        write_serial(msg[i]);
        i++;
    }
}
void output_changeToFB() {
    fbUsed = true;
}
void putc(char c) {
    if (fbUsed) {
        fb_putc(c);
        return;
    }
    write_serial(c);
}
