#include <mm/alloc.h>
#include <vfs.h>
#include <tty.h>
#include <dev.h>
#include <output.h>

static dev_t *dev;
static vfs_node_t *keyboard; // for reading the input
static int tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static int tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);

void tty_init() {
	dev = kmalloc(sizeof(dev_t));
	memset(dev,0,sizeof(dev_t));
	dev->name = "tty";
	dev->buffer_sizeMax = 400;
	dev->write = tty_write;
	dev->read = tty_read;
	dev_add(dev);
	// Find keyboard in our devfs
	keyboard = vfs_find("/dev/keyboard");
}

static int tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	char *char_buff = (char *)buff;
	for (int i = 0; i < size; i++) {
		putc(char_buff[i]);
	}
    return size;
}

static int tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (!keyboard) return 0;
	return vfs_read(keyboard,offset,size,buff);
}
