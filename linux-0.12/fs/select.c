/*
 * This file contains the procedures for the handling of select
 *
 * Created for linux based loosely upon Mathius Lattner's minix
 * patches by Peter MacDonald. Heavily edited by Linus.
 */
/*
 * 本文件包含有处理select()系统调用的过程。
 *
 * 这是Peter MacDonald基于Mathius Lattner提供给MINIX系统的补丁
 * 程序修改而成。
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>

#include <asm/segment.h>
#include <asm/system.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <const.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

/*
 * Ok, Peter made a complicated, but straightforward multiple_wait() function.
 * I have rewritten this, taking some shortcuts: This code may not be easy to
 * follow, but it should be free of race-conditions, and it's practical. If you
 * understand what I'm doing here, then you understand how the linux sleep/wakeup
 * mechanism works.
 *
 * Two very simple procedures, add_wait() and free_wait() make all the work. We
 * have to have interrupts disabled throughout the select, but that's not really
 * such a loss: sleeping automatically free interrupts when we aren't in this
 * task.
 */
/*
 * OK，Peter编制了复杂担很直观的多个_wait()函数。我对这些函数进行了改写，以使之
 * 更简洁：这些代码可能不容易看懂，但是其中应该不会存在竞争条件问题，并且很实际。
 * 如果你能理解这里编制的代码，那么就说明你已经理解Linux中睡眠/唤醒的工作机制。
 *
 * 两个很简单的过程，add_wait()和free_wait()执行了主要操作。在整个select处理
 * 过程中我们不得不禁止中断。但是这样做并不会带来太多的损失；因为当我们不在执行
 * 本任务时睡眠状态会自动地释放中断（即其他任务会使用自己EFLAGS中的中断标志）。
 */
typedef struct {
	struct task_struct * old_task;
	struct task_struct ** wait_address;
} wait_entry;

typedef struct {
	int nr;
	wait_entry entry[NR_OPEN*3];		// 等待项数组。
} select_table;

/// 等待队列添加入等待表。
// 把未准备好描述符的等待队列指针加入等待表wait_table(应该是select_table?)中。参数*wait_address是与描述
// 符相关的等待队列头指针。例如tty读缓冲队列secondary的等待队列头指针量proc_list。
// 参数p是do_select()中定义的等待表结构指针。
static void add_wait(struct task_struct ** wait_address, select_table * p)
{
	int i;

// 首先检查描述符是否有对应的等待队列，若无则返回。然后在等待表中搜索参数指定的等待队列
// 指针是否已经在等待表中设置过，若设置过也立刻返回。这个检查主要是针对管道文件描述符。
// 例如若一个管道在等待可以进行读操作，那么其必定三中心立刻进行写操作。
	if (!wait_address)
		return;
	for (i = 0; i < p->nr; i++)
		if (p->entry[i].wait_address == wait_address)
			return;
// 然后我们把描述符对应的等待队列的头指针保存在等待表wait_table中，同时让等待表项的
// old_task字段指向等待队列头指针所指向的任务（若无则为NULL），再让等待队列头指针指
// 向当前任务。最后把等待表有效项计数值nr增1（其在第179行被初始化为0）。
	p->entry[p->nr].wait_address = wait_address;
	p->entry[p->nr].old_task = *wait_address;
	*wait_address = current;
	p->nr++;
}

/// 唤醒所有等待项上的进程，清空等待表。
// 参数是等待表结构指针。当前进程在do_select()函数中被唤醒时会调用本函数（第204、
// 207行），用于唤醒等待表中处于各个等待队列上的其他任务。它与kernel/sched.c中
// sleep_on()函数的后半部分代码几乎完全相同，可参考对sleep_on()函数的说明内容。
static void free_wait(select_table * p)
{
	int i;
	struct task_struct ** tpp;

// 如果等待表中各项（共nr个有效项）记录的等待队列头指针表明还有其他后来添加进的等待
// 任务（例如其他进程调用sleep_on()函数而睡眠在该等待队列上），则此时等待队列头指针
// 指向的不是当前进程，那么我们就需要先唤醒这些任务。操作方法是将等待队列头所指任务先
// 转为就绪状态（state = 0），并把自己设置为不可中断等待状态，即自己要等待这些后续进队
// 列的任务被唤醒而执行时来唤醒本任务。所以要重新执行调度程序。
	for (i = 0; i < p->nr; i++) {
		tpp = p->entry[i].wait_address;
		while (*tpp && *tpp != current) {
			(*tpp)->state = 0;
			current->state = TASK_UNINTERRUPTIBLE;
			schedule();
		}
// 执行到这里，说明等待表当前处理项中的等待队列头指针字段wait_address指向当前任务。
// 若它为空，则说明调度有问题，于是显示警告信息。然后我们让等待队列头指针指向在我们
// 前面进入队列的任务（第76行）。若此时该头指针确实指向一个任务而不是NULL，则说明
// 队列中还有任务（*tpp不为空），于是将该任务设置成就绪状态，唤醒之。最后把等待表的
// 有效表项计数段nr置零，清空该等待表。
		if (!*tpp)
			printk("free_wait: NULL");
		if ((*tpp = p->entry[i].old_task))
			(**tpp).state = 0;	// TASK_RUNNING
	}
	p->nr = 0;
}

/// 根据i节点取得tty结构。
// 根据文件i节点判断文件是否量字符终端设备文件。若是则返回其tty结构指针，否则返回NULL。
static struct tty_struct * get_tty(struct m_inode * inode)
{
	int major, minor;

// 如果不是字符设备文件则返回NULL。如果主设备号不是5（控制终端号）或4，则也返回NULL。
// 如果主设备号是5，那么进程的tty字段值即是其终端设备号，否则它等于字符设备号的次设备号。
// 如果终端设备号小于0，表示进程没有控制张总或没有使用终端，于是返回NULL。否则返回对应的
// tty结构指针。
	if (!S_ISCHR(inode->i_mode))
		return NULL;
	if ((major = MAJOR(inode->i_zone[0])) != 5 && major != 4)
		return NULL;
	if (major == 5)
		minor = current->tty;
	else
		minor = MINOR(inode->i_zone[0]);
	if (minor < 0)
		return NULL;
	return TTY_TABLE(minor);
}

/*
 * The check_XX functions check out a file. We know it's either
 * pipe, a character device or a fifo (fifo's not implemented)
 *
 * check_XX函数用于检查一个文件。我们知道该文件要么是管道文件、
 * 要么是字符设备文件，或者要么是一个FIFO（FIFO还未实现）。
 */
/// 检查读操作准备好。
// 检查读文件操作是否准备好，即终端读缓冲队列secondary是否有字符可读，或者管道文件是否
// 不空。参数wait是等待表指针；inode是文件i节点指针。若描述符可进行读操作则返回1，否
// 则返回0。
static int check_in(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

// 首先根据文件i节点调用get_tty()检测文件是否是一个tty终端（字符）设备文件。如果是则
// 检查该终端读缓冲队列secondary中是否有字符可供读取，若有则返回1。若此时secondary为
// 空则把当前任务添加到secondary的等待队列proc_list上并返回0。如果是管道文件，则判断
// 目前管道中是否有字符可读，若有则返回1，若没有（管道空）则把当前任务添加到管道i节点
// 的等待队列上并返回0。注意，PIPE_EMPTY()宏使用管道当前头指针位置来判断管道是否空。
// 管道i节点的i_zone[0]和i_zone[1]字段盆架存放着管道当前的头尾指针。
	if ((tty = get_tty(inode)))
		if (!EMPTY(tty->secondary))
			return 1;
		else
			add_wait(&tty->secondary->proc_list, wait);
	else if (inode->i_pipe)
		if (!PIPE_EMPTY(*inode))
			return 1;
		else
			add_wait(&inode->i_wait, wait);
	return 0;
}

/// 检查写操作准备好。
// 检查文件写操作是否准备好，即终端写缓冲队列write_q中是否还有空闲位置可写，或者此时管
// 道文件是否不满。参数wait是等待表指针；inode是文件i节点指针。若描述符可进行写操作则
// 返回1，否则返回0。
static int check_out(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

// 首先根据文件i节点调用get_tty()检测文件是否是一个tty终端（字符）设备文件。如果是则
// 检查该张总写缓冲队列write_q中是否有空间可写入，若有则返回1，若没有空间则把当前任
// 务添加到write_q的等待队列proc_list上并返回0。如果是管道文件则判断目前管道中是否有
// 空闲空间可写入字符，若有则返回1，若没有（管道满）则把当前任务添加到管道i节点的等待
// 队列上并返回0。
	if ((tty = get_tty(inode)))
		if (!FULL(tty->write_q))
			return 1;
		else
			add_wait(&tty->write_q->proc_list, wait);
	else if (inode->i_pipe)
		if (!PIPE_FULL(*inode))
			return 1;
		else
			add_wait(&inode->i_wait, wait);
	return 0;
}

// 检查文件是否处于异常状态。对于终端设备文件，目前内核总是返回0。对于管道文件，如果此
// 时两个管道描述符中有一个或都已被关闭，则返回1，否则就把当前任务添加到管道i节点的等
// 待队列上并返回0。参数wait是等待表指针；inode是文件i节点指针。若出现异常条件则返
// 加1，否则返回0。
static int check_ex(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

	if ((tty = get_tty(inode)))
		if (!FULL(tty->write_q))
			return 0;
		else
			return 0;
	else if (inode->i_pipe)
		if (inode->i_count < 2)
			return 1;
		else
			add_wait(&inode->i_wait, wait);
	return 0;
}

/// select()内部执行函数。
// do_select()是内核执行select()系统调用的实际处理函数。该函数首先检查描述符集中各个描
// 述符的有效性，然后分别调用函数check_XX()对每个描述符集的描述符进行检查，同时统计描述
// 符集中当前已经准备好的描述符个数。若有任何一个描述符已经准备好，本函数就会立刻返回，
// 否则进程就会进入睡眠状态，并在过了超时时间或者由于某个描述符所在等待队列上的进程被唤
// 醒而使本进程继续运行。
int do_select(fd_set in, fd_set out, fd_set ex,
	      fd_set *inp, fd_set *outp, fd_set *exp)
{
	int count;				// 已准备好的描述符个数计数值。
	select_table wait_table;		// 等待表结构。
	int i;
	fd_set mask;

// 首先把3个描述符集进行或操作，在mask中得到描述符集中有效描述符比特位屏蔽码。然后
// 循环判断当前进程各个描述符是否有效并且包含在描述符集内。在循环中，每判断完一个描述
// 符就会把mask右移1位，因此根据mask的最低有效比特位我们就可以判断相应描述符是否在
// 用户给定的描述符集中。有效的描述符应该是一个管道文件描述符，或者是一个字符设备文件
// 描述符或者是一个FIFO描述符，其余类型的都作为无效描述符而返回EBADF错误。
	mask = in | out | ex;
	for (i = 0; i < NR_OPEN; i++, mask >>= 1) {
		if (!(mask & 1))		// 若不在描述符集中则继续判断下一个。
			continue;
		if (!current->filp[i])		// 若该文件未打开，则返回描述符错。
			return -EBADF;
		if (!current->filp[i]->f_inode)	// 若文件i节点指针空，则返回错误号。
			return -EBADF;
		if (current->filp[i]->f_inode->i_pipe)
			continue;		// 若是管道文件描述符，则有效。
		if (S_ISCHR(current->filp[i]->f_inode->i_mode))
			continue;		// 字符设备文件有效。
		if (S_ISFIFO(current->filp[i]->f_inode->i_mode))
			continue;		// FIFO也有效。
		return -EBADF;			// 其余都作为无效描述符而返回。
	}
// 下面开始循环检查3个描述符集中的各个描述符是否准备好（可以操作）。此时mask用作当前
// 正在处理描述符的屏蔽码。循环中的3个函数check_in()、check_out()和check_ex()分别用
// 来判断描述符是否已经准备好读入、写出或出现异常。若一个描述符已经准备好，则在相关描述
// 符集中设置对应比特位，并且把已准备好描述符修灵计数值count增1。第183行for循环语句
// 中的mask += mask等效于mask<< 1。
repeat:
	wait_table.nr = 0;
	*inp = *outp = *exp = 0;
	count = 0;
	mask = 1;
	for (i = 0; i < NR_OPEN; i++, mask += mask) {
// 如果此时判断的描述符在读操作描述符集中，并且该描述符已经准备好可以进行读操作，则把
// 该描述符在找描述符集in中对应比特位置为1，同时把已准备好描述符个数计数值count增1。
		if (mask & in)
			if (check_in(&wait_table, current->filp[i]->f_inode)) {
				*inp |= mask;	// 描述符集中设置对应比特位。
				count++;	// 已准备好描述符个数计数。
			}
// 如果此时判断的描述符在写操作描述符集中，并且该描述符已经准备好可以进行写操作，则把
// 该描述符在描述符集out中对应比特位置为1，同时把已准备好描述符个数计数值count增1。
		if (mask & out)
			if (check_out(&wait_table, current->filp[i]->f_inode)) {
				*outp |= mask;
				count++;
			}
// 如果此时判断的描述符在异常描述符集中，并且该描述符已经有异常出现，则把该描述符在描
// 述符集ex中对应比特位置为1，同时把已准备好描述符个数计数值count增1。
		if (mask & ex)
			if (check_ex(&wait_table, current->filp[i]->f_inode)) {
				*exp |= mask;
				count++;
			}
	}
// 在对进程所有描述符判断处理过后，若没有发现有准备好的描述符（count==0），并且此时
// 进程没有收到任何非阻塞信号，并且此时有等待着的描述符或者等待时间还没有超时，那么我
// 们就把当前进程状态设置成可中断睡眠状态，然后执行调度函数执行其他任务。当内核又一
// 次调度执行本任务时就会调用free_wait()唤醒相关等待队列上本任务前后的任务，然后跳转到
// repeat标号处（178行）再次重新检测是否有我们关心的（描述符集中的）描述符已准备好。
	if (!(current->signal & ~current->blocked) &&
	    (wait_table.nr || current->timeout) &&!count) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		free_wait(&wait_table);		// 本任务被唤醒返回后从这里开始执行。
		goto repeat;
	}
// 如果此时count不等于0，或者接收到了信号，或者等待时间到并且没有需要等待的描述符，
// 那么我们就调用free_wait()唤醒等待队列上的任务，然后返回已准备好的描述符个数值。
	free_wait(&wait_table);
	return count;
}

/*
 * Note that we cannot return -ERESTARTSYS, as we change our input
 * parameters. Sad, but there you are. We could do some tweakin in
 * the library function ...
 */
/*
 * 注意我们不能返回-ERESTARTSYS，因为我们会在select运行过程中改变
 * 输入参数值（*timeout）。很不幸，但你也只能接受这个事实。不过我们
 * 可以在库函数中做些处理...
 */
/// select系统调用函数。
// 该函数中的代码主要负责进行select功能操作前后的参数复制和转换工作。select主要的工
// 作由do_select()函数来完成。sys_select()会首先根据参数传递来的缓冲区指针从用户数据空
// 间把select()函数调用的参数分解复制到内核空间，然后设置需要等待的超时时间值timeout，
// 接着调用do_select()执行select功能，返回后就把处理结果再复制回用户空间中。
// 参数buffer指向用户数据区中select()函数的第1个参数处。如果返回值小于0表示执行时出
// 现错误；如果返回值等于0，则表示在规定等待时间内没有描述符准备好操作；若返回值大于0，
// 则表示已准备好的描述符数量。
int sys_select(unsigned long * buffer)
{
/* Perform the select(nd, in, out, ex, tv) system call. */
/* 执行select(nd, in, out, ex, tv)系统调用 */
// 首先定义几个局部变量，用于把指针参数传递来的select()函数参数分解开来。
	int i;
	fd_set res_in, in = 0, *inp;		// 读操作描述符集。
	fd_set res_out, out = 0, *outp;		// 写操作描述符集。
	fd_set res_ex, ex = 0, *exp;		// 异常条件描述符集。
	fd_set mask;				// 处理的描述符数值范围（nd）屏蔽码。
	struct timeval *tvp;			// 等待时间结构指针。
	unsigned long timeout;

// 然后从用户数据区把参数盆架隔离和复制到局部指针变量中，并根据描述符集指针是否有效分别
// 取得3个描述符集in（读）、out（写）和ex（异常）。其中mask也是一个描述符集变量，
// 根据3个描述符集中最大描述符数值+1（即第1个参数nd的值），它被设置成用户程序关心的
// 所有描述符的屏蔽码。例如，若nd = 4，则mask = 0b00001111（共32比特）。
	mask = ~((~0) << get_fs_long(buffer++));
	inp = (fd_set *) get_fs_long(buffer++);
	outp = (fd_set *) get_fs_long(buffer++);
	exp = (fd_set *) get_fs_long(buffer++);
	tvp = (struct timeval *) get_fs_long(buffer);

	if (inp)				// 若指针有效，则取读操作描述符集。
		in = mask & get_fs_long(inp);
	if (outp)				// 若指针有效，则取写操作描述符集。
		out = mask & get_fs_long(outp);
	if (exp)				// 若指针有效，则取异常描述符集。
		ex = mask & get_fs_long(exp);
// 接下来我们尝试从时间结构中取出等待（睡眠）时间值timeout。首先把timeout初始化成最大
// （无限）值，然后从用户数据空间取得该时间结构中设置的时间值，经转换和加上系统当前滴答
// 值jiffies，最后得到需要等待的时间滴答数值timeout。物资用此值来设置当前进程应该等待
// 的延时。另外，第241行上tv_usec字段是微秒值，把它除以1000000后就得到对应秒数，再乘
// 以系统每秒滴答数HZ，即把tv_usec转换成滴答值。
	timeout = 0xffffffff;
	if (tvp) {
		timeout = get_fs_long((unsigned long *)&tvp->tv_usec) / (1000000/HZ);
		timeout += get_fs_long((unsigned long *)&tvp->tv_sec) * HZ;
		timeout += jiffies;
	}
	current->timeout = timeout;		// 设置当前进程应该延时的滴答值。
// select()函数的主要工作在do_select()中完成。在调用该函数之后的代码用于把处理结果复制
// 到用户数据区中，返回给用户。为了避免出现竞争条件，在调用do_select()前需要禁止中断，
// 并在该函数返回后再开启中断。
// 如果在do_select()返回之后进程的等待延时字段timeout还大于当前系统计时滴答值jiffies，
// 说明在超时之前已经有描述符准备好，于是这里我们先记下到超时还剩余的时间值，随后我们会
// 把这个值返回给用户。如果进程的等待延时字段timeout已经小于或等于当前系统jiffies，表
// 示do_select()可能是由于超时而返回，因此把剩余时间值设置为0。
	cli();					// 禁止响应中断。
	i = do_select(in, out, ex, &res_in, &res_out, &res_ex);
	if (current->timeout > jiffies)
		timeout = current->timeout - jiffies;
	else
		timeout = 0;
	sti();					// 开启中断响应。
// 接下来我们把进程的超时字段清零。如果do_select()返回的已准备好描述符个数小于0，表示
// 执行出错，于是返回这个错误号。然后我们把处理过的描述符集内容和延迟时间结构内容写回到
// 用户数据缓冲空间。在写时间结构内容时还需要先将滴答时间单位表示的剩余延迟时间转换成秒
// 和微秒值。
	current->timeout = 0;
	if (i < 0)
		return i;
	if (inp) {
		verify_area(inp, 4);
		put_fs_long(res_in, inp);	// 可读描述符集。
	}
	if (outp) {
		verify_area(outp, 4);
		put_fs_long(res_out, outp);	// 可写描述符集。
	}
	if (exp) {
		verify_area(exp, 4);
		put_fs_long(res_ex, exp);	// 出现异常条件描述符集。
	}
	if (tvp) {
		verify_area(tvp, sizeof(*tvp));
		put_fs_long(timeout/HZ, (unsigned long *) &tvp->tv_sec); // 秒。
		timeout %= HZ;
		timeout *= (1000000/HZ);
		put_fs_long(timeout, (unsigned long *) &tvp->tv_usec); // 微秒。
	}
// 如果此时并没有已准备好的描述符，并且收到了某个非阻塞信号，则返回被中断错误号。
// 否则返回已准备好的描述符个数值。
	if (!i && (current->signal & ~current->blocked))
		return -EINTR;
	return i;
}

