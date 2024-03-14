#include <mm/alloc.h>
#include <vfs.h>
#include <tty.h>
#include <dev.h>
#include <output.h>


// TODO: Add also the virtual TTY devices and the pipelines support

static dev_t *dev;
static vfs_node_t *keyboard; // for reading the input
static int tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static int tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static int tty_ioctl(vfs_node_t *node,int request,va_list args);
static bool tty_isReady(vfs_node_t *node);
int tty_flags = FLAG_ECHO;
extern bool disableOutput;

void tty_init() {
	dev = kmalloc(sizeof(dev_t));
	memset(dev,0,sizeof(dev_t));
	dev->name = "tty";
	dev->buffer_sizeMax = 400;
	dev->write = tty_write;
	dev->read = tty_read;
	dev->ioctl = tty_ioctl;
	dev->isReady = tty_isReady;
	dev_add(dev);
	// Find keyboard in our devfs
	dev_t *kbd = dev_find("keyboard");
    if (kbd != NULL) {
        keyboard = kbd->devNode;
    }
}

static int tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	char *char_buff = (char *)buff;
	if (!(tty_flags & FLAG_ECHO)) return size;
	for (uint64_t i = 0; i < size; i++) {
		putc(char_buff[i]);
	}
    	return size;
}

static int tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (!keyboard) return 0;
	return vfs_read(keyboard,offset,size,buff);
}

static int tty_ioctl(vfs_node_t *node,int request,va_list args) {
	int *arg = va_arg(args,int *);
	switch(request) {
		case 1:
			*arg = tty_flags;
			break;
		case 2:
			// Set flags
			tty_flags = *arg;
			break;
		case 3:
			output_clear();
			break;
		default:
			return -1;
	}
	return 0;
}
static bool tty_isReady(vfs_node_t *node) {
	return keyboard->fs->isReady(keyboard);
}
