#include "stdio.h"
#include "stdlib.h" // for malloc and free
#include "syscall.h"
#include "string.h"
#include "stdarg.h"
char __stdio_buf[200];
/* Print string on screen */
void printf(const char *message,...) {
	va_list l;
	va_start(l,message);
	vsprintf(__stdio_buf,message,l);
	helin_syscall(1,(int)__stdio_buf,0,0,0,0);
	va_end(l);
}
FILE *fopen(char *file,char *mode) {
	if (!file) return NULL;
	// now lests use our syscall
	int flags = 0;
	if (mode[0] == 'r') {
		flags = 6;
	} else if (!strcmp(mode,"rw") || mode[0] == 'w') flags = 7;
	return (FILE *)helin_syscall(7,(int)file,flags,0,0,0);
}
int fwrite(char *buff,int size,int how,FILE *f) {
	int ret = 0;
	int count = size*how;
	ret = helin_syscall(10,(int)f,0,count,(int)buff,0);
	return ret;
}
int fclose(FILE *fi) {
	return helin_syscall(8,(int)fi,0,0,0,0);
}
int fread(void *buff,int size,int how,FILE *f) {
	int ret = 0;
	int count = size*how;
	ret = helin_syscall(9,(int)f,0,count,(int)buff,0);
	return ret;
}
int fseek(FILE *stream,long offset,int origin) {
	return helin_syscall(27,(int)stream,origin,offset,0,0);
}
int ftell(FILE *stream) {
	return helin_syscall(28,(int)stream,0,0,0,0);
}
