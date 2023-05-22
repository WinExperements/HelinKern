/*
 * This file implements the waitpid function needed for execuiton programs from shells
*/
#include "syscall.h"
#include "wait.h"
pid_t waitpid(pid_t pid,int *status,int options) {
    return helin_syscall(22,pid,(int)status,options,0,0);
}
