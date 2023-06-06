#ifndef OUTPUT_H
#define OUTPUT_H
#include <typedefs.h>
void output_uart_init();
void output_second_init();
bool output_secondary_avail();
void output_write(char *msg);
void output_writeInt(int i);
void output_printHex(int n);
void output_changeToFB();
void kprintf(char *format,...);
void putc(char c);
void output_clear();
#endif
