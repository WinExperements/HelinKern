#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H


#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host */
#define	AF_LOCAL	AF_UNIX		/* draft POSIX compatibility */
#define SOCK_STREAM 	0		/* HelinOS, actually the TCP */
#define SOCK_DRAM 	1		/* HelinOS, UDP */

#include <stdint.h>
#include <sys/un.h>
#ifndef	_SOCKLEN_T_DEFINED_
#define	_SOCKLEN_T_DEFINED_
typedef	__socklen_t	socklen_t;	/* length type for network syscalls */
#endif


struct sockaddr {
        sa_family_t sa_family;
        char sa_data[14];
};


int	socket(int, int, int);
int	bind(int, const struct sockaddr *, socklen_t);
int	listen(int, int);
int	accept(int, struct sockaddr *, socklen_t *);
int connect(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen);
int send(int sockfd,const void *buf,int len,int flags); // yeah, hahaha
int recv(int sockfd,void *buf,int len,int flags);

#endif
