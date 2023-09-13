#ifndef _TTY_IO_H
#define _TTY_IO_H

void verify_area(void * addr, int count);
/* volatile void panic(const char * str); */
void panic(const char * str) __attribute__((__noreturn__));
/* volatile void do_exit(long error_code); */
void do_exit(long error_code) __attribute__((noreturn));
/* int printf(const char * fmt, ...); */
int printk(const char * fmt, ...);


extern int beepcount;
extern int hd_timeout;
extern int blankinterval;
extern int blankcount;

#define suser() (current->euid == 0)

#endif
