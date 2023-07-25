
#include <termios.h>

struct tty_queue {
        unsigned long data;
        // dummy
};

struct tty_struct {
        struct termios termios;
        int pgrp;
        int session;
        int stopped;
        void (*write)(struct tty_struct * tty);
        struct tty_queue *read_q;
        struct tty_queue *write_q;
        struct tye_queue *secondary;
};

#define TTY_TABLE(nr) \
((struct tty_struct*)(0))
