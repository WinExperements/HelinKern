#ifndef DEBUG_H
#define DEBUG_H
#include <output.h>
#define DEBUG(msg,...) kprintf("(%s:%d): ",__FILE__,__LINE__); \
    kprintf(msg,__VA_ARGS__);
#define DEBUG_N(msg) kprintf("(%s:%d): %s",__FILE__,__LINE__,msg);
#endif
