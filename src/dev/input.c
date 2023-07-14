#include <dev.h>
#include <dev/input.h>
#include <mm/alloc.h>
#include <lib/string.h>

/*
 * Input file system is just easy way to register our input device in devfs.
 * So define our methods from devfs dev_t structure, but before it
 * define our input device structure
*/

typedef struct input_device {
	char *name;
	int type;
	void *data;
	int data_size;
	struct input_device *next;
	struct input_device *prev;
} inputDevice;

static int inputfs_write(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
	// doesn't supported
	return 0;
}

static int inputfs_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
	inputDevice *d = (inputDevice *)node->device;
	if (d->data_size != 0) {
		memcpy(buf,d->data,d->data_size);
		kfree(d->data);
		d->data_size = 0;
		return d->data_size;
	}
	return 0;
}
