#ifndef DEV_SOCKET_H
#define DEV_SOCKET_H

#include <typedefs.h>
#include <vfs.h>

#ifndef SOCK_STREAM
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#endif

#define PF_LOCAL 1
#define PF_UNIX         PF_LOCAL
#define PF_FILE         PF_LOCAL


typedef uint16_t sa_family_t;
typedef uint32_t socklen_t;
typedef uint32_t ssize_t;

struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};

typedef struct _socket {
	vfs_node_t *node; // unix socket
	bool discon;
	struct _socket *conn;
	uint32_t domain;
	void (*socket)(struct _socket* socket);
	int (*addrlen)(struct _socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int (*ifunc)(struct _socket* socket, int, int);
	int (*iaddrlen)(struct _socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	ssize_t (*send)(struct _socket* socket, int sockfd, const void *buf, size_t len, int flags);
	ssize_t (*recv)(struct _socket* socket, int sockfd, void *buf, size_t len, int flags);
} Socket;

void socket_init();

#endif

