/* Helin RISC-V specific port? */
#include <output.h>
#define UART_BASE 0x10000000
void output_uart_init() {}
void output_write(char *msg) {
  char *b = (char *)UART_BASE;
  while(*msg) {
    *b = *msg;
    msg++;
  } 
}
void putc(char c) {
  *((char *)UART_BASE) = c;
}
void output_changeToFB() {}
void output_clear() {}
