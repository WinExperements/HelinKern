extern void printf(const char *format,...);
struct utsname {
    char *sysname;    /* Operating system name (e.g., "Linux") */
    char *nodename;   /* Name within communications network */
    char *release;
    char *version;
    char *machine;
};
extern int uname(struct utsname *t);
int main() {
        printf("Hello world!\n");
        printf("checking system\n");
        struct utsname a;
        uname(&a);
        printf("Running on %s. This is example.\n",a.sysname);
        return 0;
}
