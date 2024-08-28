#ifndef DEV_SOCKET_H
#define DEV_SOCKET_H

#include <typedefs.h>
#include <vfs.h>
#include <lib/queue.h>
#include <lib/fifosize.h>
#ifndef SOCK_STREAM
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#endif

#define PF_LOCAL 1
#define PF_UNIX         PF_LOCAL
#define PF_FILE         PF_LOCAL

#define O_NONBLOCK	1	// Non blocking socket

typedef uint16_t sa_family_t;
typedef uint32_t socklen_t;

struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};

typedef struct _socket {
	vfs_node_t *node; // unix socket
	bool discon;
	struct _socket *conn;
	uint32_t domain;
	queue_t *acceptQueue;
	// Data queues.
	queueSize *receive;
	queueSize *sended;
	int owner; // pid of owner
	bool (*destroy)(struct _socket* socket);
	int (*bind)(struct _socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int (*listen)(struct _socket* socket, int, int);
	int (*accept)(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
 	int (*connect)(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	ssize_t (*send)(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags);
	ssize_t (*recv)(struct _socket* socket, int sockfd, void *buf, size_t len, int flags);
	bool (*isReady)(struct _socket* socket);
	void *private_data; // socket specific data, like vfs_node_t->priv_data
	int flags; // we back again.....
} Socket;

void socket_init();
void socket_add(int domain, bool (*create)(Socket *socket,int type));
bool socket_create(int domain,int type, Socket *socket);
void socket_destroy(Socket *socket);

#endif

