#include <output.h>
#include <arch/riscv/dev/serial.h>
#include <arch/riscv/opensbi.h>
#include <dev.h>
#include <lib/string.h>
#include <mm/alloc.h>
#include <output.h>
char sbi_readChar() {
	while(1) {
		struct SbiRet sbiC = sbi_ecall(0x02,0,0,0,0,0,0,0);
		if (sbiC.error > 0) {
			return (char)sbiC.value;
			break;
		}
	}
	return 255;
}
static uint64_t serial_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
	uint64_t i = 0;
	char *b = (char *)buffer;
	char *qemu_serial = (char *)0x10000000;
	for (i = 0; i < size; i++) {
		char c = *qemu_serial;
		while(c == 0) {
			c = *qemu_serial;
		}
		output_putc(c);
		b[i] = c;
		if (c == '\r') {
			output_putc('\n');
			b[i] = '\n';
			i++;
			b[i] = 0;
			break;
		}
	}
	return i;
}
static uint64_t serial_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
	char *b = (char *)buffer;
	for (uint64_t i = 0; i < size; i++) {
		output_putc(b[i]);
	}
	return size;
}
void riscv_serial_init() {
	// Create 'serial' device inside devfs.
	dev_t *serial = kmalloc(sizeof(dev_t));
	memset(serial,0,sizeof(dev_t));
	serial->name = "serial";
	serial->read = serial_read;
	serial->write = serial_write;
	serial->buffer_sizeMax = 500*1024;
	dev_add(serial);
}
