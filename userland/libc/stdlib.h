#ifndef STDLIB_H
#define STDLIB_H
int getpid();
void exit(int exitcode);
void *malloc(int size);
void free(void *p);
char *getenv(const char *name);
#endif
