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
#define NR_INODE 64
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

struct buffer_head {
        char * b_data;
        unsigned long b_blocknr;
        unsigned short b_dev;
        unsigned char b_uptodate;		// 1：数据是有效的（已更新的）；0：需要从设备中读取更新。
						// 叫b_loaded不好吗？？
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
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
/* these are in memory also */
	struct task_struct * i_wait;
	struct task_struct * i_wait2; 		/* for pipes */
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;
	unsigned short i_num;
        unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
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
/* These are only in memory */
	struct buffer_head * s_imap[8];
	struct buffer_head * s_zmap[8];
	unsigned short s_dev;
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
extern void truncate(struct m_inode * inode);
extern void sync_inodes(void);
extern int bmap(struct m_inode * inode, int block);

extern struct super_block super_block[NR_SUPER];
extern struct buffer_head * start_buffer;
extern int nr_buffers;

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern void iput(struct m_inode * inode);
extern struct m_inode * iget(int dev, int nr);
extern struct m_inode * get_empty_inode(void);
extern struct m_inode * get_pipe_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev, int block);
extern struct buffer_head * breada(int dev, int block, ...);
extern void bread_page(unsigned long addr, int dev, int b[4]);

extern struct super_block * get_super(int dev);
extern int ROOT_DEV;

// super.c
extern void put_super(int dev);

// inode.c
extern void invalidate_inodes(int dev);

#endif
