/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NR_OPEN 20 

#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024 //:49

struct buffer_head {
        char * b_data;
        unsigned long b_blocknr;
        unsigned short b_dev;
        unsigned char b_uptodate;
        unsigned char b_dirt;
        unsigned char b_count;
        unsigned char b_lock;
        struct task_struct * b_wait;
        struct buffer_head * b_prev;
        struct buffer_head * b_next;
        struct buffer_head * b_prev_free;
        struct buffer_head * b_next_free;
};

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