/*
 *  linux/fs/super.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * super.c contains code to handle the super-block tables.
 *
 * super.c程序中有处理超级块表的代码。
 */

#include "linux/fs.h"
#include "sys/types.h"

#include <linux/config.h>
#include <linux/sched.h>

#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

// 对指定设备执行高速缓冲与设备上数据的同步操作（fs/buffer.c，59行）。
int sync_dev(int dev);
void wait_for_keypress(void);			// 等待击键（kernel/chr_drv/tty_io.c，140行）。

/* set_bit uses setb, as gas doesn't recognize setc */
/* set_bit()使用了setb指令，因为汇编编译器gas不能识别指令setc */
/// 测试指定位偏移处比特位的值，并返回该原比特位值。
// 这个内联汇编宏定义与bitmap.c程序19行上的类似，但这个宏仅测试并返回比特位设置值，
// 而不对比特位进行任何改动（因此该宏定义若取名为test_bit()可能更妥贴一些）。
// 输入：%0 - eax(__res)；%1 - eax(0)；%2 - bitnr，位偏移值；%3 - （addr），起始地址。
// 第23行定义了一个局部寄存器变量。该变量将被保存在eax寄存器中，以便于高效访问和操
// 作。整个宏定义了琴语句表达式，该表达式的值是最后__res的值。第24行上指令BT用于
// 对比特位进行测试（Bit Test）。它会把地址addr（%3）和比特位偏移量bitnr（%2）指定的
// 比特位的值放入进位标志位CF中。指令SETB用于根据进位标志CF设置操作数%al。如果CF=1
// 则%al = 1，否则 %al = 0。
#define set_bit(bitnr, addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res;})

struct super_block super_block[NR_SUPER];	// 超级块结构表数组（NR_SUPER = 8）。
/* this is initialized in init/main.c */
int ROOT_DEV = 0;				// 根文件系统设备号。

/// 锁定超级块。
// 如果超级块已被锁定，则将当前任务置为不可中断的等待状态，并添加到该超级块等待队列
// s_wait中。直到该超级块解锁并明确地唤醒本任务。然后对其上锁。
// 这三个函数（lock_super()、free_super()和wait_on_super()）的作用与inode.c程序文
// 件中头3个函数的作用雷同，只是这里操作的对象换成了超级块。
static void lock_super(struct super_block * sb)
{
	cli();					// 关中断。
	while (sb->s_lock)			// 如果该超级块已经上锁，则睡眠等待。
		sleep_on(&(sb->s_wait));	// kernel/sched.c
	sb->s_lock = 1;				// 给该超级块加锁（置锁定标志）。
	sti();					// 开中断。
}

/// 对指定超级块解锁。
// 复位超级块的锁定标志，并明确地唤醒等待在此超级块等待队列s_wait上的所有进程。
// 如果使用ulock_super这个名称可能更妥帖。
static void free_super(struct super_block * sb)
{
	cli();
	sb->s_lock = 0;				// 复位锁定标志。
	wake_up(&(sb->s_wait));			// 唤醒等待该超级块的进程。
	sti();					// wake_up()在kernel/sched.c，第188行。
}

/// 睡眠等待超级块解锁。
// 如果超级块已被锁定，则将当前任务置为不可中断的等待状态，并添加到该超级块的等待队
// 列s_wait中。直到该超级块解锁并明确地唤醒本任务。
static void wait_on_super(struct super_block * sb)
{
	cli();
	while (sb->s_lock) 			// 如果超级块已经上锁，则睡眠等待。
		sleep_on(&(sb->s_wait));
	sti();
}

/// 获取指定设备的超级块。
// 在超级块表（数组）中搜索指定设备dev的超级块结构信息。若找到则返回超级块的指针，
// 否则返回空指针。
struct super_block * get_super(int dev)
{
	struct super_block * s;			// s是超级块数据结构指针。

// 首先判断参数所给出设备的有效性。若设备号为0则返回空指针。然后让s指向超级块数组
// 起始处，开始搜索整个超级块数组，以寻找指定设备dev的超级块。第62行上的指针赋
// 值语句“s = 0+super_block”等同于“s = super_block”、“s = &super_block[0]”。
	if (!dev)
		return NULL;
	s = 0 + super_block;
	while (s < NR_SUPER + super_block) {
// 如果当前搜索项是指定设备的超级块，即该超级块的设备号字段与函数参数指定的相同，
// 则先等待该超级块解锁（若已被其他进程上锁的话）。在等待期间，该超级块项有可能被
// 其他设备使用，因此等待返回之后需再判断一次是否是指定设备的超级块。如果是则返回
// 该超级块的指针。否则就重新对超级块数组再搜索一遍，因此此时s需重又指向超级块数
// 组开始处。
		if (s->s_dev == dev) {
			wait_on_super(s);
			if (s->s_dev == dev)
				return s;
			s = 0 + super_block;
// 如果当前搜索项不是，则检查下一项。如果没有找到指定的超级块，则返回空指针。
		} else
			s++;
	}
	return NULL;
}

/// 释放（放回）指定设备的超级块。
// 释放设备所使用的超级块数组项（置其s_dev=0），并释放该设备i节点位图和逻辑块位图所
// 占用的高速缓冲块。如果超级块对应的文件系统是根文件系统，或者其某个i节点上已经安
// 装有其他的文件系统，则不能释放该超级块。
void put_super(int dev)
{
	struct super_block * sb;
	int i;

// 首先判断参数的有效性和合法性。如果指定设备是根文件系统设备，则显示警告信息“根系
// 统盘改变了，准备生死决战吧”，并返回。然后在超级块中寻找指定设备号的文件系统超
// 级块。如果该超级块指明该文件系统所安装到的i节点还没有被处理过，则显示警告信息并
// 返回。在文件系统卸载（umount）操作中，s_imount会先被置成Null以后才会调用本函数，
// 参见第192行。
	if (dev == ROOT_DEV) {
		printk("root diskette changed: prepare for armageddon\n\r");
		return;
	}
	if (!(sb = get_super(dev)))
		return;
	if (sb->s_imount) {
		printk("Mounted disk changed -tssk, tssk\n\r");
		return;
	}
// 然后在找到指定设备的超级块之后，我们先锁定该超级块，再置该超级块对应的设备号字段
// d_dev为0，也即释放该设备上的文件系统超级块。然后释放该超级块占用的其他内核资源，
// 即释放该设备上文件系统i节点位图和逻辑块位图在缓冲区中所占用的缓冲块。下面常数符
// 号I_MAP_SLOTS和Z_MAP_SLOTS均等于8，用于分别指明i节点位图和逻辑块位图占用的磁
// 盘逻辑块数。注意，若这些缓冲块内容被修改过，则需要作同步操作才能反缓冲块中的数据
// 写入设备中。函数最后对该超级块解锁，并返回。
	lock_super(sb);
	sb->s_dev = 0;				// 置超级块空闲。
	for (i = 0; i < I_MAP_SLOTS; i++)
		brelse(sb->s_imap[i]);
	for (i = 0; i < Z_MAP_SLOTS; i++)
		brelse(sb->s_zmap[i]);
	free_super(sb);
	return;
}

/// 读取指定设备的超级块。
// 如果指定设备dev上的文件系统超级块已经在超级块表中，则直接返回该超级块项的指针。
// 否则就从设备dev上读取超级块到缓冲块中，并复制到超级块表中。并返回超级块指针。
static struct super_block * read_super(int dev)
{
	struct super_block * s;
	struct buffer_head * bh;
	int i, block;

// 首先判断参数的有效性，然后检查该设备是否已更换过盘片（也即是否是软盘设备）。如果更
// 换过盘，则高速缓冲区有关该设备的所有缓冲块均失效，需要进行失效处理，即释放原来加载
// 的文件系统。
	if (!dev)
		return NULL;
	check_disk_change(dev);
// 如果该设备的超级块已经在超级块表中，则直接返回该超级块的指针。否则就在超级块数组
// 中找出一个空闲项（也即字段s_dev=0的项）。如果数组已经点满则返回空指针。
	if ((s = get_super(dev)))
		return s;
	for (s = 0+super_block; ; s++) {
		if (s >= NR_SUPER+super_block)
			return NULL;
		if (!s->s_dev)
			break;
	}
// 在超级块数组中找到空闲项之后，就将该超级块项用于指定设备dev上的文件系统。于是对
// 该超级块结构中的内存字段进行部分初始化处理。
	s->s_dev = dev;				// 用于dev设备上的文件系统。
	s->s_isup = NULL;
	s->s_imount = NULL;
	s->s_time = 0;
	s->s_rd_only = 0;
	s->s_dirt = 0;
// 然后锁定该超级块，并从设备上读取超级块信息到bh指向的缓冲块中。超级块位于块设备的
// 第2个逻辑块（1号块）中，（第1个是引导盘块）。如果读超级块操作失败，则释放上面选
// 定的超级块数组中的项（即置s_dev=0），并解锁该项，返回空指针退出。否则就将读取的超
// 级块信息从缓冲块数组区复制到超级块数组相应项结构中，并释放存放读取信息的高速缓冲块。
	lock_super(s);
	if (!(bh = bread(dev, 1))) {
		s->s_dev = 0;
		free_super(s);
		return NULL;
	}
	*((struct d_super_block *) s) =
		*((struct d_super_block *) bh->b_data);
	brelse(bh);
// 现在我们从设备dev上得到了文件系统的超级块，于是开始检查该超级块的有效性，并从设
// 备上读取i节点位图和逻辑块位图等信息。如果所读取的超级块的文件系统魔数字段不对，
// 说明设备上不是正确的文件系统，因此同上面一样，释放上面选定的超级块数组中的项，并
// 解锁该项，返回空指针退出。对于该版Linux内核，只支持MINIX文件系统1.0版本，其魔
// 数是0x137f。
	if (s->s_magic != SUPER_MAGIC) {
		s->s_dev = 0;
		free_super(s);
		return NULL;
	}
// 下面开始读取设备上i节点位图和逻辑块位图数据。首先初始化内存超级块结构中位图空间。
// 然后从设备上读取i节点位图和逻辑块位图信息，并存放在超级块对应字段中。i节点位图
// 保存在设备上2号块开始的逻辑块中，共占用s_imap_blocks个块。逻辑块位图在i节点位
// 图所在的后续块中，共占用s_zmap_blocks个块。
	for (i = 0; i < I_MAP_SLOTS; i++)	// 初始化操作。
		s->s_imap[i] = NULL;
	for (i = 0; i < Z_MAP_SLOTS; i++)
		s->s_zmap[i] = NULL;
	block = 2;
	for (i = 0; i < s->s_imap_blocks; i++)	// 读取设备中i节点位图。
		if ((s->s_imap[i] = bread(dev, block)))
			block++;
		else
			break;
	for (i = 0; i < s->s_zmap_blocks; i++)	// 读取设备中逻辑块位图。
		if ((s->s_zmap[i] = bread(dev, block)))
			block++;
		else
			break;
// 如果读出的位图块数不等于位图应该占有的逻辑块数，说明文件系统位图信息有问题，超级
// 块初始化失败。因此只能释放前面申请并占用的所有资源，即释放i节点位图和逻辑块位图
// 占用的高速缓冲块、释放上面选定的超级块数组项、解锁该超级块项，并返回空指针退出。
	if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks) {
		for (i = 0; i < I_MAP_SLOTS; i++) // 释放位图占用的高速缓冲块。
			brelse(s->s_imap[i]);
		for (i = 0; i < Z_MAP_SLOTS; i++)
			brelse(s->s_zmap[i]);
		s->s_dev = 0;			// 释放选定的超级块数组项。
		free_super(s);			// 释放该超级块项。
		return NULL;
	}
// 否则一切成功。另外，由于对于申请空闲i节点的函数来讲，如果设备上所有的i节点已经
// 全被使用，则查找函数会返回0值。因此0号i节点是不能用的，所以这里将位图中第1块
// 的最低比特位置为1，以防止文件系统分配0号i节点。同样的道理，也将逻辑块位图的
// 最低位置为1。最后函数解锁该超级块，并返回超级块指针。
	s->s_imap[0]->b_data[0] |= 1;
	s->s_zmap[0]->b_data[0] |= 1;
	free_super(s);		// unlock super block
	return s;
}

/// 卸载文件系统（系统调用）。
// 参数dev_name是文件系统所在设备的设备文件名。
// 该函数首先根据参数给出的块设备文件名获得设备号，然后复位文件系统超级块中的相应字段，
// 释放超级块和位图占用的缓冲块，最后执行高速缓冲与设备上数据的同步操作。
// 若卸载操作成功则返回0，否则返回出错码。
int sys_umount(char * dev_name)
{
	struct m_inode * inode;
	struct super_block * sb;
	int dev;

// 首先根据设备文件名找到对应的i节点，并取其中的设备号。设备文件所定义设备的设备号
// 是保存在其i节点的i_zone[0]中的。参见后面namei.c程序中系统调用sys_mknod()的代
// 码第445行。另外，由于文件系统需要存放在块设备上，因此如果不是块设备文件，则放回
// 则申请的i节点dev_i，返回出错码。
	if (!(inode = namei(dev_name)))
		return -ENOENT;
	dev = inode->i_zone[0];
	if (!S_ISBLK(inode->i_mode)) {
		iput(inode);			// fs/inode.c
		return -ENOTBLK;
	}
// OK，现在上面为了得到设备号而取得的i节点已完成了它的使命，因此这里把它放回。接着我们
// 来检查卸载该文件系统的条件是否满足。如果是根文件系统，则不能被卸载，于是返回忙出错号。
// 如果在超级块表中没有找到该设备上文件系统的超级块，或者该设备上文件系统没有安装过，则
// 返回出错码。如果超级块所指明的被安装到i节点并没有置位其安装标志i_mount，则显示警
// 告信息。然后要去找i节点表，看看是否有进程在使用该设备上的文件，如果有则返回忙出错码。
	iput(inode);
	if (dev == ROOT_DEV)
		return -EBUSY;
	if (!(sb = get_super(dev)) || !(sb->s_imount))
		return -EBUSY;
	if (!sb->s_imount->i_mount)
		printk("Mounted inode has i_mount=0\n");
	for (inode = inode_table+0; inode < inode_table+NR_INODE; inode++)
		if (inode->i_dev==dev && inode->i_count)
			return -EBUSY;
// 现在该设备上文件系统的卸载条件均得到满足，因此我们可以开始实施真正的卸载操作了。
// 首先复位被被安装到的i节点的安装标志，释放该i节点。然后置超级块中被安装i节点字段
// 为空，并放回设备文件系统的根i节点，接着置超级块中被安装系统根i节点指针为空。
	sb->s_imount->i_mount = 0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
// 最后我们释放该设备上的超级块以及位图占用的高速缓冲块，并执行高速缓冲与设备上数据间的
// 同步操作，然后返回0（卸载成功）。
	put_super(dev);
	sync_dev(dev);
	return 0;
}

/// 安装（新）文件系统（系统调用）。
// 参数dev_name是设备文件名，dir_name是安装到的目录名，rw_flag被安装文件系统的可
// 读写标志。将被加载的地方必须是一个目录名，并且对应的i节点没有被其他程序占用。
// 若操作成功则返回0，否则返回出错号。
int sys_mount(char * dev_name, char * dir_name, int rw_flag)
{
	struct m_inode * dev_i, * dir_i;
	struct super_block * sb;
	int dev;

// 首先根据设备文件名找到对应的i节点，以取得其中的设备号。对于块特殊设备文件，设备
// 号在其i节点的i_zone[0]中。另外，由于文件系统必须在块设备中，因此如果不是块设备
// 文件，则放回则取得的i节点dev_i，返回出错码。
	if (!(dev_i = namei(dev_name)))
		return -ENOENT;
	dev = dev_i->i_zone[0];
	if (!S_ISBLK(dev_i->i_mode)) {
		iput(dev_i);
		return -EPERM;
	}
// OK，现在上面为了得到设备号而取得的i节点dev_i已完成了它的使命，因此这里放回该设
// 备文件的i节点。接着我们来检查一下文件系统安装到的目录名是否有效。于是根据给定的
// 目录文件名找到对应的i节点dir_i。如果该i节点的引用计数不为1（仅在这里引用），
// 或者该i节点的节点号是根文件系统的节点号1，则放回该i节距返回出错码。另外，如果
// 访哇不好看不是一个目录文件节点，则也放回该i节点，返回出错码。因为新文件系统只能安装
// 在一个目录名上。
	iput(dev_i);
	if (!(dir_i = namei(dir_name)))
		return -ENOENT;
	if (dir_i->i_count != 1 ||  dir_i->i_num == ROOT_INO) {
		iput(dir_i);
		return -EBUSY;
	}
	if (!S_ISBLK(dir_i->i_mode)) {		// 安装点需要是琴目录名。
		iput(dir_i);
		return -EPERM;
	}
// 现在安装点也检查完毕，我们开始读取要安装文件系统（新文件系统）的超级块信息。一个文件
// 系统的超级块会首先从超级块表中进行搜索，如果不在超级块表中就从设备上读取。在得到了文
// 件系统超级块之后，我们对它先进行检测一番。如果新文件系统已安装在其他地方，或者将要安
// 装到的i节点已经安装了文件系统（安装标志已经置位），则都放回该i节点，也返回出错码。
	if (!(sb = read_super(dev))) {
		iput(dir_i);
		return -EBUSY;
	}
	if (sb->s_imount) {
		iput(dir_i);
		return -EBUSY;
	}
	if (dir_i->i_mount) {
		iput(dir_i);
		return -EPERM;
	}
// 最后我们设置新文件系统超级块的“被安装到i节点”字段指向安装到的目录名的i节点，
// 并设置安装位置i节点的安装标志和节点已修改标志。然后返回0（安装成功）。
	sb->s_imount = dir_i;
	dir_i->i_mount = 1;
	dir_i->i_dirt = 1;			/* NOTE! we don't iput(dir_i) */
	return 0;				/* we do that in umount */
}

/// 安装根文件系统。
// 该函数属于系统初始化操作的一部分。函数首先初始化文件表file_table[]和超级块表，然后读
// 取根文件系统超级块，并取得根i节点。最后统计并显示根文件系统上的可用资源（空闲块数和
// 空闲i节点数）。该函数会在系统初始化时（sys_setup()）被调用（blk_drv/hd.c，157行）。
void mount_root(void)
{
	int i, free;
	struct super_block * p;
	struct m_inode * mi;

// 首先检查一个在磁盘i节点大小是否符合要求（32字节），以防止修改代码时出现不一致情况。
// 然后初始化文件表数组（共64项，即系统同时只能打开64个文件）和超级块表（8项）。这里
// 将所有文件结构中的引用计数设置为0（表示空闲），并把超级块表中各项的设备字段初始
// 化为0（也表示空闲）。如果根文件系统所在设备量软盘的话，就提示“插入根文件系统盘并按
// 回车键”，并等待按键。
	if (32 != sizeof(struct d_inode))
		panic("bad i-node size");
	for (i = 0; i < NR_FILE; i++)		// 初始化文件表（共64项）。
		file_table[i].f_count = 0;
	if (MAJOR(ROOT_DEV) == 2) {		// 提示插入根文件系统盘。
		printk("Insert root floppy and press ENTER");
		wait_for_keypress();
	}
	for (p = &super_block[0]; p < &super_block[NR_SUPER]; p++) {
		p->s_dev = 0;			// 初始化超级块表（共8项）。
		p->s_lock = 0;
		p->s_wait = NULL;
	}
// 做好以上“份外”的初始化工作之后，我们开始安装根文件系统。于是从根设备上读取文件
// 系统超级块，并取得文件系统的根i节点（1号节点）在内存i节点表中的指针。如果读根
// 设备上超级块失败或取根节点失败，则都显示信息并停机。
	if (!(p = read_super(ROOT_DEV)))
		panic("Unable to mount root");
	if (!(mi = iget(ROOT_DEV, ROOT_INO)))	// 在fs.h中ROOT_INO字义为1。
		panic("Unable to read root i-node");
// 现在我们对超级块和根i节点进行设置。把根i节点引用次数递增3次。因为随后几行代码上
// 也引用了该i节点，并且iget()函数中i节点引用计数已被设置为1。然后设置该超级块的
// 被安装文件系统根目录i节点字段（s_isup）和被安装到的i节点字段（s_imount）为该i节点，
// 再设置当前进程的当前工作目录和根目录i节点。此时当前进程是1号进程（init进程）。
	mi->i_count += 3;			/* NOTE! it is logically used 4 times, not 1 */
						/* 注意！从逻辑上讲，它已被引用了4次，而不是1次 */
	p->s_isup = p->s_imount = mi;
	current->pwd = mi;
	current->root = mi;
// 然后我们对根文件系统上的资源作统计工作，统计该设备上空闲块数和空闲i节点数。首先令i
// 等于超级块中表明的设备逻辑块总数，然后根据逻辑块位图中相应比特位的占用情况统计出空闲
// 块数。这里宏函数set_bit()只测试比特位。“i&8191”用于取得i节点号在当前位图块中对应的
// 比特位偏移值。“i>>13”是将i除以8192，也即除一个磁盘块包吃住的比特位数。
	free = 0;
	i = p->s_nzones;
	while (--i >= 0)
		if (!set_bit(i&8191, p->s_zmap[i>>13]->b_data))
			free++;
// 在显示过设备上空闲逻辑块数/逻辑块总数之后，我们再统计设备上空闲i节点数。首先令i
// 等于超级块中表明的设备i节点总数+1（加1是将0节点也统计进去），然后根据i节点位
// 图中相应比特位的占用情况计算出空闲i节点数。最后再显示设备上可用空闲i节点数和i
// 节点总数。
	printk("%d/%d free blocks\n\r", free, p->s_nzones);
	free = 0;
	i = p->s_ninodes+1;
	while (--i >= 0)
		if (!set_bit(i&8191, p->s_imap[i>>13]->b_data))
			free++;
	printk("%d/%d free inodes\n\r", free, p->s_ninodes);
}

