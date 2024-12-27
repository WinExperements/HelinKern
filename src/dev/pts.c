/*
 * HelinKern pseudo-terminal driver.
 * It manages of creating virtual TTY devices as long as
 * emulating all ioctl`s of real TTY device defined in 
 * tty.c.
 *
*/
#include <output.h>
#include <mm/alloc.h>
#include <vfs.h>
#include <dev.h>
#include <dev/pts.h>
#include <lib/string.h>
#include <lib/fifosize.h>

typedef struct _pts {
	queueSize *data;	// Data queue where we write our temporarly data.
	int ownerPID;
	struct _pts *conn;	// This is dual-side connection.
} PtsData;

static uint64_t pts_ioctl(vfs_node_t *node,int req,va_list args);
static uint64_t ptsfs_write(vfs_node_t *node,uint64_t off,uint64_t size,void *data);
static uint64_t ptsfs_read(vfs_node_t *node,uint64_t off,uint64_t size,void *data);
static void ptsfs_close(vfs_node_t *node);
static vfs_fs_t *ptsfs;
void pts_init() {
	dev_t *d = kmalloc(sizeof(dev_t));
	memset(d,0,sizeof(dev_t));
	d->name = "ptmx";
	d->ioctl = pts_ioctl;
	// Initialize PTS FS.
	ptsfs = kmalloc(sizeof(vfs_fs_t));
	memset(ptsfs,0,sizeof(vfs_fs_t));
	ptsfs->fs_name = "ptsfs";
	ptsfs->write = ptsfs_write;
	ptsfs->read = ptsfs_read;
	ptsfs->close = ptsfs_close;
	dev_add(d);
}
static uint64_t pts_ioctl(vfs_node_t *node,int req,va_list args) {
	process_t *caller = thread_getThread(thread_getCurrent());
	switch (req) {
		case 1: {
				// create device.
				vfs_node_t *nd = kmalloc(sizeof(vfs_node_t));
				memset(nd,0,sizeof(vfs_node_t));
				PtsData *oneWay = kmalloc(sizeof(PtsData));
				memset(oneWay,0,sizeof(PtsData));
				oneWay->ownerPID = caller->pid;
				oneWay->data = queueSize_create(80*25);
				PtsData *slave = kmalloc(sizeof(PtsData));
				memset(slave,0,sizeof(PtsData));
				slave->data = queueSize_create(80*25);
				slave->conn = oneWay;
				slave->ownerPID = caller->pid;
				oneWay->conn = slave;
				nd->priv_data = oneWay;
				nd->fs = ptsfs;
				nd->mask |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
				// We need to create secondary device. Currently only pts0, as we forgot how to count....
				dev_t *ptsz = kmalloc(sizeof(dev_t));
				memset(ptsz,0,sizeof(dev_t));
				ptsz->name = "pts0";
				ptsz->write = ptsfs_write; // hack! :)
				ptsz->read = ptsfs_read;
				ptsz->close = ptsfs_close;
				ptsz->device = slave;
				dev_add(ptsz);
				return thread_openFor(caller,nd,FD_RDWR);
		} break;
	}
	return -1;
}
static uint64_t ptsfs_write(vfs_node_t *node,uint64_t off,uint64_t size,void *data) {
	PtsData *dt = (PtsData *)node->priv_data;
	if (node->fs != ptsfs) {
		// devfs.
		dt = (PtsData *)node->device;
	}
	if (dt->conn == NULL || dt->ownerPID == 0) {
		return -EBADF;
	}
	if (dt->conn->data == NULL) {
		return -EBADF;
	}
	int free = 0;
	if ((free = queueSize_get_free(dt->conn->data)) <= 0) {
		return -1;
	}
	int s = min(free,size);
	queueSize_enqueue(dt->conn->data,(uint8_t*)data,s);
	return s;
}
static uint64_t ptsfs_read(vfs_node_t *node,uint64_t off,uint64_t size,void *data) {
	PtsData *dt = NULL;
	if (node->fs != ptsfs) {
		// devfs.
		dt = (PtsData *)node->device;
	} else {
		dt = (PtsData *)node->priv_data;
	}
	int datsize = queueSize_get_size(dt->data);
	if (datsize == 0) {
		return 0;
	}
	int s = min(datsize,size);
	queueSize_dequeue(dt->data,data,s);
	return s;
}
static void ptsfs_close(vfs_node_t *node) {
	process_t *caller = thread_getThread(thread_getCurrent());
	PtsData *t = (PtsData *)node->priv_data;
	if (node->fs != ptsfs) {
		return; // nothing to do here.
	}
	if (caller->pid == t->ownerPID) {
		// Free resources used by the terminal.
		queueSize_destroy(t->data);
		// Destroy slave device.
		queueSize_destroy(t->conn->data);
		memset(t,0,sizeof(PtsData));
		memset(t->conn,0,sizeof(PtsData)); // this will cause ptsfs_read to fail(we hope so)
		kfree(t->conn);
		kfree(t);
	}
	// We can be called from DEVFS! So, we need to check if the FS of node isn't devfs, otherwise we can do worse....
	if (node->fs != ptsfs) {
		// We don't need to do more things here, as we maybe called from devfs not from ptsfs.
		// Check if the owner of the pseudo-terminal channel still exists. But we need some optimizations...
		return;
	}
	kfree(node);
}
