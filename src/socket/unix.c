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
/*
 * Update: 25/08/2024. The type field is auto set along with the SOCKET to the socket
 * structure after calling *_create function of each socket implementation
 * provider, so don't edit this field as it begin rewritten by sys_socket.
*/
bool unix_create(Socket *socket,int type) {
	// Okay, socket structure must be allocated before calling actual creating body
	// Parameter check
	// The data queue and other specific to UNIX domain socket thinks created only
	// when the endpoint connection created or the connection binded to specific
	// file node.
	if (socket == NULL) return false; // failed to create socket, clean please
	socket->sended = queueSize_create(500*1024);
//	socket->receive = queueSize_create(500*1024);
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
	return true;
}


// Destroy our socket instance
bool unix_destroy(Socket *socket) {
	if (socket == NULL) return false; // you know why
	queueSize_destroy(socket->receive);
	queueSize_destroy(socket->sended);
	//arch_sti(); ; because usually called within thread_killThread where the system calls are pretty bad thinks.
	// Free all messages until we get NULL
	while(dequeue(socket->acceptQueue));
	kfree(socket->acceptQueue);
	socket->conn = NULL;
	socket->receive = NULL;
	socket->sended = NULL;
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
        char *cp = strdup(addr->sa_data);
	if (vfs_find(cp) != NULL) return -1; // we already exists
	//kprintf("SOCKET: %s\n",addr->sa_data);
	// Create VFS entry(very stupid method!)
        kfree(cp);
        cp = strdup(addr->sa_data);
	int r = vfs_createFile(cp,VFS_SOCKET); // socket
        kfree(cp);
        cp = strdup(addr->sa_data);
        socket->node = vfs_find(cp);
        kfree(cp);
	if (socket->node == NULL) {
		// Failed to create vfs node
		return -1;
	}
        socket->node->flags = 4;
	socket->node->priv_data = socket;
	if (nanoXS == NULL) {
		nanoXS = socket->node;
	}
	// If we UDP sockets then we need to create unix socket specific data AND the buffer.
	return 0;
}
int unix_listen(struct _socket* socket, int sockfd, int backlog) {
	// Accept queue must be created by it's function, because i don't know
	socket->acceptQueue = queue_new();
	return 0;
}
int unix_accept(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	arch_sti();
//	Non block sockets doesn't wait for anything.
	bool nonBlock = (socket->flags & 16384) == 16384;
	// -1 cause the for to be endless.
	int until = 2;
	while(until > 1) {
		Socket *other = NULL;
		if (socket->acceptQueue->size > 0) {
			other = (Socket *)dequeue(socket->acceptQueue);
		}
		if (other) {
			Socket *new_socket = (Socket *)kmalloc(sizeof(Socket));
			memset(new_socket,0,sizeof(Socket));
			// Default socket type is blocking TCP, because only tcp can 'accept' connections.
			bool ret = socket_create(PF_UNIX,0,new_socket);
			new_socket->owner = socket->owner;
			if (ret) {
				// Successfully
				new_socket->conn = other;
				other->conn = new_socket;
				//kprintf("New_socket: 0x%x, other: 0x%x\n",new_socket->conn,other->conn);
				vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
				memset(n,0,sizeof(vfs_node_t));
				n->name = strdup("conn");
				n->flags = VFS_SOCKET;
				n->priv_data = new_socket;
				// Security!
				n->mask = (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
				//arch_sti();
				return thread_openFor(thread_getThread(thread_getCurrent()),n,FD_RDWR);
			}
		}
		if (nonBlock) until--; // if non block flag ISN'T set, then we act like endless loop.
	}
	return -1;
}
int unix_connect(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	// On x86 sometimes interrupts begin disabled(i don't know why)
	arch_sti();
	if (strlen(addr->sa_data) == 0) return -1;
	if (socket->conn != NULL) return -1;
        char *cp = strdup(addr->sa_data);
	vfs_node_t *node = vfs_finddir(vfs_getRoot(),cp);
        kfree(cp);
	if (!node) {
		// Maybe Nano-X?
		node = nanoXS;
	}
	if (!node) {
		// double check!
		return -1;
	}
	Socket *so = (Socket*)node->priv_data;
	if (so->acceptQueue == NULL) return -1;
	enqueue(so->acceptQueue,socket);
	// poll if we aren't non block socket.
	bool nonBlock = (socket->flags & /*O_NONBLOCK) == O_NONBLOCK;*/16384) == 16384;
	if (nonBlock) return 0;
	while(socket->conn == NULL);
	kprintf("unix: connected!\n");
	return 0;
}
ssize_t unix_send(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags) {
    if (len == 0) return -1;
    //if (!socket->conn) return 0;
    arch_sti();
    // Just enqueue the message
    bool nonBlock = (socket->flags & /*O_NONBLOCK) == O_NONBLOCK;*/16384) == 16384;
    while(1) {
	uint32_t free = queueSize_get_free(socket->sended);
	if (free > 0) {
		arch_cli();
		uint32_t sm = min(free,len);
		uint32_t b = queueSize_enqueue(socket->sended,(uint8_t *)buf,sm);
		arch_sti();
		return b;
	}
	/*
	 * Я не читав документацій як воно має робити, але на Linux, якщо вствновлений прапор NONBLOCK
	 * та у нас немає вхідних данних, тоді ми повертаємо 0.
	*/
	if (nonBlock) return 0; // все одно виконає потрібну нам операцію.
    }
    return -1;
}

ssize_t unix_recv(struct _socket* socket, int sockfd, void *buf, size_t len, int flags) {
    if (socket->discon) return 0;
    arch_sti();
    //kprintf("read start from socket\n");
    if (!socket->conn) return -1;
    bool nonBlock = (socket->flags & /*O_NONBLOCK) == O_NONBLOCK;*/16384) == 16384;
    if (socket->conn->sended == NULL) return -1;
    while (1) {
        uint32_t size = queueSize_get_size(socket->conn->sended);
        if (size > 0) {
                uint32_t sm = min(size,len);
                uint32_t readed = queueSize_dequeue(socket->conn->sended,(uint8_t *)buf,sm);
                return readed;
        }
	if (nonBlock) return 0;
    }

    return -1; // timeout? or whata helllllll
}


bool unix_isReady(struct _socket* sock) {
	if (sock->acceptQueue != NULL && sock->acceptQueue->size > 0) {
		return true;
	}
	if (sock->sended == NULL) {
		// Invalid socket? Send true, and any kind of read results of error.
		return true;
	}
	//if (data->writeSize > 0) return true;
	if (sock->conn != NULL) {
		// this is an connection.
		// If this socket is connection, then we need to check input data alternativly.
		// Check if we don't have destroyed connection.
		if (sock->conn->sended == NULL) return true; // destroyed connection.
		if (queueSize_get_size(sock->conn->sended) > 0) return true;
		return false;
	}
	if (queueSize_get_size(sock->sended) > 0) return true;
	return false;
}
