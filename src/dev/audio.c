/*
* Generic audio kernel driver.
* Implements HelinKern Audio Layer API.
*/
#include <dev.h>
#include <dev/audio.h>
#include <vfs.h>
#include <mm/alloc.h>
#include <lib/string.h>
static char devID = '0';
/* User-space ioctl structures */
typedef struct _user_dev_info {
	char driver_name[100];
} DriverInfo;
static uint64_t snd_dev_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
	AudioDevice *dev = (AudioDevice *)node->device;
	int written = 0;
	while(1) {
		uint32_t avail = queueSize_get_free(dev->dataQueue);
		if (avail > 0) {
			written = queueSize_enqueue(dev->dataQueue,(uint8_t*)buffer,(uint32_t)size);
			break;
		}
	}
	return written;
}
static void snd_dev_close(vfs_node_t *node) {
	AudioDevice *dev = (AudioDevice *)node->device;
	if (dev->playing) {
		dev->driver->stop();
		dev->playing = 0;
	}
}
static uint64_t snd_dev_ioctl(vfs_node_t *node,int req,va_list args) {
	AudioDevice *dev = (AudioDevice *)node->device;
	switch(req) {
		case SND_IOCTL_PLAY:
			if (dev->playing) return -3;
			dev->driver->play(dev);
			dev->playing = 1;
			break;
		case SND_IOCTL_DEVINFO: {
			DriverInfo *drv = (DriverInfo *)va_arg(args,DriverInfo *);
			strcpy(drv->driver_name,dev->driver->name);
		} break;
		case SND_IOCTL_SET_VOLUME: {
			int who = (int)va_arg(args,int);
			int vol = (int)va_arg(args,int);
			return dev->driver->setVolume(who,vol);
		} break;
	}
	return -1;
}
void snd_init() {
}
void snd_registerDevice(AudioDevice *dev) {
	// Allocate new device.
	dev_t *device = kmalloc(sizeof(dev_t));
	memset(device,0,sizeof(dev_t));
	device->name = strdup("sndN");
	device->name[3] = devID++;
	device->write = snd_dev_write;
	device->ioctl = snd_dev_ioctl;
	device->close = snd_dev_close;
	device->device = dev;
	dev_add(device);
}
