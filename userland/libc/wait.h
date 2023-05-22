#ifndef WAIT_H
#define WAIT_H
typedef int pid_t;
pid_t waitpid(pid_t pid,int *status,int options);
#endif
