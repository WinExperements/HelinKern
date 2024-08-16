#ifndef SYS_MSG_H
#define SYS_MSG_H

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
#include <sys/ipc.h>
typedef int msgqnum_t;
typedef int msglen_t;
struct msqid_ds {
	struct ipc_perm msg_perm;
	msgqnum_t       msg_qnum;
	msglen_t        msg_qbytes;
	pid_t           msg_lspid;
	pid_t           msg_lrpid;
	time_t          msg_stime;
	time_t          msg_rtime;
	time_t          msg_ctime;
};
int       msgctl(int msgQueueID, int cmd, struct msqid_ds *fromOrTo);
int       msgget(key_t key, int flags);
ssize_t   msgrcv(int msgQueueID, void *buffer, size_t size, long msgType, int msgFlags);
int       msgsnd(int msgQueueID, const void *buffer, size_t size, int msgFlags);
#ifdef __cplusplus
}
#endif
#endif
