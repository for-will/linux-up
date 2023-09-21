

#include "linux/fs.h"
#include "sys/types.h"

void invalidate_inodes(int dev)
{
	
}

void sync_inodes(void)
{
	
}

void iput(struct m_inode * inode)
{
        // dummy
}

int bmap(struct m_inode *inode, int block)
{
	return 0;
}

struct m_inode * get_empty_inode(void)
{
	return NULL;
}

struct m_inode * get_pipe_inode(void)
{
	return NULL;
}
