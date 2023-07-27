/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#define NR_OPEN 20 //:20

#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024 //:49

struct m_inode {
        unsigned short i_mode;
        unsigned short i_count;
};

struct file {
        unsigned short f_count;
};

extern int nr_buffers;//:172

extern int ROOT_DEV;//:206

#endif