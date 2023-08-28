

#ifndef _TTY_H
#define _TTY_H

#define MAX_CONSOLES	8
#define NR_SERIALS	2
#define NR_PTYS		4

#include <termios.h>

#define TTY_BUF_SIZE 1024

struct tty_queue {
        unsigned long data;
	unsigned long head;
	unsigned long tail;
	struct task_struct * proc_list;
	char buf[TTY_BUF_SIZE];
};

#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
#define EMPTY(a) ((a)->head == (a)->tail)
#define CHARS(a) (((a)->head-(a)->tail)&(TTY_BUF_SIZE-1))
#define GETCH(queue, c) \
	(void)({c=(queue)->buf[(queue)->tail];INC((queue)->tail);})
#define PUTCH(c, queue) \
	(void)({(queue)->buf[(queue)->head]=(c);INC((queue)->head);})	

#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])

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

extern struct tty_struct tty_table[];
extern int fg_console;

#define TTY_TABLE(nr) \
((struct tty_struct*)(0))

#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void copy_to_cooked(struct tty_struct * tty);

void update_screen(void);

#endif
