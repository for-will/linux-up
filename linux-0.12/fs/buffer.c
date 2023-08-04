/*
 * linux/fs/buffer.c
 *
 */

#include <linux/fs.h>	// defined NR_BUFFERS nr_buffers

int NR_BUFFERS = 0;	// int nr_buffers = 0;

void buffer_init(long buffer_end)
{
	//todo:
}

void sys_sync()
{
	// dummy
}

void brelse(struct buffer_head * buf)
{
	// dummy
}

struct buffer_head * bread(int dev, int block)
{
	// dummy
	return 0;
}