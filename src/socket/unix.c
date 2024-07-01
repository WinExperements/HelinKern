/*
 * Unix socket implementation for HelinKern
 *
 * Status of code: not tested
*/
// Some includes
#include <output.h> // for errors and other stuff
#include <mm/alloc.h> // yeah we need some memory to work
#include <vfs.h> // did you remember that we works using file?
#include <dev/socket.h> // main header
#include <lib/fifosize.h> // messages and other stuff
#include <socket/unix.h> // our main body
#include <arch.h>
#include <lib/string.h>
// Define our specific structure
typedef struct _unix {
	queueSize *data;
	int writeSize;
} UnixSocketData;
// Define methods that we gonna use
bool unix_destroy(Socket *);
int unix_bind(struct _socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int unix_listen(struct _socket* socket, int sockfd, int backlog);
int unix_accept(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t unix_send(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags);
ssize_t unix_recv(struct _socket* socket, int sockfd, void *buf, size_t len, int flags);
int unix_connect(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
bool unix_isReady(struct _socket *socket);
static vfs_node_t *nanoXS; // TEST!
/* 
 * Create new UNIX socket instance.
 * Usually called by sys_socket via socket methods that used when we register our sockets into one database
 * We need to allocate the socket structure and our socket specific data, then we need to fill it and now we can return it user
*/
bool unix_create(Socket *socket) {
	// Okay, socket structure must be allocated before calling actual creating body
	// Parameter check
	if (socket == NULL) return false; // failed to create socket, clean please
	UnixSocketData *data = (UnixSocketData *)kmalloc(sizeof(UnixSocketData));
	memset(data,0,sizeof(UnixSocketData));
	// Check if allocation is successfull
	if (data == NULL || (int)data < 0) return false;
	data->data = queueSize_create(500*1024);
	// Now fill up the given socket structure
	socket->domain = PF_UNIX;
	socket->destroy = unix_destroy;
	socket->bind = unix_bind;
	socket->listen = unix_listen;
	socket->accept = unix_accept;
	socket->send = unix_send;
	socket->recv = unix_recv;
	socket->connect = unix_connect;
	socket->isReady = unix_isReady;
	// Create the node? Nah, it's must be created before calling us
	socket->private_data = data;
	return true;
}


// Destroy our socket instance
bool unix_destroy(Socket *socket) {
	if (socket == NULL || socket->private_data == NULL) return false; // you know why
	UnixSocketData *pr = (UnixSocketData *)socket->private_data;
//	arch_sti();
	// Free all messages until we get NULL
	kprintf("Size of queue -> %d\r\n",socket->acceptQueue->size);
	//while(dequeue(socket->acceptQueue));
	queueSize_destroy(pr->data);
	kfree(pr->data); // free queue
	kfree(socket->private_data);
	kfree(socket->acceptQueue);
	// Node are destroyed after this method, so we done.
	return true;
}

// Our main header function
void unix_register() {
	socket_add(PF_UNIX,unix_create);
	kprintf("UNIX sockets added successfully\n"); // Actually we lie a liitle bit, because it's can return errors sometimes
}

// Each socket methods
int unix_bind(struct _socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	if (vfs_find(addr->sa_data) != NULL) return -1; // we already exists
	//kprintf("SOCKET: %s\n",addr->sa_data);
	// Create VFS entry
	socket->node = vfs_creat(vfs_getRoot(),addr->sa_data,3); // socket
	if (socket->node == NULL) {
		// Failed to create vfs node
		return -1;
	}
        socket->node->flags = 4;
	socket->node->priv_data = socket;
	if (nanoXS == NULL) {
		nanoXS = socket->node;
	}
	return 0;
}
int unix_listen(struct _socket* socket, int sockfd, int backlog) {
	// Accept queue must be created by it's function, because i don't know
	socket->acceptQueue = queue_new();
	return 0;
}
int unix_accept(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
//	arch_sti(); // for any case
	bool nonBlock = socket->flags & O_NONBLOCK ? 1 : 0;
	// -1 cause the for to be endless.
	int until = 2;
	while(until > 1) {
		Socket *other = NULL;
		if (socket->acceptQueue->size > 0) {
			other = (Socket *)dequeue(socket->acceptQueue);
		}
		if (other) {
			arch_cli();
			Socket *new_socket = (Socket *)kmalloc(sizeof(Socket));
			memset(new_socket,0,sizeof(Socket));
			bool ret = socket_create(PF_UNIX,new_socket);
			new_socket->owner = socket->owner;
			if (ret) {
				// Successfully
				new_socket->conn = other;
				other->conn = new_socket;
				//kprintf("New_socket: 0x%x, other: 0x%x\n",new_socket->conn,other->conn);
				vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
				memset(n,0,sizeof(vfs_node_t));
				strcpy(n->name,"conn");
				n->flags = 4;
				n->priv_data = new_socket;
				// Security!
				n->mask = (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
				//arch_sti();
				return thread_openFor(thread_getThread(thread_getCurrent()),n);
			}
		}
		if (nonBlock) until--; // if non block flag ISN'T set, then we act like endless loop.
	}
	return -1;
}
int unix_connect(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	arch_sti();
	if (strlen(addr->sa_data) == 0) return -1;
	if (socket->conn != NULL) return -1;
	vfs_node_t *node = vfs_finddir(vfs_getRoot(),addr->sa_data);
	if (!node) {
		// Maybe Nano-X?
		node = nanoXS;
	}
	Socket *so = (Socket*)node->priv_data;
	if (so->acceptQueue == NULL) return -1;
	enqueue(so->acceptQueue,socket);
	if ((!socket->flags & O_NONBLOCK)) {while(socket->conn == NULL);}
	return 0;
}
ssize_t unix_send(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags) {
    if (len == 0) return -1;
    if (!socket->conn) return 0;
    arch_sti();
    // Just enqueue the message
    UnixSocketData *data = (UnixSocketData *)socket->conn->private_data;
    if (!data || !data->data || !buf) return len;
    while(1) {
	arch_cli();
	uint32_t free = queueSize_get_free(data->data);
	if (free > 0) {
		uint32_t sm = min(free,len);
		data->writeSize+=sm;
		uint32_t b = queueSize_enqueue(data->data,(uint8_t *)buf,sm);
		arch_sti();
		return b;
	}
        arch_sti();
    }
    return -1;
}

ssize_t unix_recv(struct _socket* socket, int sockfd, void *buf, size_t len, int flags) {
    if (socket->discon) return 0;
    arch_sti();
    //kprintf("read start from socket\n");
    UnixSocketData *data = (UnixSocketData *)socket->private_data;
    if (!data || !data->data) return 0;
    while (1) {
        arch_cli();
        uint32_t size = queueSize_get_size(data->data);
        if (size > 0) {
                uint32_t sm = min(size,len);
		data->writeSize-=sm;
                uint32_t readed = queueSize_dequeue(data->data,(uint8_t *)buf,sm);
                arch_sti();
                return readed;
        }
        arch_sti();
    }

    return 0;
}


bool unix_isReady(struct _socket* sock) {
	UnixSocketData *data = (UnixSocketData *)sock->private_data;
        if (!data) return false;
	if (!data->data) return false;
	if (sock->acceptQueue != NULL && sock->acceptQueue->size > 0) {
		return true;
	}
	//if (data->writeSize > 0) return true;
	if (queueSize_get_size(data->data) > 0) return true;
	return false;
}
