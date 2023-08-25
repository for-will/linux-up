#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/types.h>

#define NCCS 17
struct termios {
        void * dummy;
	cc_t c_cc[NCCS];	/* control characters */
};

/* c_cc characters */
#define VERASE 2


/* c_iflag bits */
#define ICRNL	0000400
#define IUCLC	0001000
#define IXON	0002000

/* c_oflag bits */
#define OPOST	0000001
#define OLCURC	0000002
#define ONLCR	0000004


/* c_lflag bits */
#define ISIG	0000001
#define ICANON	0000002
#define XCASE	0000004
#define ECHO	0000010
#define ECHOE	0000020
#define ECHOK	0000040
#define ECHONL	0000100
#define NOFLSH	0000200
#define TOSTOP	0000400
#define ECHOCTL	0001000
#define ECHOPRT	0002000
#define ECHOKE	0004000
#define FLUSHO	0010000
#define PENDIN	0040000
#define IEXTEN	0100000

#endif
