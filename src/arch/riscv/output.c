/* Helin RISC-V specific port? */
#include <output.h>
#include <arch/riscv/opensbi.h>
#define UART_BASE 0x10000000
void output_uart_init() {}
void output_write(char *msg) {
  char *uart = (char *)UART_BASE;
  while(*msg) {
    *uart = *msg;
    msg++;
  } 
}
void output_putc(char c) {
	 *((char *)UART_BASE) = c;
}
void output_changeToFB() {}
void output_clear() {}
