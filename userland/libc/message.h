#ifndef MESSAGE_H
#define MESSAGE_H
/* Sync structure betwen kernel and userland sources */
typedef struct message
{
    int type;
    int len;
    void *message;
} msg_t;
/* Send message to a process
Return values are:
0 - Send okey
1 - Send message limit
2 - No such process
 */
int msg_send(int to,void *message);
msg_t *msg_receive();
#endif
