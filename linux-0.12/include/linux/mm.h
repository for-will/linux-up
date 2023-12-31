#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096		/* 定义1页内存页面字节数。注意高速缓冲块长度是1024字节 */

#include <linux/kernel.h>
#include <signal.h>

extern int SWAP_DEV;		// 内存页面交换设备号。定义在mm/memor.c文件中，第36行。

// 从交换设备读入和写出被交换内存页面。ll_rw_page()定义在blk_drv/ll_rw_block.c文件中。
// 参数nr是主内存区中页面号；buffer是读/写缓冲区。
#define read_swap_page(nr, buffer) ll_rw_page(READ, SWAP_DEV, (nr), (buffer));
#define write_swap_page(nr, buffer) ll_rw_page(WRITE, SWAP_DEV, (nr), (buffer));

// 在主内存区中取空闲物理页面。如果已经没有可用内存了，则返回0。
extern unsigned long get_free_page(void);

// 把内容已修改过的一物理内存页面映射到线性地址空间指定位置处。与put_page()几乎完全一样。
extern unsigned long put_dirty_page(unsigned long page, unsigned long address);

// 释放物理地址addr开始的一页内存。
extern void free_page(unsigned long addr);
void swap_free(int page_nr);
void swap_in(unsigned long *table_ptr);

// extern inline volatile void oom(void)
// 这个函数貌似不能内联
static inline void oom(void) __attribute__((noreturn));
static inline void oom(void) 
{
// do_exit()应该使用退出代码，这里用了信号值SIGSEGV（11）相同值的出错码含义是“资源暂
// 不可用”，正好同义。
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

// 刷新页变换高速缓冲（TLB）宏函数。
// 为了提高地址转换的效率，CPU将最近使用的页表数据存放在芯片中高速缓冲中。在修改过
// 页表信息之后，就需要刷新该缓冲区。 这里使用重新加载页目录基地址寄存器CR3的方法来
// 进行刷新。下面eax = 0，是页目录的基址。
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

/* these are not to be changed without changing head.s etc */
/* 下面定义若需要改动，则需要与head.s等文件中的相关信息一起改变 */
// Linux0.12内核默认支持的最大内存容量是16MB，可以修改这些定义以适合更多的内存。
#define LOW_MEM 0x100000			/* 机器物理内存低端（1MB） */
extern unsigned long HIGH_MEMORY;		/* 存放实际物理内存最高端地址 */
#define PAGING_MEMORY (15*1024*1024)		/* 分页内存15MB。主内存区最多15MB */
#define PAGING_PAGES (PAGING_MEMORY>>12)	/* 分页后的物理内存页面数（3840） */
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)	/* 指定内存地址映射为页面号 */
#define USED 100				/* 页面被占用标志，参见memory.c，449行 */

// 内存映射字节图（1字节代表1页内存）。每个页面对应的字节用于标志页面当前被引用
// （占用）次数。它最大可以映射15MB的内存空间。在初始化函数mem_init()中，对于不
// 能用作主内存区页面的位置均都预先被设置成USED（100）。
extern unsigned char mem_map [ PAGING_PAGES ];

// 下面字义的符号常量对应页目录表项和页表（二级页表）项中的一些标志位。
#define PAGE_DIRTY	0x40			/* 位6，弹幕弹幕脏（已修改） */
#define PAGE_ACCESSED	0x20			/* 位5，页面被访问过 */
#define PAGE_USER	0x04			/* 位2，页面属于：1-用户；0-超级用户 */
#define PAGE_RW		0x02			/* 位1：读写权：1 - 写；0 - 读 */
#define PAGE_PRESENT	0x01			/* 位0，页面存在：1-存在；0-不存在 */

#endif

