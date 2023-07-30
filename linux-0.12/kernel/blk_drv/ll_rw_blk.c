/*
 *  linux/kernel/blk_drv/ll_rw_blk.c
 *
 */
#include <linux/sched.h>

#include "blk.h"

struct task_struct * wait_for_request = NULL;

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	// dummy
};

static inline void lock_buffer(struct buffer_head * bh)
{
	// dummy
}

static inline void unlock_buffer(struct buffer_head * bh)
{
	// dummy
}

void blk_dev_init(void)
{
	//todo:
}