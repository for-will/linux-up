/*
 *  linux/kernel/blk_drv/floppy.c
 *
 */

#include <linux/fs.h>

#include <sys/types.h>

#define MAJOR_NR 2
#include "blk.h"

unsigned char selected = 0;

void unexpected_floppy_interrupt() // dummy
{
	// todo:
}

void floppy_init(void)
{
	// todo:
}

void do_fd_request(void)
{
	// dummy
}