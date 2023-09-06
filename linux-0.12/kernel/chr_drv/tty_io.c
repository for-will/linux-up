/*
 *  linux/kernel/chr_drv/tty_io.c
 *
 *  (C) 1991 Linus Torvalds
 */

/*
 * 'tty_io.c' gives an orthogonal feeling to tty's, be they consoles
 * or rs-cannels. It also implements echoing, cooked mode etc.
 *
 * Kill-line thanks to John T Kohl, who also corrected VMIN = VTIME = 0.
 *
 * ‘tty_io.c’给tty终端一种非相关的感觉，不管它们是控制台还是串行终端。
 * 该程序同样实现了回显、规范（熟）模式等。
 *
 * Kill-line，谢谢John T Kohl。他同时还纠正了当 VMIN = VTIME = 0 时的问题。
 */

#include "sys/types.h"
#include "termios.h"
#include <ctype.h>		// 字符类型头文件。定义了一些有关字符类型判断和转换的宏。
#include <errno.h>		// 错误号头文件。包含系统中各种出错号。
#include <signal.h>		// 信号头文件。定义信号符号常量，信号结构及其操作函数原型。
#include <unistd.h>		// unistd.h是标准符号常数与类型文件，并声明了各种函数。

// 给出定时警告（alarm）信号在信号位图中对应的比特屏蔽位。
#define ALRMMASK (1<<(SIGALRM-1))

#include <linux/kernel.h>
#include <linux/sched.h>	// 调度程序头文件，定义了任务结构task_struct、任务0数据等。
#include <linux/tty.h>		// tty头文件，定义了有关tty_io，串行通信方面的参数、常数。
#include <asm/segment.h>	// 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。
#include <asm/system.h>		// 系统头文件。定义设置或修改描述符/中断门等嵌入式汇编宏。

// 杀死进程组函数（向进程组发送信号）。参数pgrp指定进程组号；sig指定信号；pri权限。
// 即向指定进程组pgrp中的每个进程发送指定信号sig。只要向一个进程发送成功最后就会
// 返回0，否则如果没有找到指定进程组号pgrp的任何一个进程，则返回出错号-ESRCH。若
// 找到进程组号是pgrp的进程，但是发送信号失败，则返回发送失败的错误码。
int kill_pg(int pgrp, int sig, int priv); 		// kernel/exit.c，171行。 
// 判断一个进程组是否是孤儿进程。如果不是则返回0；如果是则返回1。
int is_orphaned_pgrp(int pgrp);				// kernel/exit.c，232行。

// 获取termios结构中三个模式标志集之一，或者用于判断一个标志集中是否有置位的标志。
#define _L_FLAG(tty, f)	((tty)->termios.c_lflag & f) /* 本地模式标志 */
#define _I_FLAG(tty, f)	((tty)->termios.c_iflag & f) /* 输入模式标志 */
#define _O_FLAG(tty, f)	((tty)->termios.c_oflag & f) /* 输出模式标志 */

// 取termios结构终端特殊（本地）模式标志集中的一个标志。
#define L_CANON(tty)	_L_FLAG((tty), ICANON) 		/* 取规范模式标志 */
#define L_ISIG(tty)	_L_FLAG((tty), ISIG)		/* 取信号标志 */
#define L_ECHO(tty)	_L_FLAG((tty), ECHO)		/* 取回显标志 */
#define L_ECHOE(tty)	_L_FLAG((tty), ECHOE)		/* 规范模式时取回显擦除标志 */
#define L_ECHOK(tty)	_L_FLAG((tty), ECHOK)		/* 规范模式时取KILL擦除当前行标志 */
#define L_ECHOCTL(tty)	_L_FLAG((tty), ECHOCTL)		/* 取回显控制字符标志 */
#define L_ECHOKE(tty)	_L_FLAG((tty), ECHOKE)		/* 规范模式时取KILL擦除行并回显标志 */
#define L_TOSTOP(tty)	_L_FLAG((tty), TOSTOP)		/* 对于后台输出发送SIGTTOU信号 */

// 取termios结构输入模式标志集中的一个标志。
#define I_UCLC(tty)	_I_FLAG((tty), IUCLC) 		/* 取大写到小写转换标志 */
#define I_NLCR(tty)	_I_FLAG((tty), INLCR)		/* 取换行符NL转回车符CR标志 */
#define I_CRNL(tty)	_I_FLAG((tty), ICRNL)		/* 取回车符CR转换行符NL标志 */ 
#define I_NOCR(tty)	_I_FLAG((tty), IGNCR)		/* 取忽略回车符CR标志 */ 
#define I_IXON(tty)	_I_FLAG((tty), IXON)		/* 取输入控制流标志XON */ 

// 取termios结构输出模式标志集中的一个标志。
#define O_POST(tty)	_O_FLAG((tty), OPOST) 		/* 取执行输出处理标志 */
#define O_NLCR(tty)	_O_FLAG((tty), ONLCR) 		/* 取换行符NL转回车换行符CR-NL标志 */
#define O_CRNL(tty)	_O_FLAG((tty), OCRNL) 		/* 取回车符CR转换行符NL标志 */
#define O_NLRET(tty)	_O_FLAG((tty), ONLRET) 		/* 取换行符NL执行回车功能的标志 */
#define O_LCUC(tty)	_O_FLAG((tty), OLCUC) 		/* 取小写转大写字符标志 */

// 取termios结构控制标志集中波特率。CBAUD是波特率屏蔽码（0000017）。
#define C_SPEED(tty)	((tty)->termios.c_cflag & CBAUD)
// 判断tty终端是否已挂线（hang up），即其传输波特率是否为B0（0）。
#define C_HUP(tty)	(C_SPEED((tty)) == B0)

// 取最小值宏。
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// 下面定义tty终端使用的缓冲队列结构数组tty_queues和tty终端表结构数组tty_table。
// QUEUES是tty终端使用的缓冲队列最大数量。伪终端分主从两种（master和slave）。每个
// tty终端使用3个tty缓冲队列，它们分别是用于缓冲键盘和串行输入的读队列read_queue、
// 用于缓冲屏幕或串行输出的写队列write_queue，以及用于保存规范模式字符的辅助缓冲队列
// secondary。
#define QUEUES (3*(MAX_CONSOLES+NR_SERIALS+2*NR_PTYS)) /* 共54项 */
static struct tty_queue tty_queues[QUEUES];	       // tty缓冲队列数组。
struct tty_struct tty_table[256];		       // tty表结构数组。

// 下面设定各种类型的tty终端所使用缓冲队列结构在tty_queues[]数组中的起始项位置。
// 8个虚拟控制台终端占用tty_queues[]数组开关24项（3 * MAX_CONSOLES）（0 -- 23）；
// 两个串行终端占用随后的6项（3 * NR_SERIALS）（24 -- 29）。
// 4个主伪终端占用随后的12项（3 * NR_PTYS）（30 -- 41）。
// 4个从伪终端占用随后的12项（3 * NR_PTYS）（42 -- 53）。
#define con_queues tty_queues
#define rs_queues ((3*MAX_CONSOLES) + tty_queues)
#define mpty_queues ((3*(MAX_CONSOLES+NR_SERIALS)) + tty_queues)
#define spty_queues ((3*(MAX_CONSOLES+NR_SERIALS+NR_PTYS)) + tty_queues)

// 下面设定各种类型tty终端所使用的tty结构在tty_table[]数组中的起始项位置。
// 8个虚拟控制台终端可用tty_table数组开关64项（0 -- 63）；
// 两个串行终端使用随后的2项（64 -- 65）。
// 4个主伪终端使用从128开始的项，最多64项（128 -- 191）。
// 4个从伪终端使用从192开始的项，最多64项（192 -- 255）。
#define con_table tty_table		/* 定义控制台终端tty表符号常数 */
#define rs_table (64+tty_table)		/* 串行终端tty表 */
#define mpty_table (128+tty_table)	/* 主伪终端tty表 */
#define spty_table (192+tty_table)	/* 从伪终端tty表 */

int fg_console = 0;			// 当前前台控制台号（范围 0--7）。

/*
 * these are the tables used by the machine code handlers.
 * you can implement virtual consoles.
 *
 * 下面是汇编程序中使用的缓冲队列结构地址表。通过修改这个表，
 * 你可以实现虚拟控制台。
 */
// tty读写缓冲队列结构地址表。供rs_io.s程序使用，用于取得读写缓冲队列结构的地址。
struct tty_queue * table_list[] = {
	con_queues + 0, con_queues + 1,	// 前台控制台读、写队列结构地址。
	rs_queues + 0, rs_queues + 1,	// 串行终端1读、写队列结构地址。
	rs_queues + 3, rs_queues + 4	// 串行终端2读、写队列结构地址。
};

//// 修改前台控制台函数。
// 将前台控制台设定为指定的虚拟控制台。
// 参数：new_console - 指定的新控制台号。
void change_console(unsigned int new_console)
{
	// 如果参数指定的控制台已经在前台或者参数无效，则退出。否则设置当前前台控制台号，同
	// 时更新table_list[]中的前台控制台读、写队列结构地址。最后更新当前前台控制台屏幕。
	if (new_console == fg_console || new_console >= NR_CONSOLES)
		return;
	fg_console = new_console;
	table_list[0] = con_queues + 0 + fg_console*3;
	table_list[1] = con_queues + 1 + fg_console*3;
	update_screen();		// kernel/chr_drv/console.c，936行。
}

//// 如果队列缓冲区空则让进程进入可中断睡眠状态。
// 参数：queue - 指定队列的指针。
// 进程在取队列缓冲区中字符之前需要调用此函数加以验证。如果当前进程没有信号要处理，
// 并且指定的队列缓冲区空，则让进程进入可中断睡眠状态，并让队列的进程等待指针指向
// 该进程。
static void sleep_if_empty(struct tty_queue * queue)
{
	cli();
	while (!(current->signal & ~current->blocked) && EMPTY(queue))
		interruptible_sleep_on(&queue->proc_list);
	sti();
}

/// 若队列缓冲区满则让进程进入可中断的睡眠状态。
// 参数：queue - 指定队列的指针。
// 进程在往队列缓冲区中写入字符之前需要调用此函数判断队列情况。如果队列缓冲区不满则
// 返回退出。否则若进程没有信号需要处理，并且队列缓冲区中空闲剩余区长度 < 128，则让
// 进程进入可中断睡眠状态，并让该队列的进程等待指针指向该进程。
static void sleep_if_full(struct tty_queue * queue)
{
	if (!FULL(queue))
		return;
	cli();
	while (!(current->signal & ~current->blocked) && LEFT(queue)<128) 
		interruptible_sleep_on(&queue->proc_list);
	sti();
}

/// 等待按键。
// 如果前台控制台读队列缓冲区空，则让进程进入可中断睡眠状态。
void wait_for_keypress(void)
{
	sleep_if_empty(tty_table[fg_console].secondary);
}

/// 复制并转换成规范模式字符序列。
// 根据终端termios结构中设置的各种标志，将指定tty终端读队列缓冲区中的字符复制并转换
// 成规范模式（熟模式）字符并存放在辅助队列（规范模式队形）中。
// 参数：tty - 指定终端的tty结构指针。
void copy_to_cooked(struct tty_struct *tty)
{
	signed char c;

// 首先检查当前终端tty结构中各缓冲队列指针是否有效。如果三个队列指针都是NULL，则说明
// 内核tty初始化函数有问题。
	if (!(tty->read_q || tty->write_q || tty->secondary)) {
		printk("copy_to_cooked: missing queues\n\r");
		return;
	}
// 否则我们根据终端termios结构中的输入和本地标志，对从tty读队列缓冲区中取出的每个
// 字符进行适当的处理，然后放入辅助队列secondary中。在下面循环体中，如果此时读队列
// 缓冲区已经取空或者辅助队列缓冲区已经放满字符，就退出循环体。否则程序就从读队列缓
// 冲区尾指针处取一字符，并把尾指针前移一个字符位置。然后根据该字符代码值进行处理。
// 另外，如果定义了_POSIX_VDISABLE(\0)，那么在对字符处理过程中，若字符代码值等于
// _POSIX_VDISABLE的值时，表示禁止使用相应特殊控制字符的功能。
	while (1) {
		if (EMPTY(tty->read_q))
			break;
		if (FULL(tty->secondary))
			break;
		GETCH(tty->read_q, c);	// 取一字符到c，并前移尾指针。
// 如果该字符是回车CR（13），那么若回车转换行标志CRNL置位，则将字符转换为换行符
// NL（10）。否则如果忽略回车标志NOCR置位，则忽略该字符，继续处理其他字符。如果字
// 符是换行符NL（10），并且换行转回车标志NLCR置位，则将其转换为回车符CR（13）。
// 如果大写转小写输入标志UCLC置位，则将该字符转换为小写字符。
		if (c == 13) {
			if (I_CRNL(tty))
				c = 10;
			else if (I_NOCR(tty))
				continue;
		} else if (c == 10 && I_NLCR(tty))
			c = 13;
		if (I_UCLC(tty))
			c = tolower(c);
// 如果本地模式标志集中规范模式标志CANON已置位，则使用以下方式对读取的字符进行处理。
// 首先，如果该字符是键盘终止控制字符KILL（^U），则对已输入的当前字符行执行删除处理。
// 删除一行字符的过程是：如果tty辅助队列不空，并且辅助队列中取出的最后一个字符不是
// 换行符NL（10），并且该字符不是文件结束字符（^D）或该文件结束字符不等于
// _POSIX_VDISABLE，则循环地对该行上所有其它字符进行删除处理。删除过程中，若本地回显
// 标志ECHO是置位的，那么还需要往写队列加入擦除控制字符ERASE（^H）。若字符是控制字符
// （值 < 32），使用两字节表示（例如^V），则须多放入一个擦除字符ERASE。然后调用tty写
// 函数把写队列中的所有字符输出到终端设备。最后将tty辅助队列头指针后退1字节。
		if (L_CANON(tty)) {
			if ((KILL_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == KILL_CHAR(tty))) {
				/* deal with killing the input line */
				while (!(EMPTY(tty->secondary) ||
					 (c = LAST(tty->secondary)) == 10 ||
					 ((EOF_CHAR(tty) != _POSIX_VDISABLE) &&
					  (c == EOF_CHAR(tty))))) {
					if (L_ECHO(tty)) { // 若本地回显标志置位。
						if (c < 32) // 被控制字符要删2字节。
							PUTCH(127, tty->write_q);
						PUTCH(127, tty->write_q);
						tty->write(tty);
					}
					DEC(tty->secondary->head);
				}
				continue; // 继续读取读队列中字符进行处理。
			}
//
// 如果该字符是删除控制字符ERASE（^H），并且该删除字符不等于_POSIX_VDISABLE，那么：
// 如果tty的辅助队列为空，或者其最后一个字符是换行符NL（10），或者是文件结束符但不等于
// _POSIX_VDISABLE，则继续处理其他字符。若本地回显标志ECHO是置位的，那么还需往写队列
// 加入擦除控制字符ERASE（^H）。若字符是控制字符（值<32），使用两字节表示（例如^V），
// 则须多放入一个擦除字符ERASE，并调用该tty写函数把写队列中的所有字符输出到终端设备。
// 最后将tty辅助队列头指针后退1字节，继续处理其他字符。
			if ((ERASE_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == ERASE_CHAR(tty))) {
				if (EMPTY(tty->secondary) ||
				    (c = LAST(tty->secondary)) == 10 ||
				    ((EOF_CHAR(tty) != _POSIX_VDISABLE) &&
				     (c == EOF_CHAR(tty))))
					continue;
				if (L_ECHO(tty)) { // 若本地回显标志置位。
					if (c < 32)
						PUTCH(127, tty->write_q);
					PUTCH(127, tty->write_q);
					tty->write(tty);
				}
				DEC(tty->secondary->head);
				continue;
			}
		}
// 如果设置了IXON标志，则使终端停止/开始输出控制字符起作用。如果没有设置此标志，那
// 么停止和开始字符被作为一般字符供进程读取。在这段代码中，如果读取的字符是停止字
// 符STOP（^S），则置tty停止标志，让tty暂停输出，同时丢弃该特殊控制字符（不放入
// 辅助队列中），并继续处理其他字符。如果字符是开始字符START（^Q），则复位tty停止
// 标志，恢复tty输出，同时丢弃该控制字符，并继续处理其他字符。
// 对于控制台来说，这里的tty->write()是console.c中的con_write()函数。因此控制台将
// 由于发现stopped=1而会立刻暂停在屏幕上显示新字符（chr_drv/console.c，第586行）。
// 对于伪终端也是由于设置了终端stopped标志而会暂停写操作（chr_drv/pty.c，第24行）。
// 对于串行终端，也应该在发送终端过程中根据终端stopped标志暂停发送，但本版示实现。
		if (I_IXON(tty)) {
			if ((STOP_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == STOP_CHAR(tty))) {
				tty->stopped = 1;
				tty->write(tty);
				continue;
			}
			if ((START_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == START_CHAR(tty))) {
				tty->stopped = 0;
				tty->write(tty);
				continue;
			}
		}
// 若输入模式标志集中ISIG标志置位，表示终端键盘可以产生信号，则在收到控制字符INTR、
// QUIT、SUSP或DSUSP时，需要为进程产生相应的信号。如果该字符是键盘中断符（^C），
// 则向当前进程之进程组中所有进程发送键盘中断信号SIGINT，并继续处理下一字符。如果该
// 字符是退出符（^\），则向当前进程之进程组中所有进程发送键盘退出信号SIGQUIT，并继续
// 处理下一字符。如果字符是暂停符（^Z），则向当前进程发送暂停信号SIGTSTP。同样，若定
// 义了_POSIX_VDISABLE（\0），那么在对字符处理过程中，若字符代码值等于_POSIX_VDISABLE
// 的值时，表示禁止使用相应特殊被控制字符的功能。以下不再啰嗦了 :-) 
		if (L_ISIG(tty)) {
			if ((INTR_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == INTR_CHAR(tty))) {
				kill_pg(tty->pgrp, SIGINT, 1);
				continue;
			}
			if ((INTR_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == QUIT_CHAR(tty))) {
				kill_pg(tty->pgrp, SIGQUIT, 1);
				continue;
			}
			if ((INTR_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c == SUSPEND_CHAR(tty))) {
				if (!is_orphaned_pgrp(tty->pgrp))
					kill_pg(tty->pgrp, SIGTSTP, 1);
				continue;
			}
		}
// 如果该字符是换行符NL（10），或者是文件结束符EOF（4，^D），表示一行字符已处理完，
// 则把辅助缓冲队列中当前含有行数值secondary.data增1。如果在函数tty_read()中取走
// 一行字符，该值即会被减1，参见315行。
		if (c == 10 || (EOF_CHAR(tty) != _POSIX_VDISABLE &&
				c == EOF_CHAR(tty)))
			tty->secondary->data++;
// 如果本地模式标志集中回显标志ECHO在置位状态，那么，如果字符是换行符NL（10），则还需
// 将换行符NL（10）和回车符CR（13）放入tty写队列缓冲区中；如果字符是控制字符（值<32）
// 并且回显控制字符标志ECHOCTL置位，则将字符‘^’和字符c+64放入tty写队列中（也即会
// 显示^C、^H等）；否则将该字符直接放入tty写缓冲队列中。最后调用该tty写操作函数。
		if (L_ECHO(tty)) {
			if (c == 10) {
				PUTCH(10, tty->write_q);
				PUTCH(13, tty->write_q);
			} else if (c < 32) {
				if (L_ECHOCTL(tty)) {
					PUTCH('^', tty->write_q);
					PUTCH(c+64, tty->write_q);
				}
			} else
				PUTCH(c, tty->write_q);
			tty->write(tty);
		}
// 每一次循环末将处理过的字符放入辅助队列中。
// 最后矸退出循环体后唤醒等待该辅助缓冲队列的进程（如果有的话）。
		PUTCH(c, tty->secondary);
	}
	wake_up(&tty->secondary->proc_list);
}

/*
 * Called when we need to send a SIGTTIN or SIGTTOU to our process
 * group
 *
 * We only request that a system call be restarted if there was if the
 * default signal handler is being used.  The reason for this is that if
 * a job is catching SIGTTIN or SIGTTOU, the signal handler may not want
 * the system call to be restarted blindly. If there is no way to reset the
 * terminal pgrp back to the current pgrp (perhaps because the controlling
 * tty has been released on logout), we don't want to be in an infinite loop
 * while restarting the system cll, and have it always generate a SIGTTIN
 * or SIGTTOU.  The default signal handler will cause the process to stop
 * thus avoiding the infinite loop problem.  Presumably the job-control
 * cognizant parent will fix things up before continuging its child process.
 *
 * 当需要发送信号SIGTTIN或SIGTTOU到我们进程组中所有进程时就会调用该函数。
 *
 * 在进程使用默认信号处理句柄情况下，我们仅要求一个系统调用被重新启动，如果
 * 有系统调用因本信号而被中断。这样做的原因是，如果一个作业正在捕获SIGTTIN
 * 或SIGTTOU信号，那么相应信号句柄并不会希望系统调用被盲目重新启动。如果
 * 没有其他方法把终端的pgrp复位到当前pgrp（例如可能由于在logout时控制终端
 * 已被释放），那么我们并不希望在重新启动系统调用时掉入一个无限循环中，并且
 * 总是产生SIGTTIN或SIGTTOU信号。默认的信号句柄会使得进程停止，因而可以
 * 避免无限循环问题。这里假设可识别作业控制的父进程会在继续执行其子进程之前
 * 把问题搞定。
 */
/// 向使用终端的进程组中所有进程发送信号。
// 在后台进程组中的一个进程访问控制终端时，该函数用于向后台进程组中的所有进程发送
// SIGTTIN或SIGTTOU信号。无论后台进程组中的进程是否已经阻塞或忽略掉了这两个信号，
// 当前进程都将立刻退出读写操作而返回。
int tty_signal(int sig, struct tty_struct * tty)
{
// 我们并不希望停止一个孤儿进程组中的进程（参见文件kernel/exit.c中第232行上的说明）。
// 因此若当前进程组是孤儿进程组，就出错返回。否则就向当前进程组中所有进程发送指定信号sig。
	if (is_orphaned_pgrp(current->pgrp))
		return -EIO;	/* don't stop an orphaned pgrp */
	(void) kill_pg(current->pgrp, sig, 1); // 发送信号sig。
// 如果这个信号被当前进程阻塞（屏蔽），或者被当前进程忽略掉，则出错返回。否则，如果
// 当前进程对信号sig设置了新的处理句柄，那么就返回我们可被中断的信息。否则就返回在
// 系统调用重新启动后可以继续执行的信息。
	if ((current->blocked & (1<<(sig-1))) ||
	    ((int) current->sigaction[sig-1].sa_handler == 1))
		return -EIO;	/* Our signal will be ignored */
	else if (current->sigaction[sig-1].sa_handler)
		return -EINTR;	/* We _will_ be interrupted :-) */
	else
		return -ERESTARTSYS;	/* We _will_ be interrupted :-) */
					/* (but restat after we continue) */
}

/// tty读函数。
// 从终端辅助缓冲队列中读取指定数量的字符，放到用户指定的缓冲区中。
// 参数：channel - 子设备号；buf - 用户缓冲区指针； nr - 欲读字节数。
// 返回已读字节数
int tty_read(unsigned channel, char * buf, int nr)
{
	struct tty_struct * tty;
	struct tty_struct * other_tty = NULL;
	char c, * b = buf;
	int minimum, time;

// 首先判断参数有效性并取终端的tty结构指针。如果tty终端的三个缓冲队列指针都是NULL，
// 则返回EIO出错信息。如果tty终端是一个伪终端，则科学家需要取得另一个对应伪终端的tty结
// 构other_tty。
	if (channel > 255)
		return -EIO;
	tty = TTY_TABLE(channel);
	if (!(tty->write_q || tty->read_q || tty->secondary))
		return -EIO;
// 如果当前进程使用的是这里正在处理的tty终端，但该终端的进程组号却与当前进程组号不
// 同，表示当前进程是后台进程组中的一个进程，即进程不在前台。于是我们要停止当前进程
// 组的所有进程。因此这里就需要向当前进程组发送SIGTTIN信号，并返回，等待成为前台进程
// 组后再执行读操作。另外，如果当前终端是伪终端，那么对应的另一个伪终端就是other_tty。
// 若这里tty是主伪终端，那么other_tty已经改好对应的从伪终端，反之亦然。
	if ((current->tty == channel) && (tty->pgrp != current->pgrp))
		return (tty_signal(SIGTTIN, tty));
	if (channel & 0x80)
		other_tty = tty_table + (channel ^ 0x40);
	
// 然后根据VTIME和VMIN对应的控制字符数组值设置读字符操作超时定时值time和最少需
// 读取的字符个数minimum。在非规范模式下，这两个是超时定时值。VMIN表示为了满足读
// 操作而需要读取的最少字符个数。VTIME是一个1/10秒计数计时值。
	time = 10L*tty->termios.c_cc[VTIME]; // 设置读操作超时定时值。
	minimum = tty->termios.c_cc[VMIN];   // 最少需要读取的字符个数。

// 如果tty终端处于规范模式，则设置最小要读取字符数minimum等于进程欲读字符数nr。同
// 时把进程读取nr字符的超时时间值设置为极大值（不会超时）。否则说明终端处于非规范模
// 式下，若此时设置了最少读取字符数minimum，则行临时设置进程读超时值为无限大，以
// 让进程先读取辅助队列中已有字符。如果读到的字符数不足minimum的话，后面代码会根据
// 指定的超时值time来设置进程的读超时值timeout，并会等待读取其余字符。参见328行。
// 若此时没有设置最少读取字符数minimum（为0），则将其设置为进程欲读字符数nr，并且如
// 果设置了超时定时值time的话，就把进程读字符超时定时值timeout设置为系统当前时间值
// + 指定的超时值time，同时复位time。另外，如果以上设置的最少读取字符数minimum大
// 于进程欲读取的字符数nr，则让minimum=nr。即对于规范模式下的读取操作，它不爱VTIME
// 和VMIN对应控制字符值的约束和控制，它们仅有非规范模式（生模式）操作中起作用。
	if (L_CANON(tty)) {
		minimum = nr;
		current->timeout = 0xffffffff;
		time = 0;
	} else if (minimum)
		current->timeout = 0xffffffff;
	else {
		minimum = nr;
		if (time)
			current->timeout = time + jiffies;
		time = 0;
	}
	if (minimum > nr)
		minimum = nr;	// 最多读取要求的字符数。

// 现在我们开始从辅助队列中循环取出字符并放到用户缓冲区buf中。当欲读的字节数大于0，
// 则执行以下循环操作。在循环过程中，如果当前终端是伪终端，那么我们就执行其对应的另
// 一个伪终端的写操作操作函数，让另一个伪终端把字符写入当前伪终端辅助队列缓冲区中。即让
// 另一终端把写队列缓冲区中字符复制到当前伪终端读队列缓冲区中，并经行规程函数转换后
// 放入当前伪终端辅助队列中。
	while (nr > 0) {
		if (other_tty)
			other_tty->write(other_tty);
// 如果tty辅助缓冲队列为空，或者设置了规范模式标志并且tty读队列缓冲区未满，并且辅
// 助队列中字符行数为0，那么，如果没有设置过进程读字符超时值（为0），或者当前进程
// 目前收到信号，就先退出循环体。否则如果本终端是一个从伪终端，并且其对应的主伪终端
// 已经挂断，那么我们也退出循环体。如果不是以上这两种情况，我们就让当前进程进入可中
// 断睡眠状态，返回后继续处理。由于规范模式时内核以行为单位为用户提供数据，因此在该
// 模式下辅助队列中必须起码有一行字符可代也能用，即secondary.data起码是1行才行。
		cli();
		if (EMPTY(tty->secondary) ||
		    (L_CANON(tty) && !FULL(tty->read_q) &&
		     !tty->secondary->data)) {
			if (!current->timeout ||
			    (current->signal & ~current->blocked)) {
				sti();
				break;
			}
			if (IS_A_PTY_SLAVE(channel) && C_HUP(other_tty))
				break;
			interruptible_sleep_on(&tty->secondary->proc_list);
			sti();
			continue;
		}
		sti();
// 下面开始正式执行取字符操作。需读字符数nr依次递减，直到nr=0或者辅助缓冲队列为空。
// 在这个循环过程中，首先取辅助缓冲队列字符c，并且把缓冲队列尾指针tail向右移动一个
// 字符位置。如果所取字符是文件结束字符（^D）或者是换行符NL（10），则把辅助缓冲队列中
// 含有字符行数值减1。 如果该字符是文件结束符（^D）并且规范模式标志成置位状态，则中
// 断本循环，否则说明现在还没有遇到文件结束符或者正处于原始（非规范）模式。在这种模
// 式中用户以字符流作为读取对象，也不识别其中的控制字符（如文件结束符）。于是将字符
// 直接放入用户数据缓冲区buf中，并把欲读字符数减1。此时如果欲读字符数已为0则中断
// 循环。另外，如果终端处于规范模式并且读取的字符是换行NL（10），则也退出循环。
// 除此之外，只要还没有取完欲读字符数nr并且辅助队列不空，就继续取队列中的字符。
		do {
			GETCH(tty->secondary, c);
			if ((EOF_CHAR(tty) != _POSIX_VDISABLE &&
			     c == EOF_CHAR(tty)) || c == 10)
				tty->secondary->data--;
			if ((EOF_CHAR(tty) != _POSIX_VDISABLE &&
			     c == EOF_CHAR(tty)) && L_CANON(tty))
				break;
			else {
				put_fs_byte(c, b++);
				if (!--nr)
					break;
			}
			if (c == 10 && L_CANON(tty))
				break;
		} while (nr > 10 && !EMPTY(tty->secondary));
// 执行到此，那么如果tty终端处于规范模式下，说明我们可能读取了换行符或者遇到了文件
// 结束符。如果是处于非规范模式下，那么说明我们已经读取了nr个字符，或者辅助队列已经
// 被取空了。于是我们首先唤醒等待读队列的进程，然后看看是否设置过超时定时值time。如
// 果超时定时值time不为0，我们就要求等待一定的时间让其他进程可以把字符写入读队列中。
// 于是设置进程读超时值为系统当前时间jiffies + 读超时值time。当然，如果终端处于
// 规范模式，或者已经读取了nr个字符，我们就可以直接退出这个大循环了。
		wake_up(&tty->read_q->proc_list);
		if (time)
			current->timeout = time + jiffies;
		if (L_CANON(tty) || b-buf >= minimum)
			break;
	}
// 此时读取tty字符循环操作结束，因此复位进程的读取超时定时值timeout。如果此时当前进
// 程已收到信号并且还没有读取到任何字符，则以重新凉快去系统调用号返回，否则就返回已读取
// 的字符数（b-buf）。
	current->timeout = 0;
	if ((current->signal & ~current->blocked) && !(b-buf))
		return -ERESTARTSYS;
	return (b-buf);
}

/// tty写函数。
// 把用户缓冲区中的字符放入tty写队列缓冲区中。
// 参数：channel - 子设备号；buf - 缓冲区指针；nr - 写字节数。
// 返回已写字节数。
int tty_write(unsigned channel, char * buf, int nr)
{
	static int cr_flag = 0;
	struct tty_struct * tty;
	char c, *b = buf;

// 首先判断参数有效性并取终端的tty结构指针。如果tty终端的三个缓冲队列指针都是NULL，
// 则返回EIO出错信息。
	//
	if (channel > 255)
		return -EIO;
	tty = TTY_TABLE(channel);
	if (!(tty->write_q || tty->read_q || tty->secondary))
		return -EIO;
// 如果终端本地模式标志集中设置了TOSTOP，表示后台进程输出时需要发送信号SIGTTOU。
// 如果当前进程使用的是这里正在处理的tty终端，但该终端的进程组号却与当前进程组号不
// 同，即表示当前进程是后台进程组中的一个进程，即进程不在前台。于是我们需要停止当前
// 进程组的所有进程。因此这里就需要向当前进程组发送SIGTTOU信号，并返回，等待成为前
// 台进程组后再执行写操作。
	if (L_TOSTOP(tty) &&
	    (current->tty == channel) && (tty->pgrp != current->pgrp))
		return (tty_signal(SIGTTOU, tty));
// 现在我们开始从用户缓冲区buf中循环取出字符并放到写队列缓冲区中。当欲写字节数大于0，
// 则执行以下循环操作。在循环过程中，如果此时tty写队列已满，则当前进程进入可中断的睡
// 眠状态。如果当前进程有信号要处理，则退出循环体。
	while (nr > 0) {
		sleep_if_full(tty->write_q);
		if (current->signal & ~current->blocked)
			break;
// 当要写的字符数nr还大于0并且tty写队列缓冲区不满，则循环执行以下操作。首先从用户
// 缓冲区中取1字节。如果终端输出模式标志集中的执行输出处理标志OPOST置位，则执行对
// 字符的后处理操作。
		while (nr > 0 && !FULL(tty->write_q)) {
			c = get_fs_byte(b);
			if (O_POST(tty)) {
// 如果该字符是回车符‘\r’（CR，13）并且回车转换行符标志OCRNL置位，则将该字符换成
// 换行符‘\n’（NL，10）；否则如果该字符是换行符‘\n’（NL，10）并且换行转回车功能标志
// ONLRET置位的话，则将该字符换成回车符‘\r’（CR，13）。
				if (c == '\r' && O_CRNL(tty))
					c = '\n';
				else if (c == '\n' && O_NLCR(tty))
					c = '\r';
// 如果该字符是换行符‘\n’并且回车标志cr_flag没有置位，但换行转回车-换行标志ONLCR
// 置位的话，则将cr_flag标志置位，并将一回车符放入写队列中。然后继续处理下一个字符。
// 如果小写转大写标志OLCUC置位的话，就将该字符转成大写字符。
				//
				if (c == '\n' && !cr_flag && O_NLCR(tty)) {
					cr_flag = 1;
					PUTCH(13, tty->write_q);
					continue;
				}
				if (O_LCUC(tty)) // 小写转成大写字符。
					c = toupper(c);
			}
// 接着把用户数据缓冲指针b前移1字节；欲写字节数减1字节；复位cr_flag标志，并将该
// 字节放入tty写队列中。
			b++; nr--;
			cr_flag = 0;
			PUTCH(c, tty->write_q);
		}
// 若要求的字符全部写完，或者写队列已满，则程序退出循环。此时会调用对应tty写函数，
// 把写队列缓冲区中的字符显示在控制台屏幕上，或者通过串行端口发送出去。如果当前处
// 理的tty是控制台终端，那么tty->write()调用的是con_write()；如果tty是串行终端，
// 则tty->write()调用的是rs_write()函数。若还有字节要写，则我们需要等待写队列中字符
// 被取走。所以这里调用调度程序，先去执行其他任务。
		tty->write(tty);
		if (nr > 0)
			schedule();
	}
	return (b-buf);		// 最后返回写入的字节数。
}

/*
 * Jeh, sometimes I really like the 386.
 * This routine is called from an interrupt,
 * and there should be absolutely no problem
 * with sleeping even in an interrupt (I hope).
 * Of course, if somebody proves me wrong, I'll
 * hate intel for all time :-). We'll have to
 * be careful and see to reinstating the interrupt
 * chips before calling this, though.
 *
 * I don't think we sleep here under normal circumstances
 * anyway, which is good, as the task sleeping might be
 * totally innocent.
 *
 * 呵，有时我真的很喜欢386。该子程序被从一个中断处理程序中
 * 调用，并且即使在中断处理程序中睡眠也应该绝对没有问题（我
 * 希望如此）。当然，如果有人证明我是错的，那么我将憎恨intel
 * 一辈子:-)。但是我们必须小心，在调用该子程序之前需要恢复中断。
 *
 * 我不认为在通常环境下会处在这里睡眠，这样很好，因为任务睡眠
 * 是完全任意的。
 */
/// tty中断处理调用函数 - 字符规范模式处理。
// 参数：tty - 指定的tty终端号。
// 将指定tty终端队列缓冲区中的字符复制或转换成规范（熟）模式字符并存放在辅助队列中。
// 该函数会在串口读字符中断（rs_io.s，110）和键盘中断（keyboard.S，76）中被调用。
void do_tty_interrupt(int tty)
{
	copy_to_cooked(TTY_TABLE(tty));
}

/// 字符设备初始化函数。空，为以后扩展做准备。
void chr_dev_init(void)
{
}

/// tty终端初始化函数。
// 初始化所有终端缓冲队列，初始化串口终端和控制台终端。
void tty_init(void)
{
	int i;

// 首先初始化所有终端的缓冲队列结构，设置初值。对于串行终端的读/写缓冲队列，将它们的
// data字段设置为串行端口基地址值。串口1是0x3f8，串口2是0x2f8。然后先初步设置所有
// 终端的tty结构。其中特殊字符数组c_cc[]设置的初值定义在include/linux/tty.h文件中。
	for (i = 0; i < QUEUES; i++)
		tty_queues[i] = (struct tty_queue) {0, 0, 0, 0, ""};
	rs_queues[0] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
	rs_queues[1] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
	rs_queues[3] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
	rs_queues[4] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
	for (i = 0; i < 256; i++) {
		tty_table[i] = (struct tty_struct) {
			{0, 0, 0, 0, 0, INIT_C_CC},
			0, 0, 0, NULL, NULL, NULL, NULL
		};
	}
// 接着初始化控制台终端（console.c，834行）。把con_init()放在这里，是因为我们需要根
// 据显示卡类型和显示内存容量来确定系统中虚拟控制台的数量NR_CONSOLES。该值被用于随后
// 的控制台tty结构初始化循环中。对于控制台的tty结构，425--430行是tty结构中包含的
// termios结构字段。其中输入模式标志集被初始化为ICRNL标志；输出模式标志被初始化为含
// 有后处理标志OPOST和把NL转换成CRNL的标志ONLCR；本地模式标志集被初始化含有IXON、
// ICANON、ECHO、ECOCTL和ECHOKE标志；控制字符数组c_cc[]被设置含有初始值INIT_C_CC。
// 435行上初始化控制台终端tty结构中的读缓冲、写缓冲和辅助缓冲队列结构，它们分别指向
// tty缓冲队列结构数组tty_table[]中的相应结构项。参见61--73行上的相关说明。
	con_init();
	for (i = 0; i < NR_CONSOLES; i++) {
		con_table[i] = (struct tty_struct) {
			{ICRNL,		/* change incoming CR to NL */
			OPOST|ONLCR,	/* change outgoing NL to CRNL */
			0,		// 控制模式标志集。
			IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE, // 本地标志集。
			0,		/* console termio */ 	// 线路规程，0 -- TTY。
			INIT_C_CC},				// 控制字符数组c_cc[]。
			0,		/* initial pgrp */	// 所属初始进程组pgrp。
			0,		/* initial session */	// 初始会话组session。
			0,		/* initial stopped */	// 初始停止标志stopped。
			con_write,				// 控制台写函数。
			con_queues+0+i*3,con_queues+1+i*3,con_queues+2+i*3
		};
	}
// 然后初始化串行终端的tty结构各字段。450行初始化串行终端tty结构中的读/写和辅助缓
// 冲队列结构，它们分别指向tty缓冲队列结构数组tty_table[]中的相应结构项。参见61--
// 73行上的相关说明。
	for (i = 0; i < NR_SERIALS; i++) {
		rs_table[i] = (struct tty_struct) {
			{0,	/* no translation */	// 输入模式标志集。0，无须转换。
			0,	/* no translation */	// 输出模式标志集。0，无须转换。
			B2400 | CS8,		// 控制模式标志集。2400bps，8位数据位。
			0,			// 本地模式标志集。
			0,			// 线路规程，0 -- TTY。
			INIT_C_CC},		// 控制字符数组。
			0,			// 所属初始进程组。
			0,			// 初始会话组。
			0,			// 初始停止标志。
			rs_write,		// 串口终端写函数。
			rs_queues+0+i*3,rs_queues+1+i*3,rs_queues+2+i*3 // 三个队列。
		};
	}
// 然后再初始化伪终端使用的tty结构。伪终端是配对使用的，即一个主（master）伪终端配
// 有一个从（slave）伪终端。因此对它们都要进行初始化设置。在下面循环中，我们首先初始化
// 每个主伪终端的tty结构，然后再初始化其对应的从伪终端的tty结构。
	//
	for (i = 0; i < NR_PTYS; i++) {
		mpty_table[i] = (struct tty_struct) {
			{0,	/* no translation */	// 输入模式标志集。0，无须转换。
			0,	/* no translation */	// 输出模式标志集。0，无须转换。
			B9600 | CS8,		// 控制模式标志集。9600bps，8位数据位。
			0,			// 本地模式标志集。
			0,			// 线路规程，0 -- TTY。
			INIT_C_CC},		// 控制字符数组。
			0,			// 所属初始进程组。
			0,			// 所属初始会话组。
			0,			// 初始停止标志。
			mpty_write,		// 主伪终端写函数。
			mpty_queues+0+i*3,mpty_queues+1+i*3,mpty_queues+2+i*3 
		};
		spty_table[i] = (struct tty_struct) {
			{0,	/* no translation */	// 输入模式标志集。0，无须转换。
			0,	/* no translation */	// 输出模式标志集。0，无须转换。
			B9600 | CS8,		// 控制模式标志集。9600bps，8位数据位。
			IXON | ISIG | ICANON,	// 本地模式标志集。
			0,			// 线路规程，0 -- TTY。
			INIT_C_CC},		// 控制字符数组。
			0,			// 所属初始进程组。
			0,			// 所属初始会话组。
			0,			// 初始停止标志。
			spty_write,		// 串口终端写函数。
			spty_queues+0+i*3,spty_queues+1+i*3,spty_queues+2+i*3 
		};
	}
// 最后初始化串行中断处理程序和串行接口1和2（serial.c，37行），并显示系统含有的虚拟
// 控制台数NR_CONSOLES和伪终端数NR_PTYS。
	rs_init();
	printk("%d virtual consoles\n\r", NR_CONSOLES);
	printk("%d pty's\n\r", NR_PTYS);
}
