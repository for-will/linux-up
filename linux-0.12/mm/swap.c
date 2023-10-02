/*
 * linux/mm/swap.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This file should contain most things doing the swapping from/to disk.
 * Started 18.12.91
 */

#include "linux/fs.h"
#include "sys/types.h"
#include <string.h>		// 字符串头文件。定义了一些有关内存或字符串操作的嵌入函数。

#include <linux/mm.h>		// 内存管理头文件。定义页面长度，和一些内存管理函数原型。
#include <linux/sched.h>	// 调度程序头文件。定义了任务结构task_struct、任务0的数据，
				// 还有一些有关描述符参数设置和获取的嵌入式汇编函数宏语句。 
#include <linux/head.h>		// head头文件。定义了段描述符的简单结构，和几个选择符常量。
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用函数的原型定义。

// 每个字节8位，因此1页（4096字节）共有32768个比特位。若1个比特位对应1个页面，
// 则最多可管理32768个页面，对应128MB内存容量。处于置位状态表示相应交换页是空闲的。
#define SWAP_BITS (4096<<3)	/* 定义一个页面含有的交换比特位数量 */

// 比特位操作宏。骑过给定不同“op”，可定义对指定比特位进行测试、设置或清除三种操作。
// 参数addr是指定线性地址；nr是指定地址处开始的比特位偏移位。该宏把给定地址addr处
// 第nr个比特位的值放入进位标志，设置或复位该比特位并返回进位标志值（即原比特位值）。
// 第25行上第一个指令随“op”字符的不同而组合形成不同的指令：
// 当“op”= “”时，就是指令bt - （Bit Test）测试并用原值设置进位位。
// 当“op”=“s”时，就是指令bts - （Bit Test and Set）设置比特位值并用原值设置进位位。
// 当“op”=“r”时，就是指令btr - （Bit Test and Reset）复位比特位并用原值设置进位位。
// 输入：%0 - （返回值），%1 - 位偏移（nr）；%2 - 基址（addr）；%3 - 加操作寄存器初值（0）。
// 内嵌汇编代码把基地址（%2）和比特偏移值（%1）所指定的比特位值先保存到进位标志CF中，
// 然后设置（复位）该比特位。指令adcl是带进位位加，用于根据进位位CF设置操作数（%0）.
// 如果CF = 1则返回寄存器值 = 1，否则返回寄存器值 = 0。
#define bitop(name, op) \
static inline int name(char * addr, unsigned int nr) \
{ \
int __res; \
__asm__ __volatile__("bt" op " %1,%2; adcl $0,%0"\
:"=g" (__res) \
:"r" (nr),"m" (*(addr)),"0" (0)); \
return __res; \
}

// 这里根据不同的op字符定义3个内嵌函数。
bitop(bit, "")			/* 定义函数 bit(char * addr, unsigned int nr)  */
bitop(setbit, "s")		/* 定义函数 setbit(char * addr, unsigned int nr)  */
bitop(clrbit, "r")		/* 定义函数 clrbit(char * addr, unsigned int nr)  */

static char * swap_bitmap = NULL;
int SWAP_DEV = 0;		// 内核初始化时设置的交换设备号。

/*
 * We never page the pages in task[0] - kernel memory.
 * We page all other pages.
 */
// 第1个虚拟内存页面。即从任务0末端（64MB）处开始的虚拟内存页面。
#define FIRST_VM_PAGE (TASK_SIZE>>12) 		/* = 64MB/4KB = 16384 */
#define LAST_VM_PAGE (1024*1024)      		/* = 4GB/4KB = 1048576 */
#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE) /* = 1032192（从0开始计） */

/// 申请取得一交换页面号。
// 扫描整个交换映射位图（除对应位图本身的位0以外），复位值为1的第一个比特位，并返回
// 其位置值，即目前空闲的交换页面号。若操作成功则返回交换页面号，否则返回0。
static int get_swap_page(void)
{
	int nr;

	if (!swap_bitmap)
		return 0;
	for (nr = 1; nr < 32768; nr++)
		if (clrbit(swap_bitmap, nr))
			return nr; 		// 返回目前空闲的交换页面号。
	return 0;
}

/// 释放交换设备中指定的交换页面。
// 在交换位图中设置指定的页面号对应的比特位（置位表示空闲）。若原来该比特位就等于1，则
// 表示交换设备中原来该页面就没有被占用，或者位图出错。于是显示出错信息并返回。
// 参数指定交换页面号。
void swap_free(int swap_nr)
{
	if (!swap_nr)
		return;
	if (swap_bitmap && swap_nr < SWAP_BITS)
		if (!setbit(swap_bitmap, swap_nr))
			return;
	printk("Swap-space bad (swap_free())\n\r");
	return;
}

/// 把指定页面交换进内存中。
// 把指定页表项对应的内存页面从交换设备中读入到新申请的内存页面中。同时修改交换位图中
// 对应比特位（置位），以及修改页表项内容，让它指向该内存页面，并设置相应标志。
// 参数table_ptr是页表项指针。
void swap_in(unsigned long *table_ptr)
{
	int swap_nr;
	unsigned long page;

// 首先检查交换位图和参数有效性，如果交换位图不存在，或者指定页表项对应的页面已存在
// 于内存中，或者交换页面号为0，则显示警告信息并退出。对于已放到交换设备中去的内存
// 页面，相应页表项中存放的应是交换页面号*2，即（swap_nr << 1），参见下面第111行上交
// 换函数try_to_swap_out()中的说明。
	if (!swap_bitmap) {
		printk("Trying to swap in withou swap bit-map");
		return;
	}
	if (1 & *table_ptr) {
		printk("trying to swap in present page\n\r");
		return;
	}
	swap_nr = *table_ptr >> 1;
	if (!swap_nr) {
		printk("No swap page in swap_in\n\r");
		return;
	}
// 然后申请一页物理内存并从交换设备中读入页面号为swap_nr的页面。在用read_swap_page()
// 把页面交换进来后，就把交换位图中对应比特位置位。如果其原本就是置位的，说明此次是再次
// 从交换设备中读入相同的页面，于是显示一下警告信息。最后让页表项指向该物理页面，并设置
// 页面已修改、用户可读写和存在标志（Dirty、U/S、R/W、P）。
	if (!(page = get_free_page()))
		oom();
	read_swap_page(swap_nr, (char *) page);	// 在include/linux/mm.h中定义。
	if (setbit(swap_bitmap, swap_nr))
		printk("swapping in multiply from same page\n\r");
	*table_ptr = page | (PAGE_DIRTY | 7);
}

/// 尝试把页面交换出去。
// 若页面没有被修改过则不用保存在交换设备中，因为对应页面还可以再直接从相应映像文件
// 中读入。于是可以直接释放掉相应物理页面了事。否则就申请一个交换页面号，然后把页面
// 交换出去。此时交换页面号要保存在对应页表项中，并且仍需要保持页表项存在位P = 0。
// 参数是页表项指针。页面交换或释放成功返回1，否则返回0。
int try_to_swap_out(unsigned long * table_ptr)
{
	unsigned long page;
	unsigned long swap_nr;

// 首先判断参数有有效性。若需要交换出去的内存页面并不存在（或称无效），则即可退出。
// 若页表项指定的物理页面地址大于分页管理的内存高端PAGING_MEMORY（15MB），也退出。
	page = *table_ptr;
	if (!(PAGE_PRESENT & page))
		return 0;
	if (page - LOW_MEM > PAGING_MEMORY)
		return 0;
// 若内存页面已修改过，但是该页面是被共享的，那么为了提高运行效率，此类页面不宜
// 被交换出去，于是直接退出，函数返回0。否则就申请一交换页面号，并把它保存在页表
// 项中，然后把页面交换出去并释放对应物理内存页面。
	if (PAGE_DIRTY & page) {
		page &= 0xfffff000;		// 取物理页面地址。
		if (mem_map[MAP_NR(page)] != 1)
			return 0;
		if (!(swap_nr = get_swap_page())) // 申请交换页面号。
			return 0;
// 对于要到交换设备中的页面，相应页表项中将存放的是（swap_nr << 1）。乘2（左移1位）
// 是为了空出原来页表项的存在位（P）。只有存在位P=0并且页表项内容不为0的页面才会
// 在交换设备中。 Intel手册中明确指出，当一个表项的存在位P = 0时（无效页表项），
// 所有其他位（位32--1）可供随意使用。下面写交换页函数write_swap_page(nr, buffer)
// 被定义为ll_rw_page(WRITE, SWAP_DEV, (nr), (buffer))。参见linux/mm.h文件第12行。
		*table_ptr = swap_nr<<1;
		invalidate();			// 刷新CPU页变换高速缓冲。
		write_swap_page(swap_nr, (char *) page);
		free_page(page);
		return 1;
	}
// 否则表明页面没有修改过，那么就不用交换出去，而直接释放即可。
	*table_ptr = 0;
	invalidate();
	free_page(page);
	return 1;
}

/*
 * Ok, this has a rather intricate logic - the idea is to make good
 * and fast machine code. If we didn't worry about that, things would
 * be easier.
 *
 * OK，这个函数中有一个非常复杂的逻辑 - 用于产生逻辑性好并且速度快的
 * 机器码。如果我们不对此操心的话，那么事情可能更容易些。
 */
/// 把内存页面放到交换设备中。
// 从线性地址64MB对应的页目录项（FIRST_VM_PAGE >> 10）开始，搜索整个4GB线性空间。期间
// 我们尝试把对应的物理内存页面交换到交换设备中去。一旦成功地换出一个页面，就返回1，否
// 则返回0。函数中两个静态变量用于暂存当前搜索点，用于下次搜索时的起始位置。该函数会在
// get_free_page()中被调用（该函数在下面172行）。
int swap_out(void)
{
	static int dir_entry = FIRST_VM_PAGE>>10; // 即任务1的第1个目录项索引。
	static int page_entry = -1;
	int counter = VM_PAGES;			// 页面总数：1032192，见上面44行。
	int pg_table;

// 首先我们循环搜索页目录表，查找包含有效二级页表内容的页目录项pg_table，找到则退出循
// 环，否则调整该页目录项对应剩余二级表数counter，然后继续检测下一页目录项。若全部搜
// 索完还没有找到适合的（存在的）页目录项，则退出函数。
	while (counter > 0) {
		pg_table = pg_dir[dir_entry];	// 页目录项内容。
		if (pg_table & 1)
			break;
		counter -= 1024;		// 1个页表对应1024个页帧。
		dir_entry++;			// 下一目录项。
		if (dir_entry > 1024)
			dir_entry = FIRST_VM_PAGE>>10;
	}
// 在取得当前目录项中的页表指针后，针对该页表中的所有1024个页面，逐一调用交换函数
// try_to_swap_out()尝试把它交换出去。一旦某个页面成功交换到到交换设备中就返回1.若对
// 所有目录项的所有页表都已尝试失败，则显示“交换内存用完”的警告，并返回0。
	pg_table &= 0xfffff000;			// 页表指针（地址）。
	while (counter-- > 0) {
		page_entry++;			// 页表项索引（初始为-1）。
// 如果已经尝试处理完当前页表中的所有项，但还是没有找到一个能成功地交换出的页面，即此
// 时页表项索引大于等于1024，则使用如同前面第135 - 143行语句相同的处理方式来先出下一
// 个存在的二级页表。
		if (page_entry >= 1024) {
			page_entry = 0;
repeat:
			dir_entry++;
			if (dir_entry >= 1024)
				dir_entry = FIRST_VM_PAGE>>10;
			pg_table = pg_dir[dir_entry];
			if (!(pg_table & 1))
				if ((counter -= 1024) > 0)
					goto repeat;
				else
					break;
			pg_table &= 0xfffff000;	// 页表指针。
		}
		if (try_to_swap_out(page_entry + (unsigned long *) pg_table))
			return 1;
	}
	printk("Out of swap-memory\n\r");
	return 0;
}

/*
 * Get physcal address of first (actually last :-) free page, and mark it
 * used. If no free page left, return 0.
 *
 * 获取首个（实际上是最后1个：-）空闲页面，并标记为已使用。如果没有空闲页面，
 * 就返回0。
 */
/// 在主内存区中申请取得一空闲物理页面。
// 如果已经没有可用物理内存页面，则调用执行交换处理，然后再次申请页面。
// 输入：%1(ax = 0) - 0；%2(LOW_MEM)字节位图管理的内存起始位置；%3(cx = PAGING_PAGES)；
// %4(edi = mem_map + PAGING_PAGES - 1)。
// 输出：返回%0(ax = 物理页面起始地址)，即函数返回新页面的物理内存地址。
// 上面%4寄存器实际指向内存字节位图mem_map[]的最后一个字节。本函数从位图末端开始向前
// 扫描所有页面标志（页面总数为PAGING_PAGES），若有页面空闲（内存位图字节为0）则返回
// 页面地址。 注意！本函数只是指出在主内存区的一页空闲物理页面，但并没有映射到某个进程的
// 地址空间中。当然对于内核使用本函数时并不需要再使用put_page()进行映射，因为内核代码
// 和数据空间（16MB）已经对等地映射到物理地址空间中。
// 第174行定义了一个局部寄存器变量。该变量将被保存在eax寄存器中，以便于高效访问和操作。
// 这种定义变量的方法主要用于内嵌汇编程序中，详细说明见gcc手册“在指定寄存器中的变量”。
unsigned long get_free_page(void)
{
	register unsigned long __res asm("ax");

// 首先在内存映射字节位图中查找值为0的字节项，然后把对应物理内存页面清零。如果得到
// 的页面地址大于实际物理内存容量则重新寻找。如果没有找到空闲页面则去调用执行交换处
// 理，并热更新查找。最后返回空闲物理页面地址。
repeat:
	__asm__("std ; repne ; scasb\n\t"	// 设置方向位；al(0)与对应每个页面的（di）内容比较，
		"jne 1f\n\t"			// 如果没有等于0的字节，则跳转结束（返回0）。
		"movb $1,1(%%edi)\n\t"		// 1 => [1+edi]，将对应页面内存映像比特位置1。
		"sall $12,%%ecx\n\t"		// 页面数*4K = 相对页面起始地址。
		"addl %2,%%ecx\n\t"		// 再加上低端内存地址，得页面实际物理起始地址。
		"movl %%ecx,%%edx\n\t"		// 将页面实际起始地址->edx寄存器。
		"movl $1024,%%ecx\n\t"		// 寄存器ecx置计数值1024。
		"leal 4092(%%edx),%%edi\n\t"	// 将4092+edx的位置->edi（该页面的末端）。
		"rep ; stosl\n\t"		// 将edi所指内存清零（反方向，即将该页面清零）。
		"movl %%edx,%%eax\n"		// 将页面起始地址->eax(返回值)。
		"1:"
		:"=a" (__res)
		:"0" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),
		"D" (mem_map+PAGING_PAGES-1)
		: /* "di","cx","dx" */);
	if (__res >= HIGH_MEMORY)		// 页面地址大于实际内存容量则重新寻找。
		goto repeat;
	if (!__res && swap_out())		// 若没得到空闲页面则执行交换处理，并重新查找。
		goto repeat;
        return __res;				// 返回空闲物理页面地址。
}

/// 内存页面交换初始化。
// 函数首先根据设备的分区数组（块数数组）检查系统是否有交换设备，并且交换设备有效。然后
// 申请取得一页内存来存放交换页面位映射数组swap_bitmap[]。然后从交换设备的交换分区把交
// 换管理页面（第一个页面）读入该位映射数组中。然后仔细检查该交换位映射粉丝通中每个比特位
// 是否正常（为0），最后返回。
void init_swapping(void)
{
// blk_size是指向指定主设备号的块设备的块数数组。它的每一项对应一个子设备上所拥有的数
// 据块总数，每个子设备对应设备的一个分区。如果没有定义交换设备则返回；如果交换设备没有
// 设置块数数组，则显示警告信息并返回。
	extern int *blk_size[];			// blk_drv/ll_rw_blk.c，49行。
	int swap_size,i,j;

	if (!SWAP_DEV)
		return;
	if (!blk_size[MAJOR(SWAP_DEV)]) {
		printk("Unable to get size of swap device\n\r");
		return;
	}
// 然后取得并检查指定交换设备号的交换分区数据块总数swap_size。若为0则返回，若总块数小
// 于100块则显示信息“交换设备区太小”，然后退出。
	swap_size = blk_size[MAJOR(SWAP_DEV)][MINOR(SWAP_DEV)];
	if (!swap_size)
		return;
	if (swap_size < 100) {
		printk("Swap device too small (%d blocks)\n\r", swap_size);
		return;
	}
// 然后我们把交换数据块总断转换成对应可交换页面总数。该值不能大于SWAP_BITS所能表示的页
// 面数。即交换页面总数不得大于32768。然后申请一页物理内存用来存放交换页面位映射数组
// swap_bitmap，其中每1比特代表1页交换页面。
	swap_size >>= 2;
	if (swap_size > SWAP_BITS)
		swap_size = SWAP_BITS;
	swap_bitmap = (char *) get_free_page();
	if (!swap_bitmap) {
		printk("Unable to start swapping: out of memory :-)\n\r");
		return;
	}
// 随后把设备交换分区上的页面0读到swap_bitmap页面中。该页面是交换区管理页面。其中第
// 4096字节开始处含有10个字符的交换设备特征字符串“SWAP-SPACE”。若没有找到该特征字
// 符串，则说明不是一个有效的交换设备。于是显示信息，释放则申请的物理页面并退出函数。
// 否则将特征字符串字节清零。宏read_swap_page(nr, buffer)定义在linux/mm.h文件中。
	read_swap_page(0, swap_bitmap);
	if (_strncmp("SWAP-SPACE", swap_bitmap, 10)) {
		printk("Unable to find swap-space signature\n\r");
		free_page((long) swap_bitmap);
		swap_bitmap = NULL;
		return;
	}
	_memset(swap_bitmap+4086, 0, 10);
// 然后我们检查读入的交换位图，其中共有32768个比特位。若位图中的比特位为0，则表示设备
// 上对应交换页面已使用（占用），若比特位为1，则表示对应交换页面可用（空闲）。因此对于
// 设备上的交换分区，第一个页面（页面0）被用作交换管理，已占用（位为0）。而交换页面
// [1 -- swap_size-1]是可用的，因此它们在你准备中对应的比特位应该均为1（空闲）。位图中
// [swap_size -- SWAP_BITS]范围的比特位因为无对应交换页面，所以它们也应该被初始化为0
// （占用）。下面在检查位图时就根据不可用和可用部分，分两步对位图进行检查。
// 首先检查不可用交换页面的位图比特位，它们应均为0（占用）。若其比特位是1（表示空闲），
// 则表示位图有问题。于是显示出错信息、释放位图占用的页面并退出函数。不可用比特位有：
// 比特位0和比特位范围[swap_size - 1, SWAP_BITS]。
	for (i = 0; i < SWAP_BITS; i++) {
		if (i == 1)
			i = swap_size;
		if (bit(swap_bitmap, i)) {
			printk("Bad swap-space bit-map\n\r");
			free_page((long) swap_bitmap);
			swap_bitmap = NULL;
			return;
		}
	}
// 然后再检查和统计[位1到位swap_size-1]之间的所有比特位是否为1（空闲）。若统计得出没
// 有空闲的交换页面，则表示交换功能有问题，于是释放位图占用的页面并退出函数。否则显示交
// 换设备工作正常以及交换页面数和交换空间总字节数。
	j = 0;
	for (i = 1; i < swap_size; i++)
		if (bit(swap_bitmap, i))
			j++;
	if (!j) {
		free_page((long) swap_bitmap);
		swap_bitmap = NULL;
		return;
	}
	printk("Swap device ok: %d pages (%d bytes) swap-space\n\r", j, j*4096);
}
