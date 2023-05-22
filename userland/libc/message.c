#include "message.h"
#include "stdio.h"
#include "stdlib.h"
int msg_send(int to,void *message) {
    int ret = 0;
    asm volatile("int $0x80" : "=a" (ret) :  "0" (5), "b" (to),"g" ((int)message));
    return ret;
}
msg_t *msg_receive() {
    msg_t *msg = NULL;
    asm volatile("int $0x80" : "=a" (msg) : "0" (6),"b" (getpid()));
    return msg;
}
