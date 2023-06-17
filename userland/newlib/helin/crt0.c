#include <fcntl.h>

extern void exit(int code);
extern int main(int argc,char **argv);

void _start(int args) {
    // Extract argc and argv. Code taken from Linux crt0 because we use the same argument passing architecture.
    int *params = &args-1;
    int argc = *params;
    char **argv = (char **) (params+1);
    exit(main(argc,argv));
}
