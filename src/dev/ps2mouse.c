#include <kernel.h>
#include <output.h>
#include <arch/x86/io.h>
#include <arch/x86/idt.h>
#include <vfs.h>
#include <dev.h>
#include <debug.h>
#include <arch.h>



#define PACKETS_IN_PIPE 1024
#define DISCARD_POINT 32

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT   0x02
#define MOUSE_BBIT   0x01
#define MOUSE_WRITE  0xD4
#define MOUSE_F_BIT  0x20
#define MOUSE_V_BIT  0x08

void mouse_wait(uint8_t a_type) {
	uint32_t timeout = 100000;
	if (!a_type) {
		while (--timeout) {
			if ((inb(0x64) & 0x01) == 1) {
				return;
			}
		}
		DEBUG_N("mouse timeout\r\n");
		return;
	} else {
		while (--timeout) {
			if (!((inb(0x64) & 0x02))) {
				return;
			}
		}
		DEBUG_N("mouse timeout\r\n");
		return;
	}
}

static void *mouse_intr(void *stack) {
    kprintf("Mouse interrupt!\r\n");
	return stack;
}

void mouse_write(uint8_t write) {
	mouse_wait(1);
	outb(MOUSE_STATUS, MOUSE_WRITE);
	mouse_wait(1);
	outb(MOUSE_PORT, write);
}

uint8_t mouse_read() {
	mouse_wait(0);
	char t = inb(MOUSE_PORT);
	return t;
}

void ps2mouse_init() {
	DEBUG_N("Initializing PS/2 mouse\r\n");
	outb(0x64,0xA8);
	mouse_wait(1);
	outb(0x64,0x20);
	mouse_wait(0);
	uint8_t status = inb(0x60) | 2;
	mouse_wait(1);
	outb(0x64,0x60);
	mouse_wait(1);
	outb(MOUSE_PORT, status);
	mouse_write(0xF6);
	mouse_read();
	mouse_write(0xF4);
	mouse_read();
    	interrupt_add(IRQ12,mouse_intr);
	DEBUG_N("PS/2 Initialization done\r\n");
}
