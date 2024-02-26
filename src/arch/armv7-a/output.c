#include <output.h>
#include <dev/fb.h>

static bool redirectToFb = false;

void output_uart_init() {}

static void uart_write(char c) {
      
}

void output_write(char *msg) {
   int i = 0;
    while(msg[i]) {
        putc(msg[i]);
        i++;
    }
}

void output_changeToFB() {
	redirectToFb = true;
}

void putc(char c) {
    if (redirectToFb) {
    	fb_putc(c);
    }
}
void output_clear() {}
