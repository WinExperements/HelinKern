#include <fcntl.h>


extern void exit(int code);
extern int main(int argc,char **argv);
extern char **environ;
#if !defined(__x86_64__)
void _start(int args) {
    int argc = *(&args-1);    
    environ = ((char **)*(&args+1));
    char **argv = (char **)args;
    exit(main(argc,argv));
}
#else
void _start(int argc,char **argv,char **env) {
	environ = env;
	exit(main(argc,argv));
}
#endif
