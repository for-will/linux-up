/*
 *  linux/kernel/chr_drv/tty_io.c
 *
 */

#include <linux/tty.h>


#define QUEUES (3*(MAX_CONSOLES+NR_SERIALS+2*NR_PTYS))
static struct tty_queue tty_queues[QUEUES];
struct tty_struct tty_table[256];

#define con_queues tty_queues
#define rs_queues ((3*MAX_CONSOLES) + tty_queues)


int fg_console = 0;


struct tty_queue * table_list[] = {
	con_queues + 0, con_queues + 1,
	rs_queues + 0, rs_queues + 1,
	rs_queues + 3, rs_queues + 4
};

void change_console(unsigned int new_console)
{
	// dummy
}


void do_tty_interrupt(int tty)
{
	copy_to_cooked(TTY_TABLE(tty));
}

void chr_dev_init(void)
{

}

void tty_init(void)
{
	// todo:
}

void copy_to_cooked(struct tty_struct *tty)
{
	// dummy
}
