#include <output.h>


#define UART0_BASE 0x1c090000

void output_uart_init() {}

static void uart_write(char c) {
    *(volatile uint32_t *)(UART0_BASE) = c;    
}

void output_write(char *msg) {
    int i = 0;
    while(msg[i]) {
        uart_write(msg[i]);
        i++;
    }
}

void output_changeToFB() {}

void putc(char c) {
    uart_write(c);
}
void output_clear() {}
