#include <kernel.h>
#include <output.h>
#include <arch/x86/io.h>
#include <vfs.h>
#include <dev.h>
#include <debug.h>


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
	DEBUG_N("PS/2 Initialization done\r\n");
}
