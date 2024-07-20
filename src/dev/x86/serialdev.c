#include <typedefs.h>
#include <dev.h>
#include <output.h>
#include <arch.h>
#include <mm/alloc.h>
static uint64_t serialdev_write(vfs_node_t *,uint64_t offset,uint64_t how,void *buff);
static uint64_t serialdev_read(vfs_node_t *,uint64_t,uint64_t,void *);
extern void write_serial(char c);
extern char read_serial();
void serialdev_init() {
	kprintf("Initializing X86 serial device...");
	dev_t *serialdev = (dev_t *)kmalloc(sizeof(dev_t));
	memset(serialdev,0,sizeof(dev_t));
	serialdev->name = "serial";
	serialdev->write = serialdev_write;
	serialdev->read = serialdev_read;
	serialdev->buffer_sizeMax = 100;
	dev_add(serialdev);
	kprintf("done\r\n");
}

static uint64_t serialdev_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (size < 0) return 0;
	char *b = (char *)buff;
	for (uint64_t i = 0; i < size; i++) {
		write_serial(b[i]);
	}
	return size;
}
static uint64_t serialdev_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (size < 0) return 0;
	char *b = (char *)buff;
	arch_sti();
	uint64_t i;
	for (i = 0; i < size; i++) {
		char c = read_serial();
		write_serial(c);
		b[i] = c;
		if (c == '\r') {
			write_serial('\n');
			b[i] = '\n';
			i++;
			b[i] = 0;
			return i;
		} else if (c == 127) {
			if (i > 2) {
				write_serial('\b');
				write_serial(' ');
				i-=2;
				b[i] = 0;
			} else if (i > 0) {
				b[i] = 0;
				i--;
				b[i] = 0;
				write_serial('\b');
				write_serial(' ');
			} else {
				write_serial('\b');
				b[0] = 0;
			}
		}
	}
	return i;
}
