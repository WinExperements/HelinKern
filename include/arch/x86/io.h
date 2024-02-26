#ifndef IO_H
#define IO_H
#include <typedefs.h>
void outb(uint16_t port,uint8_t data);
uint8_t inb(uint16_t port);
void outw(uint32_t port,uint16_t data);
uint16_t inw(uint32_t port);
void insw(uint16_t port, unsigned char *data, unsigned long size);
uint32_t inl(uint32_t port);
void outl(uint32_t port,uint32_t data);
void outsw(uint16_t port,unsigned char *data,unsigned long size);
#endif
