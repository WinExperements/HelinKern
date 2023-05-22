#include "stdlib.h"
#include "stdio.h"
#include "syscall.h"
#include "string.h"
char **__env;
int first_page_addr = 0;
int _libc_pages_allocated = 0;
int getpid() {
	return helin_syscall(4,0,0,0,0,0);
}
void exit(int exitcode) {
	/*Before the exit, free all allocated memory */
	/*if (_libc_pages_allocated > 0) {
		helin_syscall(12,first_page_addr,_libc_pages_allocated,0,0,0);
	}*/
	helin_syscall(2,exitcode,0,0,0,0);
	for (;;) {}
}
void *malloc(int size) {
	if (size > 4096) {
		printf("malloc: Too large chunk\n");
        return NULL;
	}
	void *ret = (void *)helin_syscall(11,1,0,0,0,0);
	/*if (first_page_addr == 0) {
		first_page_addr = (int)ret;
	}
	_libc_pages_allocated++;*/
	return ret;
}
/* Need to be fixed and need to be added in program page tracking */
void free(void *address) {
	_libc_pages_allocated--;
	helin_syscall(12,(int)address,1,0,0,0);
}
char *getenv(const char *name) {
    /*int i = 0;
    char *item = malloc(strlen(__env[0]));
    if (!item) return NULL;
    strcpy(item,__env[0]);
    while(item != NULL) {
        char *token = strtok(item,"=");
        if (token != NULL) {
            if (!strcmp(token,name)) {
                return strtok(NULL,"=");
            }
        }
        item = __env[++i];
    }*/
    return NULL;
}
