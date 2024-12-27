#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
extern void helin_syscalls_init();
extern void exit(int code);
extern int main(int argc,char **argv,char **envp);
extern char **environ;
char *__progname;
extern char *basename(char *path);
void defsig_handler(int sig) {
	psignal(sig,"");
	exit(sig);
}

#if defined(__i386__)
void _start(int args) {
#elif defined(__x86_64__)
void _start(int argc,char **argv,char **env) {
#endif
#if defined(__i386__)
    int argc = *(&args-1);    
    environ = ((char **)*(&args+1));
    char **argv = (char **)args;
#elif defined(__x86_64__)
    environ = env;
#else
#error "Unknown type of crt0 _start!"
#endif
    // call constructors. Generally used by C++ programs.
    extern uintptr_t __init_array_start;
    extern uintptr_t __init_array_end;
    for (uintptr_t *addr = &__init_array_start; addr < &__init_array_end; addr++) {
      void (*constructor)() = (void (*)())*addr;
      constructor();
    }
    // call crtbegin.o
    __progname = basename(argv[0]);
    // initialize libc internals.
    helin_syscalls_init();
    // call main!
    exit(main(argc,argv,environ));
}
