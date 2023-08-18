#include <kernel.h>
#include <output.h>
#include <arch/x86/io.h>
#include <arch/x86/idt.h>
#include <vfs.h>
#include <dev.h>
#include <debug.h>
#include <arch.h>
#include <lib/string.h>
#include <lib/queue.h>


#define PACKETS_IN_PIPE 1024
#define DISCARD_POINT 32

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT   0x02
#define MOUSE_BBIT   0x01
#define MOUSE_WRITE  0xD4
#define MOUSE_F_BIT  0x20
#define MOUSE_V_BIT  0x08

static int mouse_cycle = 0;
static char mouse_byte[3];
static int old_x = 800;
static int old_y = 600;
static int mice_dev_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static bool mice_dev_isReady(vfs_node_t *node);
static queue_t *data_queue;
// dev/fb.c
extern void fb_putchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and 0 0s, say 0xFFFFFF and 0x000000 */
    int fg, int bg);

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

static void *mouse_intr(void *st) {
	uint8_t status = inb(MOUSE_STATUS);
	while(status & MOUSE_BBIT) {
		uint8_t mouse_in = inb(MOUSE_PORT);
        if (status & MOUSE_F_BIT) {
            switch(mouse_cycle) {
                case 0:
                mouse_byte[0] = mouse_in;
                if (!(mouse_in & MOUSE_V_BIT)) return st;
                ++mouse_cycle;
                break;
                case 1:
                mouse_byte[1] = mouse_in;
                ++mouse_cycle;
                break;
                case 2:
                mouse_byte[2] = mouse_in;
                if (mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40) {
                    // something is really wrong, but what?
                    break;
                } 
                mouse_cycle = 0;
		int x = mouse_byte[1];
		int y = mouse_byte[2];
		if (x && mouse_byte[0] & (1 << 4)) {
			/* Sign bit */
			x = x - 0x100;
		}
		if (y && mouse_byte[0] & (1 << 5)) {
			/* Sign bit */
			y = y - 0x100;
		}
		if (mouse_byte[0] & (1 << 6) || mouse_byte[0] & (1 << 7)) {
			/* Overflow */
			x = 0;
			y = 0;
		}
		void *copy = kmalloc(3);
		memcpy(copy,mouse_byte,3);
		enqueue(data_queue,copy);
                break;
            }
        }
        status = inb(MOUSE_STATUS);
	}
	return st;
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
	data_queue = queue_new();
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
    // Add device into devfs
    	dev_t *mouse = (dev_t *)kmalloc(sizeof(dev_t));
	memset(mouse,0,sizeof(dev_t));
	mouse->name = "mice";
	mouse->buffer_sizeMax = 3;
	mouse->read = mice_dev_read;
	mouse->isReady = mice_dev_isReady;
	dev_add(mouse);
	DEBUG_N("PS/2 Initialization done\r\n");
}

static int mice_dev_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (size < 3) return 0;
	if (data_queue->size == 0) return 0;
	void *data = dequeue(data_queue);
	memcpy(buff,data,3);
	kfree(data);
	return 3;
}

static bool mice_dev_isReady(vfs_node_t *node) {
	return data_queue->size != 0;
}
