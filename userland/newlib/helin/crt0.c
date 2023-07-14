#include <fcntl.h>


extern void exit(int code);
extern int main(int argc,char **argv);

void _start(int args) {
    /*int *params = (int *)&args-1;
    int argc = *params;
    //nt *ptr = (int *)args;
    char **argv = (char **)args;*/
    int argc = *(&args-1);
    char **argv = ((char **)args);
    exit(main(argc,argv));
}
