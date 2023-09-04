/* 
 *  linux/kernel/exit.c
 * 
 *  (C) 1991 Linus Torvalds
 */

#define DEBUG_PROC_TREE		// 定义符号“调试进程树”。

#include <errno.h>		// 错误号头文件。饮食系统中各种出错号。（Linus从minix中引进的）
#include <signal.h>		// 信号头文件。定义信号符号常量，信号结构以及信号操作函数原型。
#include <sys/wait.h>		// 等待调用头文件。定义系统调用wait()和waitpid()及相关常数符号。

#include <linux/sched.h>	// 调度程序头文件。定义了任务结构task_struct、任务0数据等。
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用函数的原形定义。
#include <linux/tty.h>		// tty头文件，定义了有关tty_io，串行通信方面的参数、常数。
#include <asm/segment.h>	// 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。

int sys_pause(void);		// 把进程置为睡眠状态，直到收到信号（kernel/sched.c，164行）。
int sys_close(int fd);		// 关闭指定文件的系统调用（fs/open.c，219行）。

//// 释放进程占用的任务槽及其任务数据结构占用的内存页面。
// 参数p是任务数据结构指针。该函数在后面的 sys_kill() 和 sys_waitpid() 函数中被调用。
// 扫描任务指针数组表 task[] 以寻找指定的任务。如果找到，则首先清空该任务槽，然后释放
// 该任务数据结构所占用的内存页面。最后执行调度函数并在返回时立即退出。如果在任务数组
// 表中没有找到指定任务对应的项，则内核panic。
void release(struct task_struct * p)
{
	int i;

// 如果给定的任务结构指针为NULL则退出。如果该指针指向当前进程则显示警告信息退出。
	if (!p)
		return;
	if (p == current) {
		printk("task releasing itself\n\r");
		return;
	}
// 扫描任务结构指针数组，寻找指定的任务p。如果找到，则置空任务指针数组中对应项，并且
// 更新任务结构之间的关联指针，释放任务p数据结构占用的内存页面。最后的执行调度程序
// 返回后退出。如果没有找到指定的任务p，则说明内核代码出错了，则显示出错信息并死机。
// 更新链接部分的代码会把指定任务p从双向链表中删除。
	for (i = 1; i < NR_TASKS; i++)
		if (task[i] == p) {
			task[i] = NULL;
			/* Update links */
// 如果p不是最后（最老）的子进程，则让比其老的比邻进程指向比它新的比邻进程。如果p
// 不是最新的子进程，则让比其新的比邻子进程指向比邻的老进程。如果任务p就是最新的
// 子进程，则还需要更新其父进程最新子进程指针cptr为指向p的比邻子进程。
// 参见图5-20中的说明。
// 指针osptr（old sibling pointer）指向比p先创建的兄弟进程。
// 指针ysptr（younger sibling pointer）指向比p后创建的兄弟进程。
// 指针pptr（parent pointer）指向p的父进程。
// 指针cptr（child pointer）是父进程指向最新（最后）创建的子进程。
			if (p->p_osptr)
				p->p_osptr->p_ysptr = p->p_ysptr;
			if (p->p_ysptr)
				p->p_ysptr->p_osptr = p->p_osptr;
			else
				p->p_pptr->p_cptr = p->p_osptr;
			free_page((long)p);
			schedule();
			return;
		}
	panic("trying to release non-existent task");
}

#ifdef DEBUG_PROC_TREE
/* 
 * Check to see if a task_struct pointer is present in the task[] array
 * Return 0 if found, and 1 if not found.
 */
// 检测任务结构指针p。
int bad_task_ptr(struct task_struct * p)
{
	int i;
	if (!p)
		return 0;
	for (i = 0; i < NR_TASKS; i++)
		if (task[i] == p)
			return 0;
	return 1;
}

/* 
 * This routine scans the pid tree and make sure the rep invarient still
 * holds.  Used  for debugging only, since it's very slow....
 * 
 * It looks a lot scarier than it really is.... we're doing nothing more
 * than verifying the doubly-linked list found in p_ysptr and p_osptr,
 * and checking it corresponds with the process tree defined by p_cptr and
 * p_pptr;
 */
// 检查进程树。
void audit_ptree()
{
	int i;
// 扫描系统中的除任务0以外的所有任务，检查它们中4个指针（pptr、cptr、ysptr和osptr）
// 的正确性。若任务数组槽（项）为空则跳过。
	for (i = 1; i < NR_TASKS; i++) {
		if (!task[i])
			continue;
// 如果任务父进程指针p_pptr没有指向任何进程（即在任务数组中不存在），则显示警告信息
// “警告，pid号N的父进程链接有问题”。以下语句对cptr、ysptr和osptr进行类似操作。
		if (bad_task_ptr(task[i]->p_pptr))
			printk("Warning, pid %d's parent link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_cptr))
			printk("Warning, pid %d's child link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_ysptr))
			printk("Warning, pid %d's ys link is bad\n",
				task[i]->pid);
		if (bad_task_ptr(task[i]->p_osptr))
			printk("Warning, pid %d's os link is bad\n",
				task[i]->pid);
// 如果任务的父进程指针p_pptr指向了自己，则显示警告信息“警告，pid号N的父进程链接
// 指针指向自己”。以下语句对cptr、ysptr和osptr进行类似操作。
		if (task[i]->p_pptr == task[i])
			printk("Warning, pid %d parent link points to self\n",
				task[i]->pid);
		if (task[i]->p_cptr == task[i])
			printk("Warning, pid %d child link points to self\n",
				task[i]->pid);
		if (task[i]->p_ysptr == task[i])
			printk("Warning, pid %d ys link points to self\n",
				task[i]->pid);
		if (task[i]->p_osptr == task[i])
			printk("Warning, pid %d os link points to self\n",
				task[i]->pid);
// 如果任务有比自己先创建的比邻兄弟进程，那么就检查它们是否有共同的父进程，并检查这个
// 老兄进程的ysptr指针是否正确地指向本进程，否则显示警告信息。
		if (task[i]->p_osptr) {
			if (task[i]->p_pptr != task[i]->p_osptr->p_pptr)
				printk(
			"Warning, pid %d older sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_osptr->p_ysptr != task[i])
				printk(
			"Warning, pid %d older sibling %d has mismatch ys link\n",
				task[i]->pid, task[i]->p_osptr->pid);
		}
// 如果任务有比自己后创建的比邻兄弟进程，那么就检查它们是否有共同的父进程，并检查这个
// 小弟进程的osptr指针是否正确地指向本进程，否则显示警告信息。
		if (task[i]->p_ysptr){
			if (task[i]->p_pptr != task[i]->p_ysptr->p_pptr)
				printk(
			"Warning, pid %d younger sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_ysptr->p_osptr != task[i])
				printk(
			"Warning, pid %d younger sibling %d has mismatched os link\n",
				task[i]->pid, task[i]->p_ysptr->pid);
		}

		if (task[i]->p_cptr) {
			if (task[i]->p_cptr->p_pptr != task[i])
				printk(
			"Warning, pid %d younger child %d has mismatched parent link\n",
				task[i]->pid, task[i]->p_cptr->pid);
			if (task[i]->p_cptr->p_ysptr)
				printk(
			"Warning, pid %d youngest child %d has non-NULL ys link\n",
				task[i]->pid, task[i]->p_cptr->pid);
		}
	}
}
#endif /* DEBUG_PROC_TREE */

//// 向任务p发送信号sig，权限为priv。
// 参数：sig - 信号值；p - 指定任务的指针；priv - 强制发送信号的标志。即不需要考虑进程
// 用户属性或级别而能发送信号的权利。该函数首先判断参数的正确性，然后判断条件是否满足。
// 如果满足就向指定进程发送信号sig并退出，否则返回未许可错误号。
static inline int send_sig(long sig, struct task_struct * p, int priv)
{
// 如果没有权限，并且当前进程的有效用户ID与进程p的不同，并且不是超级用户，则说明
// 没有向p发送信号的权利。suser()定义为（current->euid==0），用于判断是否是超
// 级用户。
	if (!p)
		return -EINVAL;
	if (!priv && (current->euid!=p->euid) && !suser())
		return -EPERM;
// 若需要发送的信号是SIGKILL或SIGCONT，那么如果此时接收信号的进程p正处于停止状态
// 就置其为就绪（运行）状态。然后修改进程p的信号位图signal，去掉（复位）会导致进程
// 停止的信号SIGSTOP、SIGTSTP、SIGTTIN和SIGTTOU。
	if ((sig == SIGKILL) || (sig == SIGCONT)) {
		if (p->state == TASK_STOPPED)
			p->state = TASK_RUNNING;
		p->exit_code = 0;
		p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
				(1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
	}
	/* If the signal will be ignored, don't even post it */
	if ((int) p->sigaction[sig-1].sa_handler == 1)
		return 0;
	/* Depends on order SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU */
// 如果信号是SIGSTOP、SIGTSTP、SIGTTIN和SIGTTOU之一，那么说明要让接收信号的进程p
// 停止运行。因此（若p的信号位图中有SIGCONT置位）就需要复位位图中继续运行的信号
// SIGCONT比特位。
	if ((sig >= SIGSTOP) && (sig <= SIGTTOU))
		p->signal &= ~(1<<(SIGCONT-1));
	/* Actually deliver the signal */
	p->signal |= (1<<(sig-1));
	return 0;
}

// 根据进程组号pgrp取得进程组所属的会话号。
// 扫描任务数组，寻找进程组号为pgrp的进程，并返回其会话号。如果没有找到指定进程组号
// 为pgrp的任何进程，则返回-1.
int session_of_pgrp(int pgrp)
{
	struct task_struct **p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p) //❓不应该是p>=&FIRST_TASK吗？
		if ((*p)->pgrp == pgrp)
			return ((*p)->session);
	return -1;
}

// 终止进程组（向进程组发送信号）。
// 参数：pgrp - 指定的进程组号； sig - 指定的信号； priv - 权限。
// 即向指定进程组pgrp中的每个进程发送指定信号sig。只要向一个进程发送成功最后就会
// 返回0，否则如果没有找到指定进程组号pgrp的任何一个进程，则返回出错号-ESRCH，若
// 找到进程组号为pgrp的进程，但是发送信号 失败，则返回发送失败的错误码。
int kill_pg(int pgrp, int sig, int priv)
{
	struct task_struct **p;
	int err, retval = -ESRCH;	// -ESRCH表示指定的进程不存在。
	int found = 0;

// 首先判断给定的信号和进程组是否有效，然后扫描系统中所有任务。若扫描到进程组号为
// pgrp的进程，就向其发送信号sig。只要有一次信号发送成功，函数最后就会返回0.
	if (sig<1 || sig>32 || pgrp<=0)
		return -EINVAL;
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if ((*p)->pgrp == pgrp) {
			if (sig && (err = send_sig(sig, *p, priv)))
				retval = err;
			else
				found++;
		}
	return(found ? 0: retval);
}

// 终止进程（向进程发送信号）。
// 参数：pid - 进程号；sig - 指定信号； priv - 权限。
// 即 向进程号为pid的进程发送指定信号sig。若找到指定pid的进程，那么若信号发送成功，
// 则返回0，否则返回信号发送出错号。如果没有找到指定进程号pid的进程，则返回出错号
// -ESRCH（指定进程不存在）。
int kill_proc(int pid, int sig, int priv)
{
	struct task_struct **p;

	if (sig<1 || sig>32)
		return -EINVAL;
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if ((*p)->pid == pid)
			return(sig ? send_sig(sig, *p, priv) : 0);
	return(-ESRCH);
}

/* 
 * POSIX specifies that kill(-1,sig) is unspecified, but what we have
 * is probably wrong. Should make it like BSD or SYSV.
 */
//// 系统调用kill()可用于向任何进程或进程组发送任何信号，而并非只是杀死进程。
// 参数pid是进程号； sig是需要发送的信号。
// 如果pid > 0，则信号被发送给进程号是pid的进程。
// 如果pid = 0，那么信号就会被发送给当前进程的进程组中的所有的进程。
// 如果pid = -1，则信号sig就会发送给除第一个进程（初始进程）外的所有进程。
// 如果pid < -1，则信号sig将发送给进程组-pid的所有进程。
// 如果信号sig为0，则不发送信号，但仍会进行错误检查。如果成功则返回0。
// 该函数扫描任务数组表，并根据pid对满足条件的进程发送指定信号sig。若pid等于0，
// 表示当前进程是进程组组长，因此需要向组内所有进程强制发送信号sig。
int sys_kill(int pid, int sig)
{
	struct task_struct **p = NR_TASKS + task;	// p指向任务数组最后一项。
	int err, retval = 0;

	if (!pid)
		return(kill_pg(current->pid, sig, 0));
	if (pid == -1) {
		while (--p > &FIRST_TASK)
			if (err = send_sig(sig, *p, 0))
				retval = err;
		return(retval);
	}
	if (pid < 0)
		return(kill_pg(-pid, sig, 0));
	/* Normal kill */
	return(kill_proc(pid, sig, 0));
}

/* 
 * Determine if a process group is "orphaned", according to the POSIX
 * definition in 2.2.2.52.  Orphaned process groups are not to be affected
 * by terminal-generated stop signals.  Newly orphaned process groups are
 * to receive a SIGHUP and a SIGCONT.
 * 
 * "I ask you, have you ever known what it is to be an orphan?"
 */
// 以上提到的POSIX P1003.1 2.2.2.52节是关于孤儿进程组的描述。在两种情况下当一个进程
// 终止时可能导致进程组变成“孤儿”。  一个进程组到其组外的父进程之间的联系依赖于该父
// 进程和其子进程两者。因此，若组外最后一个连接父进程的进程或最后一个父进程的直接后裔
// 终止的话，那么这个进程组就会成为一个孤儿进程组。在任何一种情况下，如果进程的终止导
// 致进程组变成孤儿进程组，那么进程组中的所有进程就会与它们的作业控制shell断开联系。
// 作业控制shell将不再具有该进程组存在的任何信息。而该进程组中处于停止状态的进程将会
// 永远消失。为了解决这个问题，含有停止状态进程的新近产生的孤儿进程组就需要接收到一个
// SIGHUP信号和一个SIGCONT信号，用于指示它们已经从它们的会话（session）中断开联系。
// SIGHUP信号将导致进程组中成员被终止，除非它们捕获或忽略了SIGHUP信号。而SIGCONT信
// 号将使那些没有被SIGHUP信号终止的进程继续运行。 介在大多数情况下，如果组中有一个进
// 程处于停止状态，那么组中所有的进程可能都处于停止状态。
// 
// 判断一个进程组是否是孤儿进程。如果不是则返回0；如果 是则返回1。
// 扫描任务数组。如果任务项空，或者进程的组号与指定的不同，或者进程已经处于僵死状态，
// 或者进程的父进程init进程，则说明扫描的进程不是指定进程组的成员，或者不满足要求，
// 于是跳过。  否则说明该进程是指定组的成员并且其父进程不是init进程。此时如果该进程
// 父进程的组号不等于指定的组号pgrp，但父进程的会话号等于进程的会话号，则说明它们同
// 属于琴会话。因此指定的pgrp进程组肯定不是孤儿进程组。否则...。
int is_orphaned_pgrp(int pgrp)
{
	struct task_struct **p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
		if (!(*p) ||
		     ((*p)->pgrp != pgrp) ||
		     ((*p)->state == TASK_ZOMBIE) ||
		     ((*p)->p_pptr->pid == 1))
		     	continue;
		if ((*p)->p_pptr->pgrp != pgrp &&
		    ((*p)->p_pptr->session == (*p)->session))
		    	return 0;
	}
	return(1);	/* (sighing) "Often!" */
}

// 判断进程组中是否含有处于停止状态的作业（进程组）。有则返回1；无则返回0。
// 查找方法是扫描整个任务数组。检查属于指定组pgrp的任何进程是否处于停止状态。
static int has_stopped_jobs(int pgrp)
{
	struct task_struct ** p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
		if ((*p)->pgrp != pgrp)
			continue;
		if ((*p)->state == TASK_STOPPED)
			return(1);
	}
	return(0);
}

// 程序退出 处理函数。在下面365行处被系统调用sys_exit()调用。
// 该函数将根据当前进程自身的特性对其进行处理，并把当前进程状态设置成僵死状态
// TASK_ZOMBIE，最后调用调度函数schedule()去执行其它进程，不再返回。
volatile void do_exit(long code)
{
	struct task_struct *p;
	int i;

// 首先释放当前进程代码段和数据段占的内存页。函数free_page_tables()的第1个参数
// （get_base()返回值）指明在CPU线性地址空间中起始基地址，第2个（get_limit()返回值）
// 说明欲释放的字节长度值。get_base()宏中的current->ldt[1]给出进程代码段描述符的位置，
// current->ldt[2]给出进程数据段描述符的位置；get_limit()中的0x0f是进程代码段的选择
// 符（0x17是进程数据段的选择符）。即在取段基地址时使用该段的描述符所处地址作为参数，
// 取段长度时使用该段的选择符作为参数（因为CPU有专用指令LSL通过选择符来取段长度）。
// 函数free_page_tables()函数位于mm/memory.c文件的第69行开始处；
// 宏get_base和get_limit()位于include/linux/sched.h头文件的第265行开始处。
	free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]), get_limit(0x17));

// 然后关闭当前进程打开着的所有文件。再对当前进程的工作目录pwd、根目录root、执行程序
// 文件的i节点以及库文件进行同步操作，放回各个i节点并分别置空（释放）。接着把当前
// 进程的状态设置为僵死状态（TASK_ZOMBIE）,并设置进程退出码。
	for (i = 0; i < NR_OPEN; i++)
		if (current->filp[i])
			sys_close(i);
	iput(current->pwd);
	current->pwd = NULL;
	iput(current->root);
	current->root = NULL;
	iput(current->executable);
	current->executable = NULL;
	iput(current->library);
	current->library = NULL;
	current->state = TASK_ZOMBIE;
	current->exit_code = code;
	/* 
	 * Check to see if any process groups have become orphaned
	 * as a result of our exiting, and if they have any stopped
	 * jobs, send them a SIGHUP and then a SIGCONT.  (POSIX 3.2.2.2)
	 * 
	 * Case i: Our father is in a different pgrp than we are
	 * and we were the only connection outside, so our pgrp
	 * is about to become orphaned.
	 */
// POSIX 3.2.2.2（1991版）是关于exit()函数的说明。如果父进程所在的进程组与当前进程的
// 不同，但都处于同一个会话（session）中，并且当前进程所在进程组将要变成孤儿进程了并且
// 当前进程的进程组中含有处于停止状态的作业（进程），那么就要向这个当前进程的进程组发
// 送两个信号：SIGHUP和SIGCONT。发送这两个信号的原因见232行前的说明。
	if ((current->p_pptr->pgrp != current->pgrp) &&
	    (current->p_pptr->session == current->session) &&
	    is_orphaned_pgrp(current->pgrp) && 
	    has_stopped_jobs(current->pgrp)) {
		kill_pg(current->pgrp, SIGHUP, 1);
		kill_pg(current->pgrp, SIGCONT, 1);
	}
	/* Let father know we died */
	current->p_pptr->signal |= (1<<(SIGCHLD-1));

	/* 
	 * This loop does two things:
	 *
	 * A.  Make init inherit all the child processes
	 * B.  Check to see if any process groups have become orphaned
	 *      as a result of our exiting, and if they have any stopped
	 *      jons, send them a SIGHUP and then a SIGCONT.  (POSIX 3.2.2.2)
	 */
// 如果当前进程有子进程（其p_cptr指针指向最近创建的子进程），则让进程1（init进程）
// 成为其所有子进程的父进程。如果父进程已经处于僵死状态，则向init进程（父进程）发送
// 子进程已终止信号SIGCHLD。
	if (p = current->p_cptr) {
		while (1) {
			p->p_pptr = task[1];
			if (p->state == TASK_ZOMBIE)
				task[1]->signal |= (1<<(SIGCHLD-1));
			/* 
			 * process group orphan check
			 * Case ii: Our child is in a different pgrp
			 * than we are, and it was the only connection
			 * outside, so the child pgrp is now orphaned
			 */
// 如果子进程与当前进程不在同一个进程组中但属于同一个session中，并且当前进程所在进程
// 组将要变成孤儿进程了，并且当前进程的进程组中含有处于停止状态的作业（进程），那么就
// 要向这个当前进程的进程组发送两个信号：SIGHUP和SIGCONT。  如果该子进程有兄弟进程，
// 则继续循环处理这些兄弟进程。
			if ((p->pgrp != current->pgrp) &&
			    (p->session == current->session) &&
			    is_orphaned_pgrp(p->pgrp) &&
			    has_stopped_jobs(p->pgrp)) {
				kill_pg(p->pgrp, SIGHUP, 1);
				kill_pg(p->pgrp, SIGCONT, 1);
			}
			if (p->p_osptr) {
				p = p->p_osptr;
				continue;
			}
			/* 
			 * This is it; link everything into init's children
			 * and leave
			 */
// 通过上面处理，当前进程子进程的所有兄弟子进程都已经处理过。此时p指向最老的兄弟子
// 进程。于是把这些兄弟子进程全部加入init进程的子进程双向链表头部中。加入后，init
// 进程的p_cptr指向当前进程原子进程中最年轻的（the youngest）子进程，而原子进程中
// 最老的（the oldest）兄弟子进程p_osptr指向原init进程的最年轻进程，而原init进
// 程中最年轻进程的p_ysptr指向原子进程中最老的兄弟子进程。最后把当前进程的p_cptr
// 指针置空，并退出循环。
			p->p_osptr = task[1]->p_cptr;
			task[1]->p_cptr->p_ysptr = p;
			task[1]->p_cptr = current->p_cptr;
			current->p_cptr = 0;
			break;
		}
	}
// 如果当前进程是会话头领（leader）进程，那么若它有控制终端，则首先向使用该控制终端的
// 进程组发送挂断信号SIGHUP，然后释放该终端。接着扫描任务数组，把属于当前进程会话中
// 进程的终端置空（取消）。
	if (current->leader) {
		struct task_struct **p;
		struct tty_struct *tty;

		if (current->tty >= 0) {
			tty = TTY_TABLE(current->tty);
			if (tty->pgrp > 0)
				kill_pg(tty->pgrp, SIGHUP, 1);
			tty->pgrp = 0;
			tty->session = 0;
		}
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if ((*p)->session == current->session)
				(*p)->tty = -1;
	}
// 如果当前进程上次使用过协处理器，则把记录此信息的指针置空。若定义了调试进程树符号，
// 则调用进程树检测显示函数。最后调用调度函数，重新调度进程运行，以让父进程能够处理
// 僵死进程的其它善后事宜。
	if (last_task_used_math == current)
		last_task_used_math = NULL;
#ifdef DEBUG_PROC_TREE
	audit_ptree();
#endif
	schedule();
}

// 系统调用exit()。终止进程。
// 参数error_code是用户程序提供的退出状态信息，只有低字节有效。把error_code左移8
// 比特是wait()或waitpid函数的要求。低字节中将用来保存wait()的状态信息。例如，
// 如果进程处于暂停状态（TASK_STOPPED），那么其低字节就等于0x7f。参见sys/wait.h
// 文件第13--19行。wait()或waitpid()利用这些宏就可以取得子进程退出状态码或子
// 子进程终止的原因（信号）。
int sys_exit(int error_code)
{
	do_exit((error_code&0xff)<<8);
}

// 系统调用waitpid()。挂起当前进程，直到pid指定的子进程退出（终止）或收到要求终止
// 该进程的信号，或者是需要调用一个信号句柄（信号处理程序）。如果pid所指的子进程早已
// 退出（已成所谓的僵死进程），则本调用将立刻返回。子进程使用的所有资源将释放。
// 如果pid > 0，表示等待进程号等于pid的子进程。
// 如果pid = 0，表示等待进程组号等于当前进程组号的任何子进程。
// 如果pid < -1，表示等待进程组号等于pid绝对值的任何子进程。
// 如果pid = -1，表示等待任何子进程。
// 若options = WUNTRACED，表示如果子进程是停止的，也马上返回（无须跟踪）。
// 若options = WNOHANG，表示如果没有子进程退出或终止就马上返回。
// 如果返回状态指针stat_addr不为空，则就将状态信息保存到那里。
// 参数pid是进程号；*stat_addr是保存状态信息位置的指针；options是waitpid选项。
int sys_waitpid(pid_t pid, unsigned long * stat_addr, int options)
{
	int flag;		// 该标志用于后面表示所先出的子进程处于就绪或睡眠态。
	struct task_struct *p;
	unsigned long oldblocked;

// 首先验证次要存放状态信息的位置处内存空间足够。然后复位标志flag。接着从当前进程的最
// 年轻子进程开始扫描子进程兄弟链表。
	verify_area(stat_addr, 4);
repeat:
	flag = 0;
	for (p = current->p_cptr; p; p = p->p_osptr) {
// 如果等待的子进程号pid>0，并且与被扫描子进程p的pid不相等，说明它是当前进程另外的子
// 进程。于是跳过，接着扫描下一进程。否则表示找到等待的子进程pid，于是到390行继续执行。
		if (pid > 0) {
			if (p->pid != pid)
				continue;
// 否则，如果指定等待进程的pid=0，表示正在等待进程组号等于当前进程组号的任何子进程。
// 如果此时被扫描进程p的进程组号与当前进程的组号不等，则跳过。否则表示找到进程组号
// 等于当前进程组号的某个子进程，于是到390行继续执行。
		} else if (!pid) {
			if (p->pgrp != current->pgrp)
				continue;
// 否则，如果指定的pid < -1，表示正在等待进程组号等于pid绝对值的任何子进程。如果此时
// 被扫描进程p的组号与pid的绝对值不等，则跳过。否则表示找到进程组号等于pid绝对值的
// 某个子进程，于是到390行继续执行。
		} else if (pid != -1) {
			if (p->pgrp != -pid)
				continue;
		}
// 如果前3个对pid的判断都不符合，则表示当前进程正等待其任何子进程（此时pid = -1）。
// 
// 此时所选择到的进程p或者是其进程号等于指定pid，或者是当前进程组中的任何子进程，
// 或者是进程号等于指定pid绝对值的子进程，或者是任何子进程（此时指定的 pid = -1）。
// 接下来根据这个子进程p所处的状态来处理。
// 
// 当子进程p处于停止状态时，如果此时参数选项options中WUNTRACED标志没有置位，表示
// 程序无须立刻返回，或者子进程此时的退出码不等于 0，于是继续扫描处理其他子进程。如果
// WUNTRACED置位且子进程退出码不为0，则把码移入高字节，‘或’上状态0x7f后放入
// *stat_addr，在复位子进程退出码后就立刻返回子进程号pid。这里0x7f表示的返回状态使
// WIFSTOPPED()宏为真。参见include/sys/wait.h，14行。
		switch (p->state) {
			case TASK_STOPPED:
				if (!(options & WUNTRACED) ||
				    !p->exit_code)
					continue;
				put_fs_long((p->exit_code << 8) | 0x7f,
					stat_addr);
				p->exit_code = 0;
				return p->pid;
// 如果子进程p处于僵死状态，则首先把它在用户态和内核态运行的时间分别累计到当前进程
// （父进程）中，然后取出子进程的pid和退出码，把退出码放入返回状态位置stat_addr处
// 并释放该子进程。最后返回子进程的退出码和pid。若定义了调试进程树符号，则调用进程
// 树检测显示函数。
			case TASK_ZOMBIE:
				current->cutime += p->utime;
				current->cstime += p->stime;
				flag = p->pid;
				put_fs_long(p->exit_code, stat_addr);
				release(p);
#ifdef DEBUG_PROC_TREE
				audit_ptree();
#endif
				return flag;
// 如果这个子进程p的状态既不是停止也不是僵死，那么就置flag = 1。表示找到过一个符合
// 要求的子进程，但是它处于运行态或睡眠态。
			default:
				flag = 1;
				continue;
		}
	}
// 在上面对任务数组扫描结束后，如果flag被置位，说明有符合等待要求的子进程并没有处
// 于退出或僵死状态。此时如果已设置WNOHANG选项（表示若没有子进程处于退出或终止态就
// 立刻返回），就立刻返回0并退出。否则把当前进程置为可中断等待状态，保留并修改当
// 前进程信号阻塞位图，允许其接收到SIGCHLD信号。然后执行调度程序。当系统又开始执行
// 本进程时，如果本进程收到除SIGCHLD以外的其他未屏蔽信号，则以退出码“重新启动系统
// 调用”返回。否则跳转到函数开始处repeat标号处重复处理。
	if (flag) {
		if (options & WNOHANG)
			return 0;
		current->state = TASK_INTERRUPTIBLE;
		oldblocked = current->blocked;
		current->blocked &= ~(1<<(SIGCHLD-1));
		schedule();
		current->blocked = oldblocked;
		if (current->signal & ~(current->blocked | (1<<(SIGCHLD-1))))
			return -ERESTARTSYS;
		else 
			goto repeat;
	}
// 若 flag = 0，表示没有找到符合要求的子进程，则返回出错码（子进程不存在）。
	return -ECHILD;
}
