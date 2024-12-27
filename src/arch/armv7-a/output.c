#include <output.h>
#include <dev/fb.h>

#ifdef QEMUBOARD
#define UART0_BASE 0x09000000
#endif

static bool redirectToFb = false;

void output_uart_init() {}

static void uart_write(char c) {
      #ifdef QEMUBOARD
      *(volatile uint32_t *)(UART0_BASE) = c;
      #endif
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

void output_putc(char c) {
    if (redirectToFb) {
    	fb_putc(c);
    } else {
    	#ifdef QEMUBOARD
    	uart_write(c);
    	#endif
    }
}
void output_clear() {}
