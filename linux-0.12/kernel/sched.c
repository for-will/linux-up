/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task)
 */
// 下面是调度程序的头文件。定义了任务结构task_struct、第1个初始任务的数据。还有一些以宏
// 的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序。
#include <linux/sched.h>
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用函数的原形定义。
#include <linux/sys.h>		// 系统调用头文件。含有82个系统调用C函数程序，以'sys_'开头。
#include <linux/fdreg.h>	// 软驱头文件。含有软盘控制器参数的一些定义。
#include <asm/system.h>		// 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏。
#include <asm/io.h>		// io头文件。定义硬件商品输入/输出宏汇编语句。
#include <asm/segment.h>	// 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。

#include <signal.h>		// 信号头文件。定义了有关信号符号常量，sigaction结构，操作函数原型。

#include <sys/types.h>		// 定义了 NULL

// 该宏取信号nr在信号位图中对应位的二进制数值。信号编号1-32。
// 例如，信号5的位图数值等于1<<(5-1) = 16 = 00010000b。
// 另外，除了SIGKILL和SIGSTOP信号以外其他信号都是可阻塞的（...1011,1111,1110,1111,1111b）
#define _S(nr) 		(1<<((nr)-1))
#define _BLOCKABLE 	(~(_S(SIGKILL) | _S(SIGSTOP)))

// 内核调试函数。显示任务号nr的进程号、进程状态、内核堆栈空闲字节数（大约）及其相关的
// 子进程和父进程信息。
// 因为任务结构的数据和任务的内核态栈在同一内存页面上，且任务内核态栈从页面末端开始，
// 因此，28行上的j即表示最大内核栈容量，或内核栈最低顶端位置。
// 参数：
// nr - 任务号； p - 任务结构指针。
void show_task(int nr, struct task_struct * p)
{
	int i, j = 4096 - sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, father=%d, child=%d, ", nr, p->pid,
		p->state, p->p_pptr->pid, p->p_cptr ? p->p_cptr->pid : -1);
	i = 0;
	while(i<j && !((char *)(p+1))[i])	// 检测指定任务数据结构以后等于0的字节数。
		i++;
	printk("%d/%d chars free in kstack\n\r", i, j);
	printk("	PC=%08X", *(1019 + (unsigned long *) p));
	if (p->p_ysptr || p->p_osptr)
		printk("	Younger sib=%d, older sib=%d\n\r",
			p->p_ysptr ? p->p_ysptr->pid : -1,
			p->p_osptr ? p->p_osptr->pid : -1);
	else
		printk("\n\r");
}

// 显示系统中所有任务的状态信息。
// NR_TASKS是系统能容纳的最大任务数量（64个），定义在include/linux/sched.h 第6行。
void show_state(void)
{
	int i;
	printk("\rTask-info:\n\r");
	for (i = 0; i < NR_TASKS; i++)
		if (task[i])
			show_task(i, task[i]);
}

// PC机8253计数/定时芯片的输入时钟频率约为1.193180MHz。Linux内核希望定时器中断频率是
// 100Hz，也即每10ms发出一次时钟中断。因此这里LATCH是设置8253芯片的初值，参见438行。
#define LATCH (1193180/HZ)

// extern void mem_use(void);		// [??]没有任何地方定义和引用该函数。

extern int timer_interrupt(void);	// 定时中断程序（kernel/system_call.s，189）。
extern int system_call(void);		// 系统调用中断程序（kernel/sys_call.s，84）。

// 每个任务（进程）在内核态运行时都有自己的内核态堆栈。这里定义了任务的内核态堆栈结构。
// 这里定义任务联合（任务结构成员和stack字符数组成员）。因为一个任务的数据结构与其内核
// 态堆栈放在同一内在页中，所以从堆栈寄存器ss可以获得数据段选择符。
// 67行上设置初始任务的数据。初始数据在include/linux/sched.h中，第156行开始。
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

// 从开机算起的滴答数（10ms/滴答）。系统时钟中断每发生一次即一个滴答。
// 前面的限定符volatile，英文解释意思是易变的、不稳定的。这个限定词的含义是向编译器
// 指明变量的内容可能会因被其他程序修改而变化。通常在程序中申明一个变量时，编译器会
// 尽量把它放在通用寄存器中，例如EBX，以提高访问效率。此后编译器就不会再关心该变量
// 在对应内存位置中原来的内容。若此时其他程序（例如内核程序或中断过程）修改了该变量
// 在对应内在位置的值，EBX中的值并不会随之更新。为了解决这种情况就创建了volatile
// 限定符，让代码在引用该变量时一定要从指定内存位置中取得其值，这里即要求gcc不要对
// jiffies 进行代码优化处理，也不要挪动位置，并且需要从内存中取其值。因为时钟中断
// 处理过程等程序会修改它的值。
unsigned long volatile jiffies = 0;	// 内核脉搏（嘀嗒）。
unsigned long startup_time = 0;		// 开机时间。从1970-0-0-0开始计时的秒数。
// 这个变量用于累计需要调整的时间嘀嗒数。
int jiffies_offset = 0;		/* # clock ticks to add to get "true
				    time". Should always be less than
				    1 second's worth. For time fanatis
				    who like to syncronize their machines
				    to WWV :-) */

struct task_struct *current = &(init_task.task);	// 当前任务指针（初始化指向任务0）。
struct task_struct *last_task_used_math = NULL;		// 使用过协处理器任务的指针。

// 定义任务指针数组。第1项被初始化指向初始任务（任务0）的任务数据结构。
struct task_struct * task[NR_TASKS] = {&(init_task.task), };

// 定义用户堆栈（数组），共1K项，容量4K字节。在刚开始初始化操作过程中被用作内核栈，
// 初始化操作完成后将被用作任务0的用户态堆栈。在运行任务0之前它是内核栈，以后用作任务0
// 和任务1的用户态栈。下面结构用于设置堆栈SS:ESP（数据段选择符，偏移），见head.s，23行。
// SS被设置为内核数据段选择符（0x10），ESP被设置为指向 user_stack数组最后一项后面。这是
// 因为 Interl CPU执行堆栈制作时是先递减堆栈指针ESP值，然后在ESP指针处保存入栈内容。
long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

/*
 * 'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
// 当任务被高度交换过以后，该函数用以保存原任务的协处理器状态（上下文）并恢复新调度进
// 来的当前任务的协处理器执行状态。
void math_state_restore()
{
// 如果任务没变则返回（上一个任务就是当前任务）。这里“上一个任务”是指刚被交换出去的任务。
// 另外在发送协处理器命令之前要先发WAIT指令。如果上一个任务使用了协处理器，则保存其状态
// 至任务数据结构的TSS字段中。
	if (last_task_used_math == current)
		return;
		
	__asm__("fwait");
	if (last_task_used_math){
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
// 现在，last_task_use_math指向当前任务，以备当前任务被交换出去时使用。此时如果当前
// 任务用过协处理器，则恢复其中状态。否则的话说明是第一次使用，于是就向协处理器发初始化
// 命令，并设置使用了协处理器标志。
	last_task_used_math = current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	}else {
		__asm__("fninit"::);		// 向协处理器发初始化命令。
		current->used_math = 1;		// 设置使用协处理器标志。
	}
	
}

/**
 * 'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (iej gives IO-bound pocesses good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 * 
 *  NOTE!! Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i, next, c;
	struct task_struct ** p;		// 任务结构指针的指针。

/* check alarm, wake up any interruptible tasks that have got a signal */

// 从任务数组中最后一个任务开始循环检测alarm。在循环时路过空指针项。
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p) {
// 如果设置过任务超时定时值timeout，并且已经超时，则复位超时定时值，并且如果任务处于可
// 中断睡眠状态TASK_INTERRUPTIBLE下，将其置为就绪状态（TASK_RUNNING）。
			if ((*p)->timeout && (*p)->timeout < jiffies){
				(*p)->timeout = 0;
				if ((*p)->state == TASK_INTERRUPTIBLE)
					(*p)->state = TASK_RUNNING;
			}
// 如果设置过任务的SIGALRM信号超时定时器值alarm，并且已经过期（alarm<jiffies），则在信号
// 位图中置SIGALRM信号，即向任务发送SIGALRM信号。然后清alarm。该信号的默认操作是终止
// 进程。jiffies是系统从开机开始算起的滴答数（10ms/滴答）。定义在sched.h第139行。
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1<<(SIGALRM-1));
				(*p)->alarm = 0;
			}
// 如果信号位图中除被阻塞的信号外还有其他信号，并且任务处于可中断状态，则置任务为就绪
// 状态。其中'～(_BLOCKABLE & (*p)->blocked)'用于忽略被阻塞的信号。但SIGKILL和
// SIGSTOP不能被阻塞。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state = TASK_RUNNING;	// 置为就绪（可执行）状态。
		}
/* this is the scheduler propr: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
// 这段代码也是从任务数组的最后一个任务开始循环处理，并路过不含任务的数组槽。它比较每个
// 就绪状态任务的counter（任务运行时间的递减滴答计数）值，哪一个值大，就表示相应任务的
// 运行时间还有很多，next就指向哪个的任务号。
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
// 如果比较得出有counter值不等于0的结果，或者系统中没有一个可运行的任务存在（此时c仍
// 为-1，next=0），则退出外层while循环（144行），并执行后面的任务切换操作（161行）。
// 否则就根据每个任务的优先权值，更新每一个任务的counte值，然后再回到144行重新比较。
// counter值的计算方式为 counter = counter/2 + priority
// 注意，这里计算过程不考虑进程的状态。
		if (c) break;
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
// 下面宏（在sched.h中）把上面选出来的任务next作为湔任务current，并切换到该任务
// 中运行。因为next在146行上被初始化为0，因此若系统中没有任何其他任务可运行时，则
// next始终为0.结果是高度函数会在系统空闲时去执行任务0。此时任务0仅执行 pause()
// 系统调用，并不会调用本函数。
	switch_to(next);	// 切换到任务号为next的任务，并运行之。
}

//// 下面是pause()系统调用，用于转换当前任务的状态为可中断的等待状态，并重新高度。
// 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程
// 调用一个信号捕获函数。只有当捕获了一个信号，并且信号捕获处理函数返回，pause()才
// 会返回。此时 pause()返回值应该是 -1，并且errno被置为 EINTR。这里还没有完全实
// 现（直到0.95版）。
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

// 下面函数把当前任务置为可中断的或不可中断的睡眠状态，并让睡眠队列头指针指向当前任务。
// 函数参数p是等待任务队列头指针。指针是含有一个变量地址的变量。这里参数p使用了指针的
// 指针形式 '**p'，这是因为C函数参数只能传值，没有直接的方式让被调用函数改变调用该函数
// 程序中变量的值。但是指针'*p'指向的目标（这里是任务结构）会改变，因此为了能修改调用该
// 函数程序中原来就是指针变量的值，就需要传递指针'*p'的指针，即'**p'。参见程序前示例图
// 中p指针的使用情况。
// 参数state是任务睡眠使用的状态：TASK_UNINTERRUPTIBLE或TASK_INTERRUPTIBLE。处于不可
// 中断睡眠状态（TASK_UNINTERRUPTIBLE）的任务需要内核程序利用wake_UP()函数明确唤醒之。
// 处于可中断睡眠状态（TASK_INTERRUPTIBLE）的任务可以通过信号、任务超时等手段唤醒
// （置为就绪状态 TASK_RUNNING）。
// *** 注意，由于本内核代码还是很成熟，因此下列与睡眠相关的代码还存在一些问题。
static inline void __sleep_on(struct task_struct **p, int state)
{
	struct task_struct *tmp;
// 若指针无效，则退出。（指针所指的对象可以是NULL，但指针本身不会为0）。
// 如果当前任务是任务0，则死机（impossible!）。
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
// 让tmp指向已经在等待队列上的任务（如果有的话），例如inode->i_wait，并且将睡眠队列头
// 的指针指向当前任务。这样就把当前任务插入到等待队列中。然后将当前任务置为指定
// 的等待状态，并执行重新高度。
	tmp = *p;
	*p = current;
	current->state = state;
repeat:	schedule();

// 只有当这个等待任务被唤醒时，程序才又会从这里继续执行。表示进程已被明确地唤醒并执行。
// 如果队列中还有等待的任务，并且队列头指针 *p所指向的任务还是上当前任务，则说明在本任务
// 插入队列后还有任务进入队列，于是我们应该也要唤醒这些后续进入队列的任务，因此这里将队列
// 头所指任务先置为就绪状态，而自己则置为不可中断等待状态，即要等待这些后续进入队列的任务
// 被唤醒后才用wake_up()唤醒本任务。然后中转至repeat标号处重新执行高度函数。
	if (*p && *p != current) {
		(**p).state = 0;
		current->state = TASK_UNINTERRUPTIBLE;
		goto repeat;
	}

// 执行到这里，说明任务真正被唤醒执行。此时等待队列头指针应该指向本任务。若它为空，
// 则表明调度有问题，于是显示警告信息。最后我们让头指针指向在我们的前面进入队列的任务
// (*p = tmp)。若确实存在这样一个任务，即队列中还有任务（tmp不为空），就唤醒之。因
// 此，最先进入队列的任务在唤醒后运行时最终会把等待队列头指针置成NULL。
	if (!*p)
		printk("Warning: *p = NULL\n\r");
	if (*p = tmp)
		tmp->state = 0;
}

// 将当前伤置为可中断的等待状态（TASK_INTERRUPTIBLE），并被放入头指针*p指定的等待
// 队列中。这种等待状态的任务可以通过信号、任务超时等手段唤醒。
void interruptible_sleep_on(struct task_struct **p)
{
	__sleep_on(p, TASK_INTERRUPTIBLE);
}

// 把当前任务置为不可中断的等待状态（TASK_UNINTERRUPTIBLE），并让睡眠队列头指针指向
// 当前任务。这种等待状态的任务需要利用wake_up()函数来明确唤醒。该函数提供了进程与中
// 断处理程序之间的同步机制。
void sleep_on(struct task_struct **p)
{
	__sleep_on(p, TASK_UNINTERRUPTIBLE);
}

// 唤醒不可中断等待任务。*p是任务等待队列头指针。由于新等待任务是插入在等待队列头指针
// 处的，因此唤醒的是最后进入等待队列的任务。若该任务已经处于停止或僵死状态，则显示警告
// 信息。
void wake_up(struct task_struct **p){
	if (p && *p) {
		if ((**p).state == TASK_STOPPED)	// 处于停止状态。
			printk("wake_up: TASK_STOPED");
		if ((**p).state == TASK_ZOMBIE)		// 处于僵死状态。
			printk("wake_up: TASK_ZOMBIE");
		(**p).state = 0;			// 置为就绪状态 TASK_RUNNING。
	}
}

/*
 * Ok, here are some floppy things that should't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
// 下面220－－281行代码用于处理软驱定时。在阅读这段代码之前请先看一下块设备一章中有关
// 软盘驱动程序（floppy.c）后面的说明，或者到阅读软盘块设备驱动程序时再来看这段代码。
// 
// 数组wait_motor[]用于存放等待马达启动到正常转速的进程指针。数组索引　0－3分别对应软驱A－D。
// 数组mon_timer[]存放各软驱马达启动所需要的滴答数。默认启动时间为50个滴答（0.5秒）。
// 数组moff_timer[]存放各软驱在马达停转之前需维持的时间。程序中设定为10000个滴答（100秒）。
static struct task_struct * wait_motor[4] = {NULL, NULL, NULL, NULL};
static int mon_timer[4] = {0, 0, 0, 0};
static int moff_timer[4] = {0, 0, 0, 0};

// 下面变量对应软驱控制器中当前数字输出寄存器（DOR）。该寄存器每位的定义如下：
// 位7－4：分别控制驱动器D-A马达的启动。1－启动；0－关闭。
// 位3：1　－　允许DMA和中断请求；　0　－　禁止DMA和中断请求。
// 位2：1　－　启动软盘控制器；　　　0　－　复位软盘控制器。
// 位1－0：00b－11b用于选择软驱A－D。
// 这里设置初始值为：允许DMA和中断请求、启动FDC。
unsigned char current_DOR = 0x0C;

// 指定软驱启动到正常运转状态所需等待时间。
// 参数nr　－－　软驱号（0－－3），返回值为滴答数
// 变量selected是选中软驱标志（blk_drv/floppy.c，123行）。mask是所选软驱对应的
// 数字输出寄存器DOR中启动马达比特位。mask高4位是各软驱启动马达标志。
int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;
// 系统最多有4个软驱。首先预先设置好指定软驱nr停转之前需要经过的时间（100秒）。然后
// 取当前数字输出寄存器DOR值到临时变量mask中，并把指定软驱的马达启动标志置位。
	if (nr > 3)
		panic("floppy_on: nr>3");
	moff_timer[nr] = 10000;		/* 100 s = very big :-) */ // 停转维持时间。
	cli();				/* use floppy_off to turn it off */ // 关中断。
// 如果当前没有选择软驱，则首先复位其他软驱的选择位，然后置指定软驱选择位。
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
// 如果数字输出寄存器DOR的当前值与要求值不同，则向FDC数字输出端口FD_DOR输出新值（mask），
// 并且如果要求启动的马达还没有启动，则置相应软驱的马达启动定时器值（HZ/2 = 0.5秒或50个
// 滴答）。若已经启动，则再设置启动定时为2个滴答，能满足下面do_floopy_timer()中先递减后
// 判断的要求。此后更新当前数字输出寄存器current_DOR。
// 240行上的FD_DOR是软驱数字输出寄存器DOR的端口（0x3F2），参见程序后的说明。
	if (mask != current_DOR) {
		outb(mask, FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();				// 开中断
	return mon_timer[nr];		// 最后返回启动马达所需的时间值。
}

// 等待指定软驱马达启动所需的一段时间。
// 设置指定软驱的马达启动到正常转速所需的延时，然后睡眠等待。在定时中断过程中会一直
// 递减判断这里设定的延时值。当延时到时，就会唤醒这里的等待进程。
void floopy_on(unsigned int nr)
{
// 关中断。如果马达启动定时还没到，就一直把当前进程置为不可中断睡眠状态并放入等待马达
// 运行的队列中。然后开中断。
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

// 设置关闭相应软驱马达停转定时器（3秒）。
// 若不使用该函数明确关闭指定的软驱马达，则在马达开启100秒之后也会被关闭。
void floppy_off(unsigned int nr)
{
	moff_timer[nr] = 3*HZ;
}

// 软盘定时器处理子程序。更新马达启动定时值和马达关闭停转计时值。该子程序会在时钟定时
// 中断过程中被调用，因此系统每经过一个滴答（10ms）就会被调用一次，随时更新马达开启或
// 停转定时器的值。如果某一个马达停转定时到，则将数字输出寄存器马达启动位复位。
void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

// 针对系统中具有的4个软驱，逐一检查使用中的软驱。如果还是DOR指定的马达，则路过。
// 如果马达起动定时到则唤醒相应进程。如果马达停转定时到，则复位DOR中相应马达启动位。
	for (i=0; i<4; i++,mask<<=1){
		if (!(mask & current_DOR))		// 如果还是DOR指定的马达则路过。
			continue;
		if (mon_timer[i]) {			// 如果马达启动定时到则唤醒进程。
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {		// 如果马达停转定时到则
			current_DOR &= ~mask;		// 复位相应马达启动位，并且
			outb(current_DOR, FD_DOR);	// 更新数字输出寄存器。
		} else
			moff_timer[i]--;		// 否则马达停转计时递减。
	}
}

// 下面是关于内核定时器的代码。最多可有64个定时器。
#define TIME_REQUESTS 64

// 定时器链表结构和定时器数组。该定时器链表专用于供软驱关闭马达和启动马达定时操作。
// 这种类型定时器类似现代Linux系统中的动态定时器（Dynamic Timer），仅供内核使用。
static struct timer_list {
	long jiffies;			// 定时滴答数。
	void (*fn)();			// 定时处理程序。
	struct timer_list * next;	// 链接指向下一个定时器
} timer_list[TIME_REQUESTS], * next_timer = NULL; // next_timer是定时器队列头指针。

// 添加定时器。输入参数为指定的定时值（滴答数）和相应的处理程序指针。
// 软盘驱动程序（floppy.c）利用该函数执行启动或关闭马达的延时操作。
// 参数jiffies - 以10毫秒计的滴答数；*fn() - 定时时间到时执行的函数。
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

// 如果定时处理程序指针为空，则退出。否则关中断。
	if (!fn)
		return;
	cli();
// 如果定时值<=0，则立刻调用其处理程序。并且该定时器不加入链表中。
	if (jiffies <= 0)
		(fn)();
	else {
// 否则从定时器数组中，找一个空闲项。
		for (p = timer_list; p < timer_list + TIME_REQUESTS; p++)
			if (!p->fn)
				break;
// 如果已经用完了定时器数组，则系统崩溃。否则向定时吕数据结构填入相应信息，并链入
// 链表头。
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more timer requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
// 链表项按定时值从小到大排序。在排序时减去排在前面需要的滴答数，这样在处理定时器时
// 只要查看链表头的第一项的定时是不到期即可。

		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

//// 定时器中断C函数处理程序，在sys_call.s中的_timer_interrupt（189行）中被调用。
// 参数cpl是当前特权级0或3，它是时钟中断发生时正被执行的代码选择符中的特权级。
// cpl=0时表示中断发生时正在执行内核代码；cpl=3时表示中断发生时正在执行用户代码。
// 对于一个任务，若其执行时间片用完，则进行任务切换。同时函数执行一个计时更新工作。
void do_timer(long cpl)
{
	static int blanked = 0;

// 首先判断是否需要执行黑屏（bankout）操作。如果blankcount计数不为零，或者黑屏
// 延时间隔时间blankinterval为0的话，那么若已经处于黑屏状态（黑屏标志blanked=1），
// 则让屏幕恢复显示。若blankcount计数不为零，则递减之，并且复位黑屏标志。
	if (blankcount || !blankinterval) {
		if (blanked)
			unblank_screen();
		if (blankcount)
			blankcount--;
		blanked = 0;
// 否则的话若黑屏标志未置位，则让屏幕黑屏，并且设置黑屏标志。
	} else if (!blanked) {
		blank_screen();
		blanked = 1;
	}

// 接着处理硬盘操作超时问题。如果硬盘超时计数递减之后为0，则进行硬盘访问超时处理。
	if (hd_timeout)
		if (!--hd_timeout)
			hd_times_out();	// 硬盘访问超时处理（blk_drv/hd.c，318行）。

// 如果发声计数次数到，则关闭发声。（向0x61口发送命令，复位位0和1。位0控制8253
// 计数器2的工作，位1控制气场器）。
	if (beepcount)			// 气场器发声时间滴答数（chr_drv/console.c，950行）。
		if (!--beepcount)
			sysbeepstop();	// 停止扬声器发声（chr_drv/onsole.c，944行）

// 如果当前特权级（cpl）为0（最高，表示是内核程序在工作），则将内核代码支行时间stime
// 递减：【Linus把内核程序统称为超级用户（supervisor）的程序，见sys_call.s，207行
// 上的英文注释。这种称呼来于Intel CPU手册。】如果cpl>0，则表示是一般用户程序在工作
// ，增加utime。
	if (cpl)
		current->utime++;
	else
		current->stime++;

// 如果有定时器存在，则将链表第1个定时器的值减1。如果已等于0，则调用相应的处理程序，
// 并将该处理程序指针置为空。然后去掉该项定时器。
	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);	// 这里插入了一个函数指针定义！！😔

			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();			// 调用定时处理函数。
		}
	}
// 如果当前软盘控制器FDC的数字输出寄存器DOR中马达启动位有置位，则执行软盘定时程序。
	if (current_DOR & 0xf0)
		do_floppy_timer();		// 前面第264行开始。
// 如果任务运行时间还没用完，则退出这里继续运行该任务。否则置当前任务运行计数值为0。并且
// 若发生时钟中断时正在内核代码中运行则返回，否则表示在执行用户程序，于是调用高度函数尝试
// 执行任务切换操作。
	if ((--current->counter)>0) return;
	current->counter = 0;
	if (!cpl) return;			// 对于内核态程序，不依赖counter值进行高度。
	schedule();
}

// 系统调用功能 - 设置报警定时器时间值（秒）。
// 若参数seconds > 0，则设置新定时时间值，并返回原定时时间刻还剩余的时间，否则返回0。
// 进程数据结构中报警字段alarm的单位是系统滴答，它是系统开机运行到现在的嘀嗒数jiffies
// 与定时值之和，即‘jiffies + HZ*定时秒值’，其中常数HZ = 100。本函数的主要功能是设置alarm
// 字段和进行两种单位之间的转换。
int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0) ? (jiffies+HZ*seconds) : 0;
	return (old);
}

// 取当前进程号pid。
int sys_getpid(void)
{
	return current->pid;
}

// 取父进程号ppid。
int sys_getppid(void)
{
	return current->p_pptr->pid;
}

// 取用户号uid。
int sys_getuid(void)
{
	return current->uid;
}

// 取有效的用户号euid。
int sys_geteuid(void)
{
	return current->euid;
}

// 取组号gid。
int sys_getgid(void)
{
	return current->gid;
}

// 取有效的组号egid。
int sys_getegid(void)
{
	return current->egid;
}

// 系统调用功能 -- 降低对CPU的使用优先权（有从会用吗？😏）
// 应该限制increment为大于0的值，否则可使优先权增大！！
int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

// 内核调度程序的初始化子程序。
void sched_init(void)
{
	int i;
	struct desc_struct * p;		// 描述符表结构指针。

// Linux系统开发之初，内核不成熟。内核代码会经常被修改。Linus怕自己无意中修改了这些
// 关键性的数据结构，造成与POSIX标准的不妆容。这里加入下面的这个判断语句并无必要，纯粹
// 是为了提醒自己以及其他修改内核代码的人。
	if (sizeof(struct sigaction) != 16)	// sigaction是存放有信号状态的结构。
		panic("Struct sigaction MUST be 16 bytes");

// 在全局描述符表GDT中设置初始任务（任务0）的任务状态TSS描述符和局部数据表LDT描述符。
// FIST_TSS_ENTRY和FIST_LDT_ENTRY的值分别是4和5，定义在include/linux/sched.h；
// gdt是一个描述符表数组（include/linux/head.h），实际上对应程序head.s中第234行上
// 的全局描述符表基址（_gdt）。因此gdt+FIRST_TSS_ENTRY即为gdt[FIRST_TSS_ENTRY]（
// 即是gdt[4]），也即gdt数组第4项的地址。参见include/asm/system.h，第65行开始。
	set_tss_desc(gdt+FIRST_TSS_ENTRY, &(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY, &(init_task.task.ldt));

// 清任务数组和描述符表项（注意从i=1开始，所以初始任务的描述符还在）。描述符结构
// 定义在文件include/linux/head.h中。
	p = gdt + 2 + FIRST_TSS_ENTRY;
	for (i = 1; i < NR_TASKS; i++){
		task[i] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
// EFLAGS中的NT标志位用于控制任务的嵌套调用。当NT位置位时，姥当中断任务执行
// IRET指令时就会引起任务切换。NT指出TSS中的back_link字段是否有效。NT=0时无效。
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl"); 	// 复位NT标志

// 将任务0的TSS段选择符加载到任务寄存器tr。将局部描述符表段选择符加载到局部描述
// 符表寄存器ldtr中。注意！！是将GDT中相应LDT描述符的选择符加载到ldtr。只明确
// 加载这一次，以后新任务LDT的加载，是CPU根据TSS中的LDT项自动加载。
	ltr(0);			// 定义在include/linux/sched.h 第157-
	lldt(0);		// 其中参数（0）是任务号。

// 下面代码用于初始化8253定时器。通道0，选择工作方式3，二进制计数方式。通道0的
// 输出引脚接在中断控制主芯片的IRQ0上，它每10毫秒发出一个IRQ0请求。LATCH是初始
// 定时计数值。
	outb_p(0x36, 0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff, 0x40);	/* LSB */ // 定时值低字节。
	outb(LATCH >> 8, 0x40);		/* MSB */ // 定时值高字节。

// 设置定时器中断处理程序句柄（设置时钟中断门）。修改中断控制器屏蔽码，允许时钟中断。
// 然后设置系统调用中断门。这两个设置中断描述符表IDT中描述符的宏定义在文件 
// include/asm/system.h中第33、39行处。两者的区别参见system.h文件开始处的说明。
	set_intr_gate(0x20, &timer_interrupt);
	outb(inb_p(0x21)&~0x01, 0x21);		// 修改屏蔽码，允许定时器中断。
	set_system_gate(0x80, &system_call);
}