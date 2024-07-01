// Inter-Process communication :)
// Handle C++
#ifndef SYS_IPC_H
#define SYS_IPC_H
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
#define IPC_CREAT	10
#define IPC_EXCL	11
#define IPC_NOWAIT	12
#define IPC_RMID	99
#define IPC_SET		5
#define IPC_STAT	3
#define IPC_PRIVATE	0
struct ipc_perm {
	uid_t uid;
	gid_t gid;
	uid_t cuid;
	gid_t cgid;
	mode_t mode;
};
key_t  ftok(const char *path, int id);
#ifdef __cplusplus
}
#endif
#endif
