#include <typedefs.h>
#include <dev.h>
#include <output.h>
#include <mm/alloc.h>
static int serialdev_write(vfs_node_t *,uint64_t offset,uint64_t how,void *buff);
static int serialdev_read(vfs_node_t *,uint64_t,uint64_t,void *);
extern void write_serial(char c);
extern char read_serial();
void serialdev_init() {
	kprintf("Initializing X86 serial device...");
	dev_t *serialdev = (dev_t *)kmalloc(sizeof(dev_t));
	memset(serialdev,0,sizeof(dev_t));
	serialdev->name = "serial";
	serialdev->write = serialdev_write;
	serialdev->read = serialdev_read;
	dev_add(serialdev);
	kprintf("done\r\n");
}

static int serialdev_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (size < 0) return 0;
	char *b = (char *)buff;
	for (uint64_t i = 0; i < size; i++) {
		putc(b[i]);
	}
	return size;
}
static int serialdev_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (size < 0) return 0;
	kprintf("Reading from serial device\r\n");
	char *b = (char *)buff;
	uint64_t i;
	for (i = 0; i < size; i++) {
		char c = read_serial();
		output_putc(c);
		b[i] = c;
		if (c == '\n') {
			break;
		}
	}
	return i;
}
