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
#include <lib/queue.h> // messages and other stuff
#include <socket/unix.h> // our main body
#include <arch.h>
#include <lib/string.h>
// Define our specific structure
typedef struct _unix {
	queue_t *data;
} UnixSocketData;
typedef struct _unixData {
	int len;
	void *ptr;
} UnixClientData;
// Define methods that we gonna use
int unix_destroy(Socket *);
int unix_bind(struct _socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int unix_listen(struct _socket* socket, int sockfd, int backlog);
int unix_accept(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t unix_send(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags);
ssize_t unix_recv(struct _socket* socket, int sockfd, void *buf, size_t len, int flags);
int unix_connect(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);


/* 
 * Create new UNIX socket instance.
 * Usually called by sys_socket via socket methods that used when we register our sockets into one database
 * We need to allocate the socket structure and our socket specific data, then we need to fill it and now we can return it user
*/
int unix_create(Socket *socket) {
	// Okay, socket structure must be allocated before calling actual creating body
	// Parameter check
	if (socket == NULL) return false; // failed to create socket, clean please
	UnixSocketData *data = (UnixSocketData *)kmalloc(sizeof(UnixSocketData));
	// Check if allocation is successfull
	if (data == NULL || (int)data < 0) return false;
	data->data = queue_new();
	// Now fill up the given socket structure
	socket->domain = PF_UNIX;
	socket->destroy = unix_destroy;
	socket->bind = unix_bind;
	socket->listen = unix_listen;
	socket->accept = unix_accept;
	socket->send = unix_send;
	socket->recv = unix_recv;
	socket->connect = unix_connect;
	// Create the node? Nah, it's must be created before calling us
	socket->private_data = data;
	return true;
}


// Destroy our socket instance
int unix_destroy(Socket *socket) {
	if (socket == NULL || socket->private_data == NULL) return false; // you know why
	UnixSocketData *pr = (UnixSocketData *)socket->private_data;
	// Free all messages until we get NULL
	while(dequeue(socket->acceptQueue));
	while(dequeue(pr->data) != NULL);
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
	kprintf("SOCKET: %s\n",addr->sa_data);
	// Create VFS entry
	socket->node = vfs_creat(vfs_getRoot(),addr->sa_data,3); // socket
	if (socket->node == NULL) {
		// Failed to create vfs node
		return -1;
	}
	socket->node->priv_data = socket;
	return 0;
}
int unix_listen(struct _socket* socket, int sockfd, int backlog) {
	// Accept queue must be created by it's function, because i don't know
	socket->acceptQueue = queue_new();
	return 0;
}
int unix_accept(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	arch_sti(); // for any case
	while(1) {
		Socket *other = NULL;
		if (socket->acceptQueue->size > 0) {
			other = (Socket *)dequeue(socket->acceptQueue);
		}
		if (other) {
			Socket *new_socket = (Socket *)kmalloc(sizeof(Socket));
			memset(new_socket,0,sizeof(Socket));
			bool ret = socket_create(PF_UNIX,new_socket);
			if (ret) {
				// Successfully
				new_socket->conn = other;
				other->conn = new_socket;
				kprintf("New_socket: 0x%x, other: 0x%x\n",new_socket->conn,other->conn);
				vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
				memset(n,0,sizeof(vfs_node_t));
				strcpy(n->name,"conn");
				n->priv_data = new_socket;
				return thread_openFor(thread_getThread(thread_getCurrent()),n);
			}
		}
	}
	return -1;
}
int unix_connect(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	arch_sti();
	if (strlen(addr->sa_data) == 0) return -1;
	if (socket->conn != NULL) return -1;
	vfs_node_t *node = vfs_find(addr->sa_data);
	if (!node) return -1;
	Socket *so = (Socket*)node->priv_data;
	if (so->acceptQueue == NULL) return -1;
	enqueue(so->acceptQueue,socket);
	while(socket->conn == NULL);
	return 0;
}
ssize_t unix_send(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags) {
	if (!socket->conn) return -1;
	// Just enqueue the message
	UnixSocketData *data = (UnixSocketData *)socket->conn->private_data;
	void *copy = kmalloc(len);
	memcpy(copy,buf,len);
	UnixClientData *cl = (UnixClientData *)kmalloc(sizeof(UnixClientData));
	memset(cl,0,sizeof(UnixClientData));
	cl->len = len;
	cl->ptr = copy;
	enqueue(data->data,cl);
	kprintf("%s: done\n",__func__);
	return 0;
}
ssize_t unix_recv(struct _socket* socket, int sockfd, void *buf, size_t len, int flags) {
	if (socket->discon) return -1;
	UnixSocketData *data = (UnixSocketData *)socket->private_data;
	while(data->data->size == 0); // wait for any data before receiving, yeah maybe it's very stupid to do
	UnixClientData *sock_data = dequeue(data->data);
	memcpy(buf,sock_data->ptr,sock_data->len);
	kfree(sock_data->ptr);
	kfree(sock_data);
	return 0;
}
