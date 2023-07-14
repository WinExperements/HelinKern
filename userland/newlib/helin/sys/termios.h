#ifndef SYS_TERMIOS_H
#define SYS_TERMIOS_H

typedef unsigned short tcflag_t;
typedef unsigned int speed_t;

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	speed_t c_ispeed;
	speed_t c_ospeed;
	unsigned char c_cc[20];
};

// Flags
#define ECHO 0000001

// Functions
int tcgetattr(int fd,struct termios* tio);
int tcsetattr(int fd,struct termios* tio);


#endif
