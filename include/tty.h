#ifndef TTY_H
#define TTY_H

/* TTY device support
 *
 * Any keyboard devices is registred as single keyboard device in devfs.
 * This keyboard device is begin used by the TTY device to able read from user.
 * This TTY implementation is specially added for better newlib support.
 */


/*
 * Add /dev/tty
*/
#include <typedefs.h>
struct termios {
        tcflag_t c_iflag;
        tcflag_t c_oflag;
        tcflag_t c_cflag;
        tcflag_t c_lflag;
        speed_t c_ispeed;
        speed_t c_ospeed;
        unsigned char c_cc[20];
};

#define FLAG_ECHO 	0000010
#define FLAG_RAW  	0000002
#define FLAG_NOBLOCK 	0000003
void tty_init();
int tty_getFlags();
#endif
