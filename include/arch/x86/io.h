#ifndef IO_H
#define IO_H
#include <typedefs.h>
void outb(uint32_t port,uint8_t data);
uint8_t inb(uint32_t port);
void outw(uint32_t port,uint16_t data);
uint16_t inw(uint32_t port);
void insw(uint16_t port, unsigned char *data, unsigned long size);
#endif
