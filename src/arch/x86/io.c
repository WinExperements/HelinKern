#include <arch/x86/io.h>
void outb(uint32_t port,uint8_t data) {
    asm("outb %b0, %w1": :"a"(data), "Nd"(port));
}
uint8_t inb(uint32_t port) {
     uint8_t result;
     asm("inb %w1, %b0": "=a"(result):"Nd"(port));
     return result;
}
uint16_t inw(uint32_t port) {
     uint16_t result;
     asm("inw %w1, %w0": "=a"(result):"Nd"(port));
     return result;
}
void outw(uint32_t port,uint16_t data) {
     asm("outw %w0, %w1": :"a"(data), "Nd"(port));
}
void insw(uint16_t port, unsigned char *data, unsigned long size) {
     asm volatile ("rep insw" : "+D" (data), "+c" (size) : "d" (port) : "memory");
}
uint32_t inl(uint32_t port) {
     uint32_t data;
    __asm__ volatile("inl %w1, %0" : "=a" (data) : "Nd" (port));
    return data;
}
void outl(uint32_t port,uint32_t data) {
     __asm__ volatile("outl %0, %w1" : : "a" (data), "Nd" (port));
}
