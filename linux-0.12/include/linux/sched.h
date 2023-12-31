#ifndef _SCHED_H
#define _SCHED_H

#define HZ 100			/* 定义系统时钟滴答频率（1百赫兹，每个滴答10ms） */

#define NR_TASKS	64		/* 系统中同时最多任务（进程）数 */
#define TASK_SIZE	0x04000000	/* 每个任务的长度（64MB） */
#define LIBRARY_SIZE	0x00400000	/* 动态加载库长度（4MB） */

#if (TASK_SIZE &x3fffff)
#error "TASK_ZIE must be multiple of 4M"	/* 任务长度必须是4MB的倍数 */
#endif

#if (LIBRARY_SIZE & 0x3fffff)
#error "LIBARRY_SIZE must be a multiple of 4M"	/* 库长度也必须是4MB的倍数 */
#endif

#if (LIBRARY_SIZE >= (TASK_SIZE/2))
#error "LIBRARY_ZIE too damn big!"		/* 加载库的长度不得大于任务长度的一半 */
#endif

// 在进程逻辑地址空间中动态库被加载的位置（60MB处）。
#define LIBRARY_OFFSET (TASK_SIZE - LIBRARY_SIZE)

// 下面宏CT_TO_SECS和CT_TO_USECS用于把系统当前滴答数转换成用秒值加微秒值表示。
#define CT_TO_SECS(x)	((x) / HZ)
#define CT_TO_USECS(x)	(((x) % HZ) * 1000000/HZ)

#define FIRST_TASK task[0]			/* 任务0比较特殊，所以特意给它单独定义一个符号 */
#define LAST_TASK task[NR_TASKS-1]		/* 任务数组中的最后一项任务 */

#include <linux/head.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-execf-flags and select masks are in one long, max 32 file/proc"
#endif

// 这里字义了进程运行时可能处的状态。
#define TASK_RUNNING		0		/* 进程正在运行或已准备就绪 */
#define TASK_INTERRUPTIBLE	1		/* 进程处于可中断等待状态 */
#define TASK_UNINTERRUPTIBLE	2		/* 进程处于不可中断等待状态，主要用于I/O操作等待 */
#define TASK_ZOMBIE		3		/* 进程处于僵死状态，已经停止运行，但父进程还没发信号 */
#define TASK_STOPPED		4		/* 进程已停止 */

#ifndef NULL
#define NULL ((void *) 0)
#endif

// 复制进程的页目录页表。Linus认为这是内核中最复杂的函数之一。（mm/memory.c，118）
extern int copy_page_tables(unsigned long from, unsigned long to, long size);

// 释放页表所指定的内存块及页表本身。（mm/memory.c，69）
extern int free_page_tables(unsigned long from, unsigned long size);

// 调度程序的初始化函数。（kernel/sched.c，417）
extern void sched_init(void);

// 进程调度函数。（kernel/sched.c，119）
extern void schedule(void);

// 异常（陷阱）中断处理初始化函数，设置中断调用门并允许中断请求信号。（kernel/traps.c，185）
extern void trap_init(void);

// 显示内核出错信息，然后进入死循环。（kernel/panic.c，16）
extern void panic(const char * str);

// 往tty上写指定长度的字符串。（kernel/chr_drv/tty_io.c，339）
extern int tty_write(unsigned minor, char * buf, int count);

typedef int (*fn_ptr)();			// 字义函数指针类型。

// 下面是数学协处理器使用的结构，主要用于保存进程切换时i387的执行状态信息。
struct i387_struct {
	long 	cwd;		// 控制字（Control word）。
	long	swd;		// 状态字（Status word）。
	long 	twd;		// 标记字（Tag word）。
	long 	fip;		// 协处理器代码指针。
	long 	fcs;		// 协处理器代码段寄存器。
	long 	foo;		// 内存操作数的偏移位置。
	long 	fos;		// 内存操作数据的段值。
	long 	st_pace[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
				/* 8个10字节的协处理器累加器 */
};

// 任务状态段数据结构。
struct tss_struct {
	long 	back_link;	/* 16 high bits zero */
	long 	esp0;
	long 	ss0;		/* 16 high bits zero */
	long 	esp1;
	long 	ss1;		/* 16 high bits zero */
	long	esp2;
	long 	ss2;		/* 16 high bits zero */
	long 	cr3;
	long 	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long 	esp;
	long 	ebp;
	long	esi;
	long 	edi;
	long 	es;		/* 16 high bits zero */
	long 	cs;		/* 16 high bits zero */
	long 	ss;		/* 16 high bits zero */
	long 	ds;		/* 16 high bits zero */
	long 	fs;		/* 16 high bits zero */
	long 	gs;		/* 16 high bits zero */
	long 	ldt;		/* 16 high bits zero */
	long 	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

// 下面是任务（进程）数据结构，或称为进程描述符。详细说明请参见5.7节内容。
// long state			任务的运行状态（-1不可运行，0可运行（就绪），>0已停止）。
// long counter			任务运行时间计数（递减）（滴答数），运行时间片。
// long priority		优先数。任务开始运行时counter=priority，越大运行越长。
// long signal			信号位图，每个比特位代表一种信号，信号值=位偏移值+1.
// struct sigaction sigaction[32] 信号执行属性结构，对应信号将要执行的操作和标志信息。
// long blocketd		进程信号屏蔽码（对应信号位图）。
// -----------------------------
// int exit_code		任务执行停止的退出码，其父进程会取。
// unsigned long start_code	代码段地址。
// unsigned long end_code	代码长度（字节数）。
// unsigned long end_data	代码长度 + 数据长度（字节数）。
// unsigned long brk		总长度（字节数）。
// unsigned long start_stack	堆栈段地址。
// long pid			进程标识号（进程号）。
// long pgrp			进程组号。
// long session			会话号。
// long leader			会话首领。
// int groups[NGROUPS]		进程所属组号。一个进程可属于多个组。
// task_struct *p_pptr		指向父进程的指针。
// task_struct *p_cptr		指向最新子进程的指针。
// task_struct *p_ysptr		指向比自己后创建的相邻进程的指针。
// task_struct *p_osptr		指向比自己早创建的相邻进程的指针。
// unsigned short uid		用户标识号（用户id）。
// unsigned short euid		有效用户id。
// unsigned short suid		保存的用户id。
// unsigned short gid		组标识号（组id）。
// unsigned short egid		有效组id。
// unsigned short sgid		保存的组id。
// long timeout			内核定时超时值。
// long alarm			报警定时值（滴答数）。
// long utime			用户态运行时间（滴答数）。
// long stime			系统态运行时间（滴答数）。
// long cutime			子进程用户态运行时间。
// long cstime			子进程系统态运行时间。
// long start_time		进程开始运行时刻。
// struct rlimit rlim[RLIM_ULIMITS] 进程资源使用统计数组。
// unsigned int flags		各进程的标志，在下面第149行开始定义（还未使用）。
// unsigned short used_math	标志：是否使用了协处理器。
// -----------------------------
// int tty			进程使用tty终端的子设备号。-1表示没有使用。
// unsigned short umask		文件创建属性屏蔽位。
// struct m_inode * pwd		当前工作目录i节点结构指针。
// struct m_inode * root	根目录i节点结构指针。
// struct m_inode * executable	执行文件i节点结构指针。
// struct m_inode * library	被加载库文件i节点结构指针。
// unsigned long close_on_exec	执行时关闭文件句柄位图标志。（参见include/fcntl.h）
// struct desc_struct ldt[3]	局部描述符表。0-空，1-代码段cs，2-数据和堆栈段ds&ss。
// struct tss_struct tss	进程的任务状态段信息结构。
// =============================		
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32];
	long blocked;	/* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,pgrp,session,leader;
	int	groups[NGROUPS];
	/*
	 * pointers to parent process, youngest child, younger sibling,
	 * older sibling, respectively. (p->father can be replaced with
	 * p->p_pptr->pid)
	 */
	struct task_struct	*p_pptr, *p_cptr, *p_ysptr, *p_osptr;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	unsigned long timeout,alarm;
	long utime,stime,cutime,cstime,start_time;
	struct rlimit rlim[RLIM_NLIMITS];
	unsigned int flags;	/* per process flags, defined below */
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	struct m_inode * library;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};

/*
 * Per process flags
 */
/* 每个进程的标志 */
#define PF_ALIGNWARN	0x00000001		/* Print aligment warning msgs */
						/* Not implemented yet, only for 486 */
/* 
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640KB)
 */
/*
 * INIT_TASK用于设置第1个任务表，若想修改，责任自负！
 * 基址Base = 0，段长limit = 0x9ffff（=640KB）。
 */
// 对应上面任务结构的第1个任务的信息。
#define INIT_TASK \
/* state etc */	{ 0,15,15, 		/* state, counter, priority 		*/\
/* signals */	0,{{},},0, 		/* signal, sigaction[32], blocked 	*/\
/* ec,brk... */	0,0,0,0,0,0, 		/* exit_code, start_code, end_code, 	*/\
					/* end_data, brk, start_stack 		*/\
/* pid tec.. */	0,0,0,0, 		/* pid, pgrp, session, leader 		*/\
/* suppl grps*/	{NOGROUP,},		/* groups[] 				*/\
/* proc links*/ &init_task.task,0,0,0,	/* p_pptr, p_cptr, p_ysptr, p_osptr 	*/\
/* uid etc */	0,0,0,0,0,0,		/* uid, euid, suid, gid, egid, sgid 	*/\
/* timeout */	0,0,0,0,0,0,0, 		/* alarm, utime, stime, cutime, 	*/\
					/* cstime, start_time, used_math 	*/\
/* rlimits */	{ {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}, 	\
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}, 	\
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}}, 	\
/* flags */	0, 	/* flags 						*/\
/* math */	0, 	/* used_math 						*/\
		/* tty, umask, pwd, root, executable, close_on_exec 		*/\
/* fs info */	-1,0022,NULL,NULL,NULL,NULL,0, 					  \
/* filp */	{NULL,},/* filp[20] 						*/\
	{ 		/* ldt[3] 						*/\
		{0,0}, 								  \
		/* 代码长640K，基址0x0，G=1，D=1，DPL=3，P=1 TYPE=0xa 		*/\
/* ldt */	{0x9f,0xc0fa00}, 						  \
		/* 代码长640K，基址0x0，G=1，D=1，DPL=3，P=1 TYPE=0x2 		*/\
		{0x9f,0xc0f200}, 						  \
	}, 									  \
/* tss */{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir, /* tss 	*/\
	  0,0,0,0,0,0,0,0, 							  \
	  0,0,0x17,0x17,0x17,0x17,0x17,0x17, 					  \
	  _LDT(0),0x80000000, 							  \
	  	{0} 								  \
	  }, 									  \
}

extern struct  task_struct *task[NR_TASKS];	// 任务指针数组。
extern struct task_struct *last_task_used_math;	// 上一个使用过协处理器的进程。
extern struct task_struct *current;		// 当前运行进程结构指针变量。
extern unsigned long volatile jiffies;		// 从开机开始算起的滴答数（10ms/滴答）。
extern unsigned long startup_time;		// 开机时间。从1970:0:0开始计时的秒数。
extern int jiffies_offset;			// 用于累计需要调整的时间滴答数。

// 当前时间（秒数）。
#define CURRENT_TIME (startup_time+(jiffies+jiffies_offset)/HZ)


// 添加定时器函数（定时时间jiffies滴答数，定时到时调用函数*fn()）。（kernel/sched.c）
extern void add_timer(long jiffies, void (*fn)(void));

// 不可中断的等待睡眠。（kernel/sched.c）
extern void sleep_on(struct task_struct ** p);

// 可中断的等待睡眠。（kernel/sched.c）
extern void interruptible_sleep_on(struct task_struct ** p);

// 明确唤醒睡眠的进程。（kernel/sched.c）
extern void wake_up(struct task_struct ** p);

// 检查当前进程是否在指定的用户组grp中。
extern int in_group_p(gid_t grp);

/*
 * Entry into gdt where to find first TSS. 0-null, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
/*
 * 寻找第1个TSS在全局表中的入口。0-没有用nul，1-代码段cs，2-数据段ds，3-系统段syscall
 * 4-任务状态段TSS0，5-局部表LDT0，6-任务状态段TSS1，等。
 */
// 从该英文注释可以猜想到，Linus当时曾想把系统调用的代码专门放到GDT表中第4个独立的段中。
// 但后来并没有那样做，于是就一起把GDT表跌第4个描述符项（上面syscall项）闲置在一旁。
// 下面定义全局表中第1个任务的状态段（TSS）和局部描述符表（LDT）的描述符的选择符索引号。
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)

// 宏定义，计算在全局表中第n个任务的TSS段描述符的选择符值（偏移量）。
// 因每个描述符占8字节，因此FIRST_TSS_ENTY<<3表示该描述符在GDT表中的起始偏移位置。
// 因为每个任务使用1个TSS和1个LDT描述符，共占用16字节，因此需要n<<4来表示对应
// TSS起始位置。该宏得到的值正好也是该TSS的选择符值。随后的宏定义用于计算在全局表中
// 第n个任务的LDT段描述符的选择符值（偏移量）。
#define _TSS(n) ((((unsigned long) n)<<4) + (FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4) + (FIRST_LDT_ENTRY<<3))

// 下面宏定义用于把第n个任务的TSS段选择符加载到任务寄存器TR中。
// 随后一个宏则用于把第n个任务的LDT段选择符加载到局部描述符表寄存器LDTR中。
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))

// 取当前运行任务的任务号（是任务数组中的索引值，与进程号pid不同）。
// 返回：n - 当前任务号。用于（kernel/traps.c，第78行）。
#define str(n) \
__asm__("str %%ax\n\t" 		/* 将任务寄存器中TSS段的选择符复制到ax中 	*/\
	"subl %2,%%eax\n\t"	/* （eax - FIRST_TSS_ENTRY*8） -> eax 	*/\
	"shrl $4,%%eax"		/* （eax/16）-> eax = 当前任务号 		*/\
	:"=a" (n) 							  \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))


/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, int which case it does nothing.
 * This also clars the TS-flag if the task we swithed to has used
 * tha math co-processor lates.
 */
/*
 * switch_to(n)将切换当前任务到任务nr，即n。首先稍微i末尾淘汰了n不是当前任务，
 * 如果是则什么也不做退出。如果我们切换到的任务最近（上次运行）使用过数学
 * 协处理器的话，则还需复位控制寄存器cr0中的TS标志。
 */
// 跳转到一个任务的TSS段选择符组成的地址处会造成CPU进行任务切换操作。
// 输入：%0 - 指向__tmp;		%1 - 指向__tmp.b处，用于存放新TSS的选择符；
// 	DX - 新任务n的TSS段选择符；ECX - 新任务n的任务结构指针task[n]。
// 其中临时数据结构__tmp用于组建228行远跳转（FAR JUMP）指令的操作数。访操作数由4字节
// 偏移地址和2字节的段选择符组成。因此__tmp中a的值是32位偏移值，而b的低2字节是新
// TSS段的选择符（高2字节不用）。跳转到TSS段选择符会造成任务切换到该TSS对应的进程。
// 对于造成任务切换的长跳转，a值无用。228行上的间接跳转指令使用6字节操作数作为跳转目
// 的地的长指针，其格式为：JMP 16位段选择符：32位偏移值。
// 任务切换回来之后，在判断原任务上次执行是否使用过协处理器时，是通过将原任务指针与保存
// 在last_task_used_math变量中的上次使用过协处理器任务指针进行比较而作出的，参见文件
// kernel/sched.c中有关math_state_restore()函数的说明。
#define switch_to(n) {				\
struct {long a, b} __tmp; 			\
__asm__("cmpl %%ecx,current\n\t" 	/* 任务n是当前任务吗？（current==task[n]?）	*/\
	"je 1f\n\t"			/* 是，则什么都不做，退出 			*/\
	"movw %%dx,%1\n\t"		/* 将新任务TSS的16位选择符存入__tmp.b中 	*/\
	"xchgl %%ecx,current\n\t"	/* current = task[n]; ecx = 被切换出的任务 */\
	"ljmp %0\n\t"			/* 执行长跳转至*&__tmp，造成任务切换 	*/\
					/* 在任务切换回来后才会继续执行下面的语句 	*/\
	"cmpl %%ecx,last_task_used_math\n\t" 	/* 原任务上次使用过协处理器吗？ 	*/\
	"jne 1f\n\t" 			/* 没有则跳转，退出 			*/\
	"clts\n"			/* 原任务上次使用过协处理器，则清cr0中的任务 	*/\
	"1:"				/* 切换标志TS 				*/\
	::"m" (*&__tmp.a),"m" (*&__tmp.b), 	\
	"d" (_TSS(n)),"c" ((long) task[n])); 	\
__asm__("":::"ecx");	/* ecx被修改过，添加这一句不一定有用，但应该也不会有什么破坏性 */ \
}
// 页面地址对准。（在内核代码中没有任何地方引用！！）
#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

// 设置位于地址addr处描述符中的各基地址字段（基地址是base）。
// %0 - 地址addr偏移2；%1 - 地址addr偏移4；%2 - 地址addr偏移7；edx - 基地址base。
#define _set_base(addr, base) \
__asm__("movw %%dx,%0\n\t" 	/* 基址base低16位（位15-0）-> [addr+2] 	 */\
	"rorl $16,%%edx\n\t"	/* edx中基址高16位（位31->16）-> dx 	 */\
	"movb %%dl,%1\n\t"	/* 基址高16位中的低8位（位23-16）-> [addr+4] */\
	"movb %%dh,%2\n\t"	/* 基址高16位中的高8位（位31-24）-> [addr+7] */\
	::"m" (*((addr)+2)), 	\
	  "m" (*((addr)+4)), 	\
	  "m" (*((addr)+7)), 	\
	  "d" (base) 		\
	:/* "dx" */); 		\
__asm__("":::"dx")
		// 告诉gcc编译器edx寄存器中的值已被嵌入汇编程序改变了。

// 设置位于地址addr处描述符中的段限长字段（段长是limti）。
// %0 - 地址addr；%1 - 地址addr偏移6处；edx - 段长值limit。
#define _set_limit(addr, limit) \
__asm__("movw %%dx,%0\n\t" 	/* 段长limit低16位（位15-0）-> [addr] 	*/\
	"rorl $16,%%edx\n\t"	/* EDX中的段长高4位（位19-16）-> DL 	*/\
	"movb %1,%%dh\n\t"	/* 取原[addr+6]字节->DH，其中高4位是此标志 	*/\
	"andb $0xf0,%%dh\n\t"	/* 清DH的低4位（将存放段长的位19-16） 	*/\
	"orb %%dh,%%dl\n\t"	/* 将原高4位和段长的高4位（位19-16）合成1字节  */\
	"movb %%dl,%1"		/* 并放回[addr+6]处 			*/\
	::"m" (*(addr)), 	\
	  "m" (*((addr)+6)), 	\
	  "d" (limit) 		\
	:/* "dx" */);		\
__asm__("":::"dx")

// 设置局部描述符表中ldt描述符的基地址字段。设置局部描述符表中ldt描述符的段长字段。
#define set_base(ldt, base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt, limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

inline fix_set_base(char * ldt, unsigned long base)
{
	ldt[2] = base&0xff;
	ldt[3] = (base>>8)&0xff;
	ldt[4] = (base>>16)&0xff;
	ldt[7] = (base>>24)&0xff;
}

// HIGH:24~31	-- 基地址24~31 #addr[7]
// HIGH:16-23	-- 基地址16~23 #addr[4]
// LOW :00-15	-- 基地址00~15 #addr[2..3]
// 从地址addr处描述符中取段基地址。功能与_set_base()正好相反。
// EDX - 存放基地址（__base）；%1 - 地址addr偏移2；%2 - 地址addr偏移4；%3 - addr偏移7。
#define _get_base(addr) ({ 	\
unsigned long __base; 		\
__asm__("movb %3,%%dh\n\t" 	/* 取[addr+7]处基址高16位的高8位（位31-24）-> DH */\
	"movb %2,%%dl\n\t"	/* 取[addr+4]处基址高16位的低8位（位23-16）-> DL */\
	"shll $16,%%edx\n\t"	/* 基地址高16位到EDX中高16位处 		*/\
	"movw %1,%%dx"		/* 取[addr+2]处基址低16位（位15-0）-> DX 	*/\
	:"=&d" (__base)		/* 从而EDX中含有32位的段基地址 		*/\
	:"m" (*((addr)+2)), 	\
	 "m" (*((addr)+4)), 	\
	 "m" (*((addr)+7)));	\
__base;})

// 取局部描述符表中ldt所指段描述符中的基地址。
#define get_base(ldt) _get_base(((char *) &(ldt)))

// 取段选择符segment指定的描述符中的段限长值。
// 指令lsl是Load Segment Limit缩写。它从指定段描述符中取出分散的限长比特位拼成完整的
// 段限长放入指定寄存器中。所得的段限长是实际字节数减1，因此这里还需要加1后才返回。
// %0 - 存放段长值（字节数）；%1 - 段选择符segment。
// "r" - 使用任意动态分配的寄存器
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
