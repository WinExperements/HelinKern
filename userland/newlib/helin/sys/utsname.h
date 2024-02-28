#ifndef UTNAME_H
#define UTNAME_H


typedef struct utsname {
    char *sysname;    /* Operating system name (e.g., "Linux") */
    char *nodename;   /* Name within communications network */
    char *release;
    char *version;
    char *machine;
};

int uname(struct utsname *name);

#endif
