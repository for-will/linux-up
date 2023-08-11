/*
 *  linux/kernel/blk_drv/ramdisk.c
 *
 * Writen by Theodore Ts'o, 12/2/91
 */
// Thedore Ts'o (Ted Ts'o)是Linux社区中的著名人物。Linux在世界范围内的流行也有他很大
// 的功劳。早在Linux操作系统刚问世时，他就怀着极大的热情为Linux的发展提供了电子邮件列
// 表服务maillist，并在北美地区最早设立了Linux的ftp服务器站点（tsx-11.mit.edu）。他对
// Linux作出的最大贡献之一是提出并实现了ext2文件系统。该文件系统已成为Linux世界中事实
// 上的文件系统标准。最近他又推出了ext3、ext4文件系统，大大提高了文件系统的稳定性、可
// 恢复性和访问效率。作为对他的推崇，第97期（2002年5月）的LinuxJournal期刊将他作为了
// 封面人物，并对他进行了采访。目前他为Google公司工作，仍然人事着有关文件系统和存储方面
// 的工作。（他的个人主页是：http://thunk.org/tytso/）

#include <string.h>		// 字符串头文件。主要定义了一些有关字符串操作的嵌入函数。

#include <linux/config.h>	// 内核配置头文件。定义键盘语言和硬盘类型（HD_TYPE）可选项。
#include <linux/sched.h>	// 调度程序头文件。定义了任务结构task_struct、任务0的数据，
				// 还有一些有关描述符参数设置各获取的嵌入式汇编函数宏语句。
#include <linux/fs.h>		// 文件系统头文件。定义文件表结构（file、m_inode）等。
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用函数的原型定义。
#include <asm/system.h>		// 系统头文件。定义了设置或修改描述符/中断门等嵌入式汇编宏。
#include <asm/segment.h>	// 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。
#include <asm/memory.h>		// 内存拷贝头文件。含有memcpy()嵌入式汇编宏函数。

// 定义RAM盘主设备号符号常数。在驱动程序中主设备号必须在包含blk.h文件之前被定义。
// 因为blk.h文件中要用到这个符号常数值来确定一系列的其他常数符号和宏。
#define MAJOR_NR 1
#include "blk.h"

// 虚拟盘在内存中的起始位置。该位置会在52行上初始化函数rd_init()中确定。参见内核
// 初始化程序 init/main.c ，第151行。‘rd’是’ramdisk’的缩写。
char 	* rd_start;		// 虚拟盘在内存中的开始地址。
int 	rd_lenght = 0;		// 虚拟盘所占内存大小（字节）。

// 虚拟盘当前请求项操作函数。
// 该函数的程序结构与硬盘的do_hd_request()函数类似，参见hd.c，330行。在低级块设备
// 接口函数 ll_rw_block() 建立起虚拟盘（rd）的请求项并添加到rd的链表中之后，就会调
// 用该函数对rd当前请求项进行处理。该函数首先计算当前请求项中指定的起始扇区对应虚
// 拟盘所处内存的起始位置addr和要求的扇区数对应的字节长度值len，然后根据请求项中
// 的命令进行操作。若是写命令WRITE，就把请求项所指缓冲区中的数据直接复制到内存位置
// addr处。若是读操作则反之。数据复制完成后即可直接调用 end_request() 对本次请求项
// 作结束处理。然后跳转到函数开始处再去处理下一个请求项。若已没有请求项则退出。
void do_rd_request(void)
{
	int	len;
	char	*addr;
// 首先检测请求项的合法性，若已没有请求项则退出（参见blk.h，第248行）。然后计算请
// 求项处理的虚拟盘中起始扇区在物理内存中对应的地址addr和占用的内存字节长度值len。
// 下句用于取得请求项中的起始扇区对应的内存起始位置和内存长度。其中sector << 9表示
// sector * 512，换算成字节值。CURRENT被定义为（blk_dev[MAJOR_NR].current_request）。  	
	INIT_REQUEST;
	addr = rd_start + (CURRENT->sector << 9);
	len = CURRENT->nr_sectors << 9;
// 如果当前请求项中子设备号不为1或者对应内存起始位置大于虚拟盘末尾，则结束该请求项，
// 并跳转到 reqpeat 处去处理下一个虚拟盘请求项。标号 repeat 定义在宏 INIT_REQUEST 内，
// 位于宏的开始处，参见blk.h文件第149行。
	if (MINOR(CURRENT->dev) != 1 || (addr + len) > rd_start+rd_lenght) {
	        end_request(0);
		goto repeat;
	}
// 然后进行实际的读写操作。如果是写命令（WRITE），则将请求项中缓冲区的内容复制到地址
// addr处，长度为len字节。如果是读命令（READ），则将addr开始的内存内容复制到请求项
// 缓冲区中，长度为len字节。否则显示命令不存在，死机。
	if (CURRENT->cmd == WRITE) {
	        (void) memcpy(addr,
			      CURRENT->buffer,
			      len);
	}else if (CURRENT->cmd == READ) {
	        (void) memcpy(CURRENT->buffer,
			      addr,
			      len);
	} else
		panic("unknown ramdisk-command");
// 然后在请求项成功后处理，置更新标志，并继续处理本设备的下一请求项。
	end_request(1);
	goto repeat;
}

/*
 * Returns amout of memory which needs to be reserved.
 * 返回内存虚拟盘ramdisk所需的内存量
 */
// 虚拟盘初始化函数，取得保留给虚拟盘的内存容量。
// 该函数首先设置虚拟盘设备的请求项处理函数指针do_rd_request()，然后确定虚拟盘
// 在物理内存中的起始地址、占用字节长度值，并对整个虚拟盘区清零。然后返回盘区长度。
// 当linux/Makefile文件中设置过RAMDISK值不为零时，表示系统中会创建RAM虚拟盘设备。
// 在这种情况下的内核初始化过程中，本函数就会调用（init/main.c，L151行）。该函数
// 的第2个参数length会被赋值成RAMDISK * 1024，单位为字节。
long rd_init(long mem_start, int length)
{
	int 	i;
	char 	*cp;

	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;	// do_rd_request()。
	rd_start = (char *) mem_start;			// 对于16MB系统该值为4MB。
	rd_lenght = length;				// 虚拟盘区域长度值。
	cp = rd_start;
	for (i = 0; i < length; i++) 			// 盘区清零。
		*cp++ = '\0';
	return (length);
}

/*
 * If the root device is the ram disk, try to load it.
 * In order to do this, the root device is originally set to the
 * floppy, and we later change it to be ram disk.
 *
 * 如果根文件系统设备（root device）是ramdisk的话，则尝试加载它。
 * root device原先是指向软盘的，我们将它改成指向ramdisk。
 */
//// 尝试把根文件系统加载到虚拟盘中。
// 该函数将在内核设置函数setup()（hd.c，162行）中被调用。另外，1磁盘块 = 1024字节。
// 第75行上的变量block=256表示根文件系统映像文件被存储于boot盘第246磁盘块开始处。
void rd_load()
{
	struct buffer_head *bh;			// 高速缓冲块头文件。
	struct super_block s;			// 文件超级块结构。
	int 	block = 256;			/* Start at block 256 */
	int	i = 1;
	int 	nblocks;			// 文件系统盘块总数。
	char 	*cp;				/* Move pointer */

// 首先检查虚拟盘的有效性和完事性。如果ramdisk的长度为零，则退出。否则显示ramdisk
// 的大小以及内存起始位置。如果此时根文件设备不是软盘设备，则也退出。
	if (!rd_lenght)
		return;
	printk("Ram disk: %d bytes, starting at 0x%x\n",rd_lenght,
	       (int) rd_start);
	if (MAJOR(ROOT_DEV) != 2)
		return;
// 然后读根文件系统的基本参数，即读软盘块256+1、256和256+2。这里block+1是指虚拟盘的超
// 级块。breada()用于从磁盘上读取指定的数据块，并标出还需要读的块，然后返回含有数据块的
// 缓冲区指针。如果返回NULL，则表示数据块不可读（fs/bufer.c，322）。然后把缓冲区中的磁
// 盘超级块（d_super_block是磁盘超级块结构）复制到s变量中，并释放缓冲区。接着我们开始
// 对超级块的有效性进行判断。如果超级块中文件系统魔数不对，则说明加载的数据块不是MINIX
// 文件系统，于是退出。有关MINIX超级块的结构请参见文件系统一章内容。
	bh = breada(ROOT_DEV, block + 1, block, block + 2, -1);
	if (!bh) {
	        printk("Disk error while looking for ramdisk!\n");
		return;
	}
	*((struct d_super_block *) &s) = *((struct d_super_block *) bh->b_data);
	brelse(bh);
	if (s.s_magic != SUPER_MAGIC)
		/* No ram disk image present, assume normal floppy boot */
		/* 磁盘中没有ramdisk映像文件，退出去执行通常的软盘引导 */
		return;
// 然后我们尝试把整个文件系统读入到内存虚拟盘中。对于一个文件系统来说，其超级块结构
// 的s_nzone字段中保存着总逻辑块数（或称为区段数）。一个逻辑块中含有的数据块数则由字
// 段s_log_zone_size指定。因为文件系统中的数据块总数就等于 (逻辑块数 * 2^(每区
// 段场数的次方))，即nblocks = (s_nzones * 2^s_log_zone_size)。如果遇到文件系统中数据块
// 总数大于内存虚拟盘所能容纳的场数的情况，则不能执行加载操作，而只能显示出错信息并返回。
	nblocks = s.s_nzones << s.s_log_zone_size;
	if (nblocks > (rd_lenght >> BLOCK_SIZE_BITS)) {
	        printk("Ram disk image too big! (%d blocks, %d avail)\n",
		       nblocks, rd_lenght >> BLOCK_SIZE_BITS);
		return;
	}
// 否则若虚拟盘能容纳得下文件系统总数据块数，则我们显示加载数据块信息，并让cp指向内存
// 虚拟盘起始处。然后开始执行循环操作将磁盘上根文件系统映像文件加载到虚拟盘上。在操作过
// 程中，如果一次需要加载的盘块数大于2块，我们就是用超前预读函数breada()，否则就使用
// bead()函数进行单块读取。若在读盘过程中出现I/O操作错误，就只能放弃加载过程返回。所
// 读取的磁盘块会使用memcpy()函数从调整缓冲区中复制到内存虚拟盘相应位置处，同时显示已
// 加载的场数。显示字符串中的八进制数‘010’表示显示一个制表符。
	printk("Loading %d bytes into ram disk... 000k",
	       nblocks << BLOCK_SIZE_BITS);
	cp = rd_start;
	while (nblocks) {
		if (nblocks > 2)				// 若读取块数多于2块则采用超前预读。
			bh = breada(ROOT_DEV, block, block+1, block+2, -1);
		else						// 否则就单块读取。
		        bh = bread(ROOT_DEV, block);
		if (!bh) {
		        printk("I/O error on block %d, aborting load\n",
			       block);
			return;
		}
		(void) memcpy(cp, bh->b_data, BLOCK_SIZE);	// 复制到cp处。
		brelse(bh);
		printk("\010\010\010\010%4dk", i);		// 打印加载块计数值。
		cp += BLOCK_SIZE;				// 虚拟盘指针前移。
		block++;
		nblocks--;
		i++;
	}
// 当boot盘中从256盘块开始的整个根文件系统加载完毕后，我们显示“done”，并把目前
// 根文件系统设备号修改成虚拟盘的设备号0x0101，最后返回。
	printk("\010\010\010\010\010done \n");
	ROOT_DEV = 0x0101;
}
