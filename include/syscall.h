#ifndef SYSCALL_H
#define SYSCALL_H
#define POWEROFF_MAGIC 0xfffc04 // random number
void syscall_init();
int syscall_get(int n);
#endif
