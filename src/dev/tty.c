#include <mm/alloc.h>
#include <vfs.h>
#include <tty.h>
#include <dev.h>
#include <output.h>
#include <lib/string.h>

// TODO: Add also the virtual TTY devices and the pipelines support

static dev_t *dev;
static vfs_node_t *keyboard; // for reading the input
static uint64_t tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static uint64_t tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff);
static uint64_t tty_ioctl(vfs_node_t *node,int request,va_list args);
static bool tty_isReady(vfs_node_t *node);
struct termios curState;
extern bool disableOutput;
extern bool userWrite;
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
	curState.c_lflag = FLAG_ECHO;
	// Find keyboard in our devfs
	dev_t *kbd = dev_find("keyboard");
    	if (kbd != NULL) {
        	keyboard = kbd->devNode;
    	} else {
		kbd = dev_find("serial");
		if (kbd == NULL) {
			kprintf("%s: warrning: no way to read user input!\r\n",__func__);
		}
		keyboard = kbd->devNode;
	}
}

static uint64_t tty_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	char *char_buff = (char *)buff;
	if ((curState.c_lflag & FLAG_ECHO) != FLAG_ECHO) {
		return size;
	}
	for (uint64_t i = 0; i < size; i++) {
    		userWrite = true;
		output_putc(char_buff[i]);
	}
  	userWrite = false;
  	return size;
}

static uint64_t tty_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (!keyboard) return 0;
	return vfs_read(keyboard,offset,size,buff);
}

static uint64_t tty_ioctl(vfs_node_t *node,int request,va_list args) {
	void *arg = va_arg(args,void *);
	switch(request) {
		case 1:
			// get
			memcpy(arg,&curState,sizeof(struct termios));
			break;
		case 2:
			// Set flags
			memcpy(&curState,arg,sizeof(struct termios));
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
int tty_getFlags() {
	return curState.c_lflag;
}
