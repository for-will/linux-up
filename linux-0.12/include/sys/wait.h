#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#define _LOW(v)		((v) & 0377)		/* 取低字节（8进制表示） */
#define _HIGH(v)	( ((v) >> 8) & 0377)	/* 取高字节 */

/* options for waitpid, WUNTRACED not supported */
/* waitpid的选项，其中WUNTRACED未被支持 */
// [注：其实0.12内核已经支持WUNTRACED选项。上面这条注释应该是以前内核版本遗留下来的。]
// 以下常数符号是函数waitpid(pid_t pid, long *stat_addr, int options)中iptions使用的选项。
#define WNOHANG         1			/* 如果没有状也不要挂起，并立刻返回 */
#define WUNTRACED       2			/* 报告停止执行的子进程状态。 */

// 以下宏定义用于判断waitpid()函数返回的状态字（第20、21行的参数*stat_loc）的含义。
#define WIFEXITED(s)	(!((s)&0xFF))		/* 如果子进程正常退出，则为真 */
#define WIFSTOPPED(s)	(((s)&0xFF)==0x7F)	/* 如果子进程下停止着，则为true */
#define WEXITSTATUS(s)	(((s)>>8)&0xFF)		/* 返回退出状态 */
#define WTERMSIG(s)	((s)&0x7F)		/* 返回导致进程终止的信号值（信号量） */
#define WCOREDUMP(s)	((s)&0x80)		/* 判断进程是否执行了内存映像转储（dumpcore） */
#define WSTOPSIG(s)	(((s)>>8)&0xFF)		/* 返回导致进程停止的信号值 */

// 如果由于未捕捉信号而导致子进程退出则为真。
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF)

// wait()和waitpid()函数允许进程获取其子进程之一的状态信息。函数各种选项允许获取已经终止
// 或停止的子进程状态信息。如果存在两个或以上子进程的状态信息，则报告的顺序不是指定的。
// wait()将挂起当前进程，直到其子进程之一退出（终止），或者收到要求终止该进程的信号，
// 或者是需要调用一个信号句柄（信号处理程序）。
// waitpid()挂起当前进程，直到pid指定的子进程退出（终止）或者收到要求终止该进程的信号，
// 或者是需要调用一个信号句柄（信号处理程序）。
// 如果pid = -1，options = 0，则waitpid()的作用与wait()函数一样。否则其行为将随pid和options
// 参数的不同而不同。（参见kernel/exit.c，142）
// 参数pid是进程号；*stat_loc是保存状态信息位置的指针；options是等待选项，见第10，11行。
pid_t wait(int * stat_loc);
pid_t waitpid(pid_t pid, int * stat_loc, int options);

#endif

