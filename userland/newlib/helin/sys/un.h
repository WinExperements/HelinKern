#ifndef SYS_UN_H
#define SYS_UN_H

#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef	_SA_FAMILY_T_DEFINED_
#define	_SA_FAMILY_T_DEFINED_
typedef	__sa_family_t	sa_family_t;	/* sockaddr address family type */
#endif

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
	unsigned char	sun_len;	/* sockaddr len excluding NUL */
	sa_family_t	sun_family;	/* AF_UNIX */
	char	sun_path[104];		/* path name (gag) */
};

#endif
