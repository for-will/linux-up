/*
 *  linux/kernel/blk_drv/hd.c
 *
 */

#include <linux/fs.h>

#include <sys/types.h>

#define MAJOR_NR 3
#include "blk.h"

void unexpected_hd_interrupt() // dummy
{
	// todo
}

void hd_init(void)
{
	//todo:
}

void hd_times_out(void)
{
	// dummy
}