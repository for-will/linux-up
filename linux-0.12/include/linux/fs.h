/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include "linux/fdreg.h"
#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd
 * 4 - /dev/ttyx
 * 5 - /dev/tty
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */
/*
 * 系统所含的设备如下：（与minix系统的一样，所以我们可以使用minix的
 * 文件系统。下面这些是主设备号。）
 *
 * 0 - 没有用到（nodev）
 * 1 - /dev/mem		内存设备。
 * 2 - /dev/fd		软盘设备。
 * 3 - /dev/hd		硬盘设备。
 * 4 - /dev/ttyx	tty串行终端设备。
 * 5 - /dev/tty		tty终端设备。
 * 6 - /dev/lp		打印设备。
 * 7 - unnamed pipes	没有命名的管道。
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)	/* 判断设备是否是可以寻找定位的 */

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* “write-ahead” - silly, but somewhat useful  */

void buffer_init(long buffer_end);		// 高速缓冲区初始化函数。

#define MAJOR(a) (((unsigned)(a))>>8)		/* 取设备主设备号（是高字节） */
#define MINOR(a) ((a)&0xff)			/* 取设备次设备号（是低字节） */

#define NAME_LEN 14				/* 名字长度值 */
#define ROOT_INO 1				/* 根i节点 */

#define I_MAP_SLOTS 8				/* i节点位图槽数 */
#define Z_MAP_SLOTS 8				/* 逻辑块（区段块）位图槽数 */
#define SUPER_MAGIC 0x137F			/* 文件系统魔数 */
	
#define NR_OPEN 20 				/* 进程最多打开文件数 */
#define NR_INODE 64				/* 系统同时最多使用I节点个数 */
#define NR_FILE 64				/* 系统最多文件个数（文件数组项数） */
#define NR_SUPER 8				/* 系统所含超级块个数（超级块数组项数） */
#define NR_HASH 307				/* 缓冲区Hash表数组项数值 */
#define NR_BUFFERS nr_buffers			/* 系统所含缓冲块个数。初始化后不再改变 */
#define BLOCK_SIZE 1024				/* 数据块长度（字节值） */
#define BLOCK_SIZE_BITS 10			/* 数据块长度所占比特位数 */
#ifndef NULL
#define NULL ((void *) 0)
#endif

// 每个逻辑块可存放的i节点数。
#define INODES_PER_BLOCK ((BLOCK_SIZE/(sizeof (struct d_inode))))
// 每个逻辑块可存放的目录项数。
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE/(sizeof (struct dir_entry))))

// 管道头、管道尾、管道大小、管道空？、管道满？、管道头指针递增。
#define PIPE_READ_WAIT(inode) ((inode).i_wait)
#define PIPE_WRITE_WAIT(inode) ((inode).i_wait2)
#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))

#define NIL_FILP 	((struct file *)0)	/* 空文件结构指针 */
#define SEL_IN		1
#define SEL_OUT		2
#define SEL_EX		4

typedef char buffer_block[BLOCK_SIZE];		// 块缓冲区。

// 缓冲块头数据结构。（极为重要！！！）
// 在程序中常用bh来表示buffer_head类型的缩写。
struct buffer_head {
        char * b_data;				/* pointer to data block (1024 bytes) */
        unsigned long b_blocknr;		/* block number */ // 块号。
        unsigned short b_dev;			/* device (0 = free) */ // 数据源的设备号。
        unsigned char b_uptodate;	 	// 更新标志：表示数据是否已更新。
						// 1：数据是有效的（已更新的）；
						// 0：需要从设备中读取更新。
						// 叫b_loaded不好吗？？
	unsigned char b_dirt;			/* 0-clean, 1-dirty */
        unsigned char b_count;			/* users using this block */ // 使用的用户数。
        unsigned char b_lock;			/* 0 - ok, 1 - lcoked */ // 缓冲区是否被锁定。
        struct task_struct * b_wait;		// 指向等待该缓冲区解锁的任务。
        struct buffer_head * b_prev;		// hash队列上前一块（这四个指针用于缓冲区的管理）。
        struct buffer_head * b_next;		// hash队列上下一块。
        struct buffer_head * b_prev_free;	// 空闲表上前一块。
        struct buffer_head * b_next_free;	// 空闲表上下一块。
};

// 磁盘上的索引节点（i节点）数据结构（32字节）。
struct d_inode {
	unsigned short i_mode;			// 文件类型和属性（rwx位）。
	unsigned short i_uid;			// 用户id（文件拥有者标识符）。
	unsigned long i_size;			// 文件大小（字节数）。
	unsigned long i_time;			// 村委会那天时间（自1970.1.1：0算起，秒）。
	unsigned char i_gid;			// 组id（文件拥有者所在的组）。
	unsigned char i_nlinks;			// 链接数（多少信文件目录项指向该i节点）。
	unsigned short i_zone[9];		// 直接（0-6）、间接（7）或双重间接（8）逻辑块号。
						// zone是区的意思，可译成区段，或逻辑块。
};

// 这里在内存中的i节点结构。前7项与d_inode完全一样。
struct m_inode {
        unsigned short i_mode;			// 文件类型和属性（rwx位）。
	unsigned short i_uid;			// 用户id（文件拥有者标识符）。
	unsigned long i_size;			// 文件大小（字节数）。
	unsigned long i_mtime;			// 修改时间（自1970.1.1：0算起，秒）。
	unsigned char i_gid;			// 组id（文件拥有都所在的组）。
	unsigned char i_nlinks;			// 文件目录项链接数。
	unsigned short i_zone[9];		// 直接（0-6）、间接（7）或双重间接（8）逻辑块号。
/* these are in memory also */
	struct task_struct * i_wait;		// 等待该i节点的进程。
	struct task_struct * i_wait2; 		/* for pipes */
	unsigned long i_atime;			// 最后访问时间。
	unsigned long i_ctime;			// i节点自身修改时间。
	unsigned short i_dev;			// i节点所在的设备号。
	unsigned short i_num;			// i节点号。
        unsigned short i_count;			// i节点被使用的次数，9青汁该i节点空闲。
	unsigned char i_lock;			// 锁定标志。
	unsigned char i_dirt;			// 已修改（脏）标志。
	unsigned char i_pipe;			// 管道标志。
	unsigned char i_mount;			// 安装标志。
	unsigned char i_seek;			// 搜寻标志（lseek时）。
	unsigned char i_update;			// 更新标志。
};

// 文件结构（用于在文件句柄与i节点之间建立关系）。
struct file {
	unsigned short f_mode;			// 文件操作模式（RW位）
	unsigned short f_flags;			// 文件打开或控制的标志。
	unsigned short f_count;			// 对应文件引用计数值。
	struct m_inode * f_inode;		// 指向对应i节点。
	off_t f_pos;				// 文件位置（读写偏移值）。
};

// 内存中磁盘超级块结构。
struct super_block {
	unsigned short s_ninodes;		// 节点数。
	unsigned short s_nzones;		// 逻辑块数。
	unsigned short s_imap_blocks;		// i节点位图所占用的数据块数。
	unsigned short s_zmap_blocks;		// 逻辑块位图所占用的数据块数。
	unsigned short s_firstdatazone;		// 第一个数据逻辑块号。
	unsigned short s_log_zone_size;		// log(数据块数/逻辑块)。（以2为底）。
	unsigned long s_max_size;		// 文件最大长度。
	unsigned short s_magic;			// 文件系统魔数。
/* These are only in memory */
	struct buffer_head * s_imap[8];		// i节点位图缓冲块指针数组（占用8块，可表示64M）。
	struct buffer_head * s_zmap[8];		// 逻辑块立体图缓冲块指针数组（占用8块）。
	unsigned short s_dev;			// 超级块所在的设备号。
	struct m_inode * s_isup;		// 被安装的文件系统根目录的i节点。（isup-super i）
	struct m_inode * s_imount;		// 被安装到的i节点。
	unsigned long s_time;			// 修改时间。
	struct task_struct * s_wait;		// 等待该超级块的进程。
	unsigned char s_lock;			// 被锁定标志。
	unsigned char s_rd_only;		// 只读标志。
	unsigned char s_dirt;			// 已修改（脏）标志。
};

// 磁盘上超级块结构，与上面131-138行完全一样。
struct d_super_block {
	unsigned short s_ninodes;		// 节点数。
	unsigned short s_nzones;		// 逻辑块数。
	unsigned short s_imap_blocks;		// i节点位图和客人的数据块数。
	unsigned short s_zmap_blocks;		// 逻辑块位图所占用的数据块数。
	unsigned short s_firstdatazone;		// 第一个数据逻辑块。
	unsigned short s_log_zone_size;		// log（数据块数/逻辑块）。（以2为底）。
	unsigned long s_max_size;		// 文件最大长度。
	unsigned short s_magic;			// 文件系统魔数。
};

// 文件目录项结构（16字节）。
struct dir_entry {
	unsigned short inode;			// i节点号。
	char name[NAME_LEN];			// 文件名，长度NAME_LEN=14。
};

extern struct m_inode inode_table[NR_INODE];	// 字义i节点表数组（32项）。
extern struct file file_table[NR_FILE];		// 文件表数组（64项）。
extern struct super_block super_block[NR_SUPER]; // 超级块数组（8项）。
extern struct buffer_head * start_buffer;	// 缓冲区起始内存位置。
extern int nr_buffers;				// 彖块数。

/// 以下是磁盘驱动器操作函数原型。
// 检测驱动器中软盘是否改变。
extern void check_disk_change(int dev);

// 检测指定软驱中软盘更换情况。如果软盘更换了则返回1，否则返回0。
extern int floppy_change(unsigned int nr);

// 设置启动指定驱动器所需等待的时间（设置等待定时器）。
extern int ticks_to_floppy_on(unsigned int dev);

// 启动指定驱动器。
extern void floppy_on(unsigned int dev);

// 关闭指定的软盘驱动器。
extern void floppy_off(unsigned int dev);


/// 以下是文件系统操作管理用的函数原型。
// 将i节点指定的文件截为0。
extern void truncate(struct m_inode * inode);

// 刷新i节点信息。
extern void sync_inodes(void);

// 等待指定的i节点。
extern void wait_on(struct m_inode * inode);

// 逻辑块（区段，磁盘块）位图操作。取数据块block在设备上对应的逻辑块号。
extern int bmap(struct m_inode * inode, int block);

// 创建数据块block在设备上对应的逻辑块，并返回在设备上的逻辑块号。
extern int create_block(struct m_inode * inode, int block);

// 获取指定路径名的i节点。
extern struct m_inode * namei(const char * pathname);

// 获取指定路径名的i节点，不跟随符号链接。
extern struct m_inode * lnamei(const char * pathname);

// 根据路径名为打开文件操作作准备。
extern int open_namei(const char * pathname, int flag, int mode,
		      struct m_inode ** res_inode);

// 释放一个i节点（回写入设备）。
extern void iput(struct m_inode * inode);

// 从设备读取指定节点号的一个i节点。
extern struct m_inode * iget(int dev, int nr);

// wwi节点表（inode_table）中获取一个空闲i节点项。
extern struct m_inode * get_empty_inode(void);

// 获取（申请一）管道节点。返回为i节点指针（如果是NULL则失败）。
extern struct m_inode * get_pipe_inode(void);

// 在哈希表中查找指定的数据块。返回找到块的缓冲头指针。
extern struct buffer_head * get_hash_table(int dev, int block);

// 从设备读取指定块（首先会在hash表中查找）。
extern struct buffer_head * getblk(int dev, int block);

// 低级读/写数据块函数。
extern void ll_rw_block(int rw, struct buffer_head * bh);

// 读/写数据页面，即每次4块数据块。
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);

// 释放指定缓冲块。
extern void brelse(struct buffer_head * buf);

// 读取指定的数据块。
extern struct buffer_head * bread(int dev, int block);

// 读一个页面（4块缓冲区）到指定地址的内存中。
extern void bread_page(unsigned long addr, int dev, int b[4]);

// 读取头一个指定的数据块，并标记后续将要读的块。
extern struct buffer_head * breada(int dev, int block, ...);

// 向设备dev申请一个磁盘块（区段，逻辑块），返回逻辑块号。
extern int new_block(int dev);

// 释放设备数据区中的逻辑块（区段，磁盘块）block。复位指定逻辑块block的逻辑块位图比特位。
extern int free_block(int dev, int block);

// 为设备dev建立一个新i节点，返回i节点号。
extern struct m_inode * new_inode(int dev);

// 释放一个i节点（删除文件时）。
extern void free_inode(struct m_inode * inode);

// 刷新指定设备缓冲区。
extern int sync_dev(int dev);

// 读取指定设备的超级块。
extern struct super_block * get_super(int dev);

extern int ROOT_DEV;

// 安装根文件系统。
extern void mount_root(void);

// super.c
extern void put_super(int dev);

// inode.c
extern void invalidate_inodes(int dev);

#endif

