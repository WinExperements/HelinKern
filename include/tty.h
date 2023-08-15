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

#define FLAG_ECHO 	0000001
#define FLAG_RAW  	0000002
#define FLAG_NOBLOCK 	0000003
void tty_init();
#endif
