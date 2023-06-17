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

void tty_init();
#endif
