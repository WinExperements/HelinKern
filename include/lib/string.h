#ifndef STRING_H
#define STRING_H
#include <typedefs.h>
void memcpy(void *vd, const void *vs, unsigned length);
bool strcmp(char *s1,char *s2);
bool strcpy(char *dest,const char *src);
const char *strchr(const char *s, char ch);
char *strtok(char *s, const char *delim);
char *strdup(char *src);
int atoi(char *number);
extern char* strcat(char* s1, const char* s2);
#endif
