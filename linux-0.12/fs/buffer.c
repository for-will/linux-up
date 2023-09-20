/*
 * linux/fs/buffer.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * 'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer (except for the
 * data, of course), but instaed letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though (I hope).
 *
 * ‘buffer.c’用于实现缓冲区高速缓存功能。通过不让中断处理过程改变缓冲区，而是让调
 * 用者来执行，避免了竞争条件（当然除改变数据以外）。注意！由于中断可以唤醒一个调
 * 用者，因此就需要开关中断指令（cli-sti）序列来检测调用时睡眠。但这需要非常地快
 * （我希望）。
 */

/*
 * NOTE! There is one discordant note here: checking floppies for
 * disk change. This is where it fits best, I think, as it should
 * invalidate changed floppy-disk-caches.
 *
 * 注意！有一个程序应不属于这里：检测软盘是否更换。但我想这里是放置
 * 该程序最好的地方了，因为它需要使已更换软盘缓冲失效。
 */

#include "sys/types.h"
#include <linux/fs.h>
#include <stdarg.h>		// 标准参数头文件。以宏的形式定义变量参数列表。主要说明了一个
				// 类型（va_list）和三个宏（va_start，va_arg和va_end），用于
				// vsprintf、vprintf、vfprintf函数。
#include <linux/config.h>	// 内核配置头文件。定义键盘语言和硬盘类型（HD_TYPE）可选项。
#include <linux/sched.h>	// 调度程序头文件。定义了任务结构task_struct、任务0的数据，
				// 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用函数的原型定义。
#include <asm/system.h>		// 系统头文件。定义了设置或修改描述符/中断门等的嵌入汇编宏。
#include <asm/io.h>		// io头文件。定义硬件端口输入/输出宏汇编语句。

// 变量end是由链接程序ld在链接内核模块时生成，用于指明内核执行模块的末端位置，参见
// 图12-15所示。我们可以从编译内核时生成的System.map文件中查出该值。这里用它来表明
// 调整缓冲区开始于内核代码末端位置。
// 第33行上的buffer_wait变量是等待空闲缓冲块而睡眠的任务队列头指针。它与缓冲块头部
// 结构中b_wait指针的作用不同。当任务申请一个缓冲翼支付而正好遇到系统缺乏可用空闲缓冲块
// 时，当前任务就会被添加到buffer_wait睡眠等待队列中。而b_wait则是专门供等待指定缓
// 冲块（即b_wait对应的缓冲块）的任务使用的等待队列头指针。
extern int end;
struct buffer_head * start_buffer = (struct buffer_head *) &end;
struct buffer_head * hash_table[NR_HASH]; 	// NR_HASH = 307项。
static struct buffer_head * free_list;		// 空闲缓冲块链表头指针。
static struct task_struct * buffer_wait = NULL; // 等待空闲缓冲块面睡眠的任务队列。

// 下面定义系统缓冲区中含有的缓冲块个数。这里，NR_BUFFERS是一个定义在linux/fs.h头
// 文件第48行的常量符号，被定义为变量nr_buffers，而该变量在fs.h文件第172行被声明为
// 全局变量。大写名称通常都是一个宏名称，Linux先生这样写代码是为也利用这个大写名称
// 来隐含地表示nr_buffers是一个在内核高速缓冲区初始化后不再改变的“常量”。它将在
// 初始化函数buffer_init()中被设置（第371行）。
/* int nr_buffers = 0; */
int NR_BUFFERS = 0;				// 系统含有缓冲块个数。

/// 等待指定缓冲块解锁。
// 如果指定的缓冲块bh已经被上锁，那么我们就让进程不可中断地睡眠在该缓冲块的等待队列
// b_wait中。在缓冲块解锁时，其等待队列上的所有进程将被唤醒。虽然是在关闭中断（cli）
// 之后去睡眠的，但这样做并不会影响在其他进程上下文中响应中断。因为每个进程都在自己的
// TSS段中保存了标志寄存器EFLAGS的值，所以在进程切换时CPU中当前EFLAGS的值也随之改
// 变。使用sleep_on()进入睡眠状态的进程需要用wake_up()明确地唤醒。
static inline void wait_on_buffer(struct buffer_head * bh)
{
	cli();					// 关中断。
	while (bh->b_lock)			// 如果已被上锁则进程进入睡眠，等待其解锁。
		sleep_on(&bh->b_wait);
	sti();					// 开中断。
}

/// 设备数据同步函数。
// 同步设备和内存高速缓冲中数据。其中sync_inodes()函数字义在inode.c，第59行处。
// 该函数首先调用i节点同步函数，把内存i节点表中所有修改过的i节点写入高速缓冲。
// 然后扫描整个高速缓冲区，并对已被修改的缓冲块产生写盘请求，将缓冲中数据写入盘中，
// 做到高速缓冲中的数据与设备中的同步。
int sys_sync(void)
{
	int i;
	struct buffer_head * bh;

	sync_inodes();				/* write out inodes into buffers */
	bh = start_buffer;			// bh指向缓冲区中开始处。
	for (i = 0; i < NR_BUFFERS; i++,bh++) {
		wait_on_buffer(bh);		// 等待缓冲区解锁（如果已上锁的话）。
		if (bh->b_dirt)
			ll_rw_block(WRITE, bh);	// 产生写设备块请求。
	}
	return 0;
}

/// 对指定设备执行高速缓冲数据与设备上数据的同步操作。
// 该函数首先搜索高速缓冲区中所有缓冲块。对于指定设备dev的缓冲块，若其数据已被修改
// 过就写入盘中（同步操作）。然后把内存中i节点数据表数据写入高速缓冲中。之后再对指定设
// 备dev执行一次与上述相同的写盘操作。
int sync_dev(int dev)
{
	int i;
	struct buffer_head * bh;

// 首先对参数指定的设备执行数据同步操作，让设备上的数据与高速缓冲区中的数据同步。
// 方法是扫描高速缓冲区中所有缓冲块，对指定设备dev的缓冲块，先检测其是否已被上锁，
// 若已被上锁就睡眠等待其解锁。然后再判断一次该缓冲块是否还是指定设备的缓冲块并且
// 已修改过（b_dirt标志置位），若是就对其执行写盘操作。因为在我们睡眠期间该缓冲块
// 有可能已被释放或者挪作它用，所以在继续执行前需要再次判断一下该缓冲块是否还是
// 指定设备的缓冲块。
	bh = start_buffer;			// bh指向缓冲区开始处。
	for (i = 0; i < NR_BUFFERS; i++,bh++) {
		if (bh->b_dev != dev)		// 不是设备dev的缓冲块则继续。
			continue;
		wait_on_buffer(bh);		// 等待缓冲区解锁（如果已上锁的话）。
		if (bh->b_dev == dev && bh->b_dirt)
			ll_rw_block(WRITE, bh);
	}
// 再将i节点数据写入调整缓冲，让i节点表inode_table中的inode与缓冲中的信息同步。
// 然后在高速缓冲中的数据更新之后，再把它们与设备中的数据同步。这里彩两遍同步操作
// 是为了提高内核执行效率。第一遍缓冲区中同步操作可以让内核中许多“脏块”变干净，使得
// i节点的同步操作能够高效执行。本次缓冲区同步操作则把那些由于i节点同步操作而又变
// 脏的缓冲块与设备中数据同步。
	sync_inodes();
	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++,bh++) {
		if (bh->b_dev != dev)
			continue;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_dirt)
			ll_rw_block(WRITE, bh);
	}
	return 0;
}

/// 使指定设备在高速缓冲区中的数据无效。
// 扫描高速缓冲区中所有缓冲块。对指定设备的缓冲块复位其有效（更新）标志和已修改标志。
inline void invalidate_buffers(int dev)
{
	int i;
	struct buffer_head * bh;

	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++,bh++) {
		if (bh->b_dev != dev)		// 如果不是指定设备的缓冲块，则
			continue;		// 继续扫描下一块。
		wait_on_buffer(bh);		// 等待该缓冲区解锁（如果已被上锁）。
// 由于进程执行过睡眠等待，所以需要再判断一下缓冲区是否还是属于指定设备的。
		if (bh->b_dev == dev)
			bh->b_uptodate = bh->b_dirt = 0;
	}
}

/*
 * This routine checks whether a floppy has been changed, and
 * invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mout' or 'open'. This
 * is best way of comgining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 *
 * NOTE! Althouth currently this is only for floppies, the idea is
 * that any additional removable block-device will use this routine,
 * and that mount/open needn't know that floppyies/whatever are
 * special.
 */
/*
 * 该子程序检查一个软盘是否已被更换，如果已经更换就使高速缓冲中与该软驱
 * 对应的所有缓冲区无效。该子程序相对来说较慢，所以我们要尽量少使用它。
 * 所以仅在执行‘mount’或‘open’时才调用它。我想这是将速度和实用性相结合的
 * 最好方法。若在操作过程中更换软盘，就会导致数据的丢失。这是咎由自取。
 *
 * 注意！尽管目前该子程序仅用于软盘，以后任何可移动介质的块设备都将使用该
 * 程序，mount/open操作不需要知道是软盘还是其他什么特殊介质。
 */
/// 检查磁盘是否更换，如果已更换就使对应高速缓冲区无效。
void check_disk_change(int dev)
{
	int i;

// 首先检测一下是不是软盘设备，因为现在仅支持软盘可移动介质。如果不是则退出。然后
// 测试软盘是否已更换，如果没有则退出。floppy_chage()在blk_drv/floppy.c第139行。
	if (MAJOR(dev) != 2)
		return;
	if (!floppy_change(dev & 0x03))
		return;
// 软盘已经更换，所以释放对应设备的i节点位图和逻辑块位图所占的高速缓冲块；并使该
// 设备的i节点和数据块信息所占据的高速缓冲块无效。
	for (i = 0; i < NR_SUPER; i++)
		if (super_block[i].s_dev == dev)
			put_super(super_block[i].s_dev);
	invalidate_inodes(dev);
	invalidate_buffers(dev);
}

// 下面两行代码是hash（散列）函数定义和hash表项的计算宏。
// hash表的主要作用是减少查找比较元素所花费的时间。通过在元素的存储位置与关键字之间
// 建立一个对应关系（hash函数），我们就可以直接通过函数计算立刻查找到指定的元素。建
// 立hash函数的指导条件主要是尽量确保散列到任何数组项的概率基本相等。建立hash函数的
// 方法有多种，这是Linux主要采用了最常用的关键字除留余数法。因为我们寻找的缓冲块有两个
// 条件，即设备号dev和缓冲块号block，因此设计的hash函数肯定需要包含这两个关键值。这里
// 两个关键字的异或操作只是计算关键值的一种方法。再对关键值进行MOD运算就可以保证函数所
// 计算得到的值都处于函数数组项范围内。
#define _hashfn(dev, block) (((unsigned)((dev)^(block))) % NR_HASH)
#define hash(dev, block) hash_table[_hashfn(dev, block)]

/// 从哈市队列和空闲缓冲队列中移走缓冲块。
// hash队列是双向链表结构，空闲缓冲块队列是双向循环链表结构。
static inline void remove_from_queues(struct buffer_head * bh)
{
/* remove from hash-queue */	
	if (bh->b_next)
		bh->b_next->b_prev = bh->b_prev;
	if (bh->b_prev)
		bh->b_prev->b_next = bh->b_next;
// 如果该缓冲块是该队列的头一个块，则让hash表的对应项指向本队列中的下一个缓冲块。
	if (hash(bh->b_dev, bh->b_blocknr) == bh)
		hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
/* remove from free list */
	if (!(bh->b_prev_free) || !(bh->b_next_free))
		panic("Free block list corrupted");
	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;
// 如果空闲链表头指向本缓冲块，则让其指向下一缓冲块。
	if (free_list == bh)
		free_list = bh->b_next_free;
}

/// 将缓冲块插入空闲链表尾部，同时放入hash队列中。
static inline void insert_into_queues(struct buffer_head * bh)
{
/* put at end of free list */
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh;
/* put the buffer in new hash-queue if it has a device */
// 请注意，当hash表某项第1次插入项时，hash()计算值肯定为NULL，因此此时第161行上
// 得到的bh->b_next肯定是NULL，所以第163行上应该在bh->b_next不为NULL时才能给
// b_pre赋bh值。即第163行前应该增加判断“if (bh->b_next)”。该错误到0.96版后
// 才被纠正。
	bh->b_prev = NULL;
	bh->b_next = NULL;
	if (!bh->b_dev)
		return;
	bh->b_next = hash(bh->b_dev, bh->b_blocknr);
	hash(bh->b_dev, bh->b_blocknr) = bh;
	// bh->b_next->b_prev = bh;		// 此句前应添加“if (bh->b_next)”判断。
	if (bh->b_next) bh->b_next->b_prev = bh; // 如上↑
}

/// 利用hash表在高速缓冲中寻找给定设备和指定块号的缓冲块。
// 如果找到则返回缓冲区块的指针，否则返回NULL。
static struct buffer_head * find_buffer(int dev, int block)
{
	struct buffer_head * tmp;

// 搜索hash表，寻找指定设备号和块号的缓冲块。	
	for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next)
		if (tmp->b_dev == dev && tmp->b_blocknr == block)
			return tmp;
	return NULL;
}

/*
 * Why like this, I hear you say... The reason is race-conditions.
 * As we don't lock buffers (unless we are reading them, that is),
 * sometimes might happen to it while we sleep (ie a read-error
 * will force it bad). This shouldn't really happen currently, but
 * the code is ready.
 */
/*
 * 代码为什么是这样子的？我听见你说...原因是竞争条件。由于我们没有对
 * 缓冲块上锁（除非我们正在读取它们中的数据），那么当我们（进程）睡眠时
 * 缓冲块可能会发生一些问题（例如一个读错误将疏散平台该缓冲块出错）。目前
 * 这种情况实际上是不会发生的，但处理的代码已经准备好了。
 */
/// 利用hash表在高速缓冲区中寻找指定的缓冲块。若找到则对该缓冲块上锁并返回块头指针。
struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (;;) {
// 在高速缓冲中寻找给定设备和指定块的缓冲区块，如果没有找到返回NULL，退出。		
		if (!(bh = find_buffer(dev, block)))
			return NULL;
// 若找到了想要的缓冲块，则对该缓冲块增加引用计数，并等待该缓冲块解锁（若已被上锁）。
// 由于经过了睡眠状态，因此有必要再验证该缓冲块的正确性，并返回缓冲块头指针。
// 如果在睡眠时该缓冲块的设备号或块号发生了改变，则撤消对它的引用计数，重新寻找。
		bh->b_count++;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_blocknr == block)
			return bh;
		bh->b_count--;
	}
}

/*
 * Ok, this is getblk, and it isn't very clear, agin to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it locks.
 *
 * The algoritm is changed: hopefully better, and an elusive bug removed.
 */
/*
 * OK，下面是getblk函数，该函数的逻辑并不是很清晰，同样也是因为要考虑
 * 竞争条件问题。其中大部分代码很少用到，（例如重复操作语句），因此它应该
 * 比看上去的样子有效得多。
 *
 * 算法已经作了改变：希望能更好，而且一个难以琢磨的错误已经去除。
 */
// 下面宏用于同时判断缓冲块的修改标志和锁定标志，并定义修改标志的权重要比锁定标志大。
#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)
/// 取高速缓冲中指定的缓冲块。
// 检查指定（设备号和块号）的缓冲块是否已经在高速缓冲中。如果指定块已经在高速缓冲中，
// 则返回对应缓冲块头指针退出；如果不在，就需要在高速缓冲中设置一个对应设备号和块号的
// 新项。返回相应缓冲块头指针。
struct buffer_head * getblk(int dev, int block)
{
	struct buffer_head * tmp, * bh;

repeat:
// 搜索hash表，如果指定块已经在高速缓冲中，则返回对应缓冲块的头指针，退出。
	if ((bh = get_hash_table(dev, block)))
		return bh;
// 否则就扫描空闲空闲数据块链表，寻找空闲缓冲块。
// 首先让tmp指向空链表的第一个空闲缓冲块头，然后执行下面循环中的操作。
	tmp = free_list;
	do {
// 如果该缓冲块正被使用（引用计数不等于0），则继续扫描下一项。对于b_cout=0的块，
// 即高速缓冲中当前没有引用的块不一定是干净（b_dirt=0）或没有锁定的（b_lock=0）。
// 因此，我们还是需要继续下面的判断和选择。例如对于一个b_count=0的块，当一个任务改写
// 过一块内容后就释放了，则该块b_count仍会为0，但是b_lock不等于0；当一个任务执行
// breada()预读几个块时，只要ll_rw_block()调用过后，它就会递减b_count；但此时实际上
// 硬盘访问操作可能还在进行，因此此时b_lock=1，但b_count=0。
		if (tmp->b_count)
			continue;
// 如果缓冲头指针bh为空，或者tmp所指缓冲头的标志（修改、锁定）权重小于bh头标志的权
// 重，则主bh指向tmp缓冲块头。如果该tmp缓冲块头表明缓冲块既没有修改也没有锁定标
// 志置位（即BADNESS()=0），则说明已为指定设备上的块取得对应的高速缓冲块，则退出循环。
// 否则我们就继续执行本循环，看看能否找到一个BADNESS值最小的缓冲块。
		if (!bh || BADNESS(tmp) < BADNESS(bh)) {
			bh = tmp;
			if (!BADNESS(tmp))
				break;
		}
/* and repeat until we find something good */
	} while ((tmp = tmp->b_next_free) != free_list);
// 如果循环检查发现所有缓冲块都正在被使用（所有缓冲块的头部引用计数都>0）中，则睡眠
// 等待有空闲缓冲块可用。当有空闲缓冲块可用时本进程会被明确地唤醒。然后我们就跳转到
// 函数开始处重新查找空闲缓冲块。
	if (!bh) {
		sleep_on(&buffer_wait);
		goto repeat;			// 跳转至210行。
	}
// 执行到这里，说明我们已经找到了一个比较适合的空闲缓冲块了。于是先等待该缓冲区解锁
// （如果已被上锁的话）。如果在我们睡眠阶段该缓冲块又被其他任务使用的话，咱爸妈又重复上
// 述寻找过程。
	wait_on_buffer(bh);
	if (bh->b_count)			// 又被占用？？
		goto repeat;
// 如果该缓冲区已被修改，则将数据写盘，并再次等待缓冲区解锁。同样地，若该缓冲区又被
// 其他任务使用的话，只好再重复上述寻找过程。
	while (bh->b_dirt) {
		sync_dev(bh->b_dev);
		wait_on_buffer(bh);
		if (bh->b_count)		// 又被占用？？
			goto repeat;
	}
/* NOTE!! While we slept waiting for this block, somebody else might */
/* already have added "this" block to the cache. check it */
/* 注意！！当进程为了等待该缓冲块面睡眠时，其他进程可能已经将该缓冲块 */
/* 加入进高速缓冲中，所以我们也好对此进行检查。 */
// 在hash表中检查指定的缓冲块是否乘我们睡眠之际被加入进去。如果是的话，就再次
// 重复上述寻找过程。
	if (find_buffer(dev, block))
		goto repeat;
/* OK, FINALLY we know that this buffer is the only one of it's kind, */
/* and that it's unused (b_count=0), and clean */
/* OK，最终我们知道该缓冲块是指定参数的唯一一块，而且目前还没有被占用 */
/* （b_count=0）也未被上锁（b_lock=0），并且量干净的（未被修改的） */
// 于是让我们占用此缓冲块。置引用计数为1，复位修改标志和有效（更新）标志。
	bh->b_count = 1;
	bh->b_dirt = 0;
	bh->b_uptodate = 0;
// 从hash队列和空闲块链表中移出该缓冲块头，让该缓冲块用于指定设备和其上的指定块。
// 然后根据此新的设备号和块号重新插入到空闲链表和hash队列新位置处，并最终返回缓冲
// 块头指针。
	remove_from_queues(bh);
	bh->b_dev = dev;
	bh->b_blocknr = block;
	insert_into_queues(bh);
	return bh;
}

/// 释放指定缓冲块。
// 等待该缓冲块解锁。然后引用计数递减1，并明确地唤醒等待空闲缓冲块的进程。
void brelse(struct buffer_head *buf)
{
	if (!buf)				// 如果缓冲头指针无效则返回。
		return;
	wait_on_buffer(buf);
	if (!(buf->b_count--))
		panic("Trying to free free buffer");
	wake_up(&buffer_wait);
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 *
 * 从设备上读取指定的数据块并返回含有数据的缓冲区。如果指定的块不存在
 * 则返回NULL。
 */
/// 从设备上读取数据块。
// 该函数根据指定的设备号dev和数据块号block，首先在高速缓冲区中申请一块缓冲块。
// 如果该缓冲块中已经包含有效的数据就直接返回该缓冲块指针，否则就从设备中读取
// 指定的数据块到该缓冲块中并返回缓冲块指针。
struct buffer_head * bread(int dev, int block)
{
	struct buffer_head * bh;

// 在高速缓冲区中申请一块缓冲块。如果返回值是NULL，则表示内核出错，停机。然后我们
// 判断其中是否已有可用数据。如果该缓冲块中数据是有效的（已更新的）可以直接使用，
// 则返回。
	if (!(bh = getblk(dev, block)))
		panic("bread: getblk returned NULL\n");
	if (bh->b_uptodate)
		return bh;
// 否则我们就调用底层块设备读写ll_rw_block()函数，产生读设备块请求。然后等待指定
// 数据块被读入，并等待缓冲区解锁。在睡眠醒来之后，如果该缓冲区已更新，则返回缓冲
// 区头指针，退出。否则表明读设备操作失败，于是释放该缓冲区，返回NULL，退出。
	ll_rw_block(READ, bh);
	wait_on_buffer(bh);
	if (bh->b_uptodate)
		return bh;
	brelse(bh);
	return NULL;
}

/// 复制内存块。
// 从from地址复制一块（1024字节）数据到to位置。
#define COPYBLK(from, to) \
__asm__("cld\n\t" \
	"rep\n\t" \
	"movsl\n\t" \
	::"c" (BLOCK_SIZE/4),"S" (from),"D" (to) \
	:/* "cx","di","si" */)

/*
 * bread_page reads four buffers into memory at the desired address. It's
 * a function of its own, as there is some speed to be got by reading them
 * all at the same time, not waiting for one to be read, and then another
 * etc.
 *
 * bread_page一次读四个缓冲块数据到内存指定的地址处。它是一个完整的函数，
 * 因为同时读取四块可以获得速度上的好处，不用等着读一块，再读一块了。
 */
/// 读设备上一个页面（4个缓冲块）的内容到指定内存地址处。
// 参数address是保存页面数据的地址；dev是指定的设备号；b[4]是含有4个设备数据块号
// 的数组。该函数仅用于mm/memory.c文件的do_no_page()函数中（第428行）。
void bread_page(unsigned long address, int dev, int b[])
{
	struct buffer_head * bh[4];
	int i;

// 该函数循环执行4次，根据放在数组b[]中的4个块号从设备dev中读取一页内容放到指定
// 内存位置address处。对于参数b[i]给出的有效块号，函数首先从高速缓冲中取指定设备
// 和块号的缓冲块。如果缓冲块中数据无效（未更新）则产生读设备请求从设备上读取相应数
// 据块。对于无效的块号b[i]则不用去理它了。因此本函数其实可以根据指定的b[]中的块号
// 随意读取1--4个数据块。
	for (i = 0; i < 4; i++)
		if (b[i]) {			// 若块号有效。
			if ((bh[i] = getblk(dev, b[i])))
				if (!bh[i]->b_uptodate)
					ll_rw_block(READ, bh[i]);
		} else
			bh[i] = NULL;
// 随后将4个缓冲块上的内容顺序复制到指定地址处。在进行复制（使用）缓冲块之前我们
// 先要睡眠等待缓冲块解锁（若被上锁的话）。另外，因为可能睡眠过了，所以我们还需要
// 在复制之前再检查一下缓冲块中的数据是否是有效的。复制完后我们还需要释放缓冲块。
	for (i = 0; i < 4; i++,address += BLOCK_SIZE)
		if (bh[i]) {
			wait_on_buffer(bh[i]);	// 等待缓冲块解锁（若被上锁的话）。
			if (bh[i]->b_uptodate)	// 若缓冲块中数据有效的话则复制。
				COPYBLK((unsigned long) bh[i]->b_data, address);
			brelse(bh[i]);		// 释放该缓冲区。
		}
}

/*
 * Ok, breada can be used as bread, but additionally to mark other
 * blocks for reading as well. End the argument list with a negative
 * number.
 *
 * OK，breada可以像bread一样使用，但会另外预读一些块。该函数参数列表
 * 需要使用一个负数来表明参数列表的结束。
 */
/// 从指定设备读取指定的一些块。
// 函数参数可变，是一系列指定的块号。成功时返回第1块的缓冲块头指针，否则返回
// NULL。
struct buffer_head * breada(int dev, int first, ...)
{
	va_list args;
	struct buffer_head * bh, *tmp;

// 首先取可变参数表中第1个参数（块号）。接着从高速缓冲中取指定设备和块号的缓冲
// 块。如果该缓冲块数据无效（更新标志未置位），则发出读设备数据块请求。
	va_start(args, first);
	if (!(bh = getblk(dev, first)))
		panic("breada: getblk returned NULL\n");
	if (!bh->b_uptodate)
		ll_rw_block(READ, bh);
// 然后顺序取可变参数表中其他预读块号，并作与上面同样处理，但不引用。注意，336行上
// 有一个bug。其中的bh应该是tmp。这个bug直到0.96版的内核代码中才被纠正过来。
// 另外，因为这里预读随后的数据块，只需读进高速缓冲区但并不会马上就使用，所以第337
// 行语句需要将其引用计数递减释放掉该块（因为getblk()函数会增加缓冲块引用计数值）。
	while ((first = va_arg(args, int)) >= 0) {
		tmp = getblk(dev, first);
		if (tmp) {
			if (!tmp->b_uptodate)
				// ll_rw_block(READA, bh); // bh应该是tmp。
				ll_rw_block(READA, bh); // ↑
			tmp->b_count--;			// 暂释放掉就放假预读块。
		}
	}
// 此时可变参数表中所有参数处理完毕。于是等待第1个缓冲块解锁（如果已被上锁）。在等待
// 退出之后如果缓冲块中数据仍然有效，则返回缓冲块头指针退出。否则释放该缓冲块返回NULL，
// 退出。
	va_end(args);
	wait_on_buffer(bh);
	if (bh->b_uptodate)
		return bh;
	brelse(bh);
	return (NULL);
}

/// 缓冲区初始化函数。
// 参数buffer_end是缓冲区内存末端。对于具有16MB内存的系统，缓冲区末端被设置为4MB。
// 对于有8MB内存的系统，缓冲区末端被设置为2MB。该函数从缓冲区开始位置start_buffer
// 处事缓冲区末端buffer_end处分别同时设置（初始化）缓冲块头结构和陈若仪的数据块，直到
// 缓冲区中所有内存被分配完毕。参见程序列表前面的示意图。
void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer;
	void * b;
	int i;

// 首先根据参数提供的缓冲区高端位置确定实际缓冲区高端位置b。如果缓冲区高端等于1MB，
// 则因为从640KB - 1MB被显示内存和BIOS占用，所以实际可用缓冲区内存高端应该是
// 640KB。否则缓冲区内存高端一定大于1MB。
	if (buffer_end == 1<<20)
		b = (void *) (640*1024);
	else
		b = (void *) buffer_end;
// 下面这段代码用于初始化高速缓冲区，建立空闲缓冲块循环链表，并获取系统中缓冲块数目。
// 操作的过程是从缓冲区高端开始划分1KB大小的缓冲块，与此同时在缓冲区低端建立描述该
// 缓冲块的结构buffer_head，并将这些buffer_head给成双向链表。
// h是指向缓冲块头结构的指针，而h+1是指向内存地址连续的下一个缓冲块头地址，也可以说
// 是指向h缓冲块头的末端处。为了保证有足够长度的内存来存储一个缓冲块头结构，需要b所
// 指向的内存块地址大于h的缓冲块头的末端，即要求b>h+1。
	while ((b -= BLOCK_SIZE) >= ((void *) (h+1))) {
		h->b_dev = 0;			// 使用该缓冲块的设备号。
		h->b_dirt = 0;			// 脏标志，即缓冲块修改标志。
		h->b_count = 0;			// 缓冲块引用计数。
		h->b_lock = 0;			// 缓冲块锁定标志。
		h->b_uptodate = 0;		// 缓冲块更新标志（或称数据有效标志）。
		h->b_wait = NULL;		// 指向等待该缓冲块解锁的进程。
		h->b_next = NULL;		// 指向具有相同hash值的下一个缓冲头。
		h->b_prev = NULL;		// 指向具有相同hash值的前一个缓冲头。
		h->b_data = (char *) b;		// 指向对应缓冲块数据块（1024字节）。
		h->b_prev_free = h - 1;		// 指向链表中前一项。
		h->b_next_free = h + 1;		// 指向链表中下一项。
		h++;				// h指向下一新缓冲头位置。
		NR_BUFFERS++;			// 缓冲区块数累加。
		if (b == (void *) 0x100000)	// 若b递减到等于1MB，则跳过384KB，
			b = (void *) 0xA0000;	// 让b指向地址0xA0000(640KB)处。
		/* 640KB内存（buffer_head）大概可以映射17.8MB的内存block */
		/* 所以更大的内存的话，这里会有bug，需要修改 */
	}
// 然后让h指向最后一个缓冲块头；让空闲链表头指向头一个缓冲块头；链表头的b_prev_free
// 指向前一项（即最后一项）；h的下一项指针指向第一项，从而让空闲链表形成双向环形结构。
// 最后初始化hash表，置表中所有指针为NULL。
	h--;					// 让h指向最后一个缓冲块头。
	free_list = start_buffer;		// 让空闲链表头指向第一个缓冲块头。
	free_list->b_prev_free = h;
	h->b_next_free = free_list;
	for (i = 0; i < NR_HASH; i++)
		hash_table[i] = NULL;
}
