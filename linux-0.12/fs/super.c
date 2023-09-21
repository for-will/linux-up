/*
 *  linux/fs/super.c
 *
 */

#include "linux/fs.h"

struct super_block super_block[NR_SUPER];
/* this is initialized in init/main.c */
int ROOT_DEV = 0;

void mount_root()
{
        
}

void put_super(int dev)
{
	
}

struct super_block * get_super(int dev)
{
	
}
