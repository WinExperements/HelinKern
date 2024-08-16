/*
 * Message queue
 *
*/
#include <ipc/msg.h>
#include <lib/queue.h>
#include <lib/fifosize.h>
#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <vfs.h>
static queue_t *keysQueue;
typedef struct _msg {
	process_t *owner;
	int key;
	int connections;
	queueSize *data;
} MsgObj; // in-kernel message queue object.
// User-space communication object.
typedef struct _usrMsg {
	int key; // unique key.
	void *buffer; // data buffer.
	int reqSize;
} UserMsgObj;
static vfs_fs_t *msgFs;
// Msg obj fs(to allow auto destroy)!
void msgobj_close(vfs_node_t *node) {
	MsgObj *obj = (MsgObj *)node->priv_data;
	obj->connections--;
	if (obj->connections <= 0) {
		kprintf("DEBUG: Destroying message queue global in-kernel object!\r\n");
		queueSize_destroy(obj->data);
		kfree(obj);
		queue_remove(keysQueue,obj);
	}
	kfree(node);
}
MsgObj *msg_findByKey(int key) {
	queue_for(obj,keysQueue) {
		MsgObj *msgObj = (MsgObj *)obj->value;
		if (msgObj->key == key) {
			return msgObj;
		}
	}
	return NULL;
}
int msg_createIPC(process_t *caller,void *arg) {
	// Try to find the object asociated with this specific key.
	UserMsgObj *usr = (UserMsgObj *)arg;
	if (usr->key < 0) {
		return -1;
	}
	MsgObj *obj = msg_findByKey(usr->key);
	if (obj == NULL) {
		// Okay. Create new object?
		obj = kmalloc(sizeof(MsgObj));
		memset(obj,0,sizeof(MsgObj));
		obj->key = usr->key;
		obj->data = queueSize_create(16*1024);
		enqueue(keysQueue,obj);
	}
	// Object found or newly created. Allocate the inode to add the auto-destroying feature.
	obj->connections++;
	vfs_node_t *autoDestroy = kmalloc(sizeof(vfs_node_t));
	memset(autoDestroy,0,sizeof(vfs_node_t));
	autoDestroy->fs = msgFs;
	autoDestroy->priv_data = obj;
	return thread_openFor(caller,autoDestroy);
}
int msg_command(process_t *caller,int cmd,void *arg) {
	UserMsgObj *obj = (UserMsgObj *)arg;
	if (obj->key < 0) {
		return -1;
	}
	file_descriptor_t *fd = caller->fds[obj->key];
	if (fd == NULL || fd->node->fs != msgFs) {
		// Invalid key.
		return -1;
	}
	// Check about the data.
	if (cmd != 99 && (obj->reqSize <= 0 || obj->buffer == NULL)) {
		return -1;
	}
	MsgObj *mobj = (MsgObj *)fd->node->priv_data;
	switch(cmd) {
		case 0x1:
			// send message.
			queueSize_enqueue(mobj->data,obj->buffer,obj->reqSize);
			break;
		case 0x2:
			queueSize_dequeue(mobj->data,obj->buffer,obj->reqSize);
			break;
		default:
			return -1;
	}
	return 0;
}
int msg_destroyIPC(process_t *caller) {
	return 0;
}

void msg_queue_init() {
	// Register message queue.
	keysQueue = queue_new();
	Ipc *msg_queue = (Ipc *)kmalloc(sizeof(Ipc));
	memset(msg_queue,0,sizeof(Ipc));
	msg_queue->name = "MessageQueue";
	msg_queue->magicID = 'm';
	msg_queue->createIPC = msg_createIPC;
	msg_queue->command = msg_command;
	msg_queue->destroyIPC = msg_destroyIPC;
	ipc_add(msg_queue);
	msgFs = kmalloc(sizeof(vfs_fs_t));
	memset(msgFs,0,sizeof(vfs_fs_t));
	msgFs->fs_name = "msgFS";
	msgFs->close = msgobj_close;
}
