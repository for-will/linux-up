/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#define READ 0
#define WRITE 1
#define READA 2
#define WRITEA 3

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define SUPER_MAGIC 0x137F			\
	

#define NR_OPEN 20 

#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

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

	unsigned short i_dev;
};

struct file {
        unsigned short f_count;
};

struct super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

struct d_super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern int bmap(struct m_inode * inode, int block);

extern int nr_buffers;//:172
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev, int block);
extern struct buffer_head * breada(int dev, int block, ...);
extern void bread_page(unsigned long addr, int dev, int b[4]);

extern int ROOT_DEV;//:206

#endif
