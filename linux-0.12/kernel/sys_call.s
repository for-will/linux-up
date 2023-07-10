/*
 * linux/kernel/sys_call.s
 *
 */

 /*
  * system_call.s contians the system-call low-level handling routines.
  * This also contains the timer-interrupt handler, as some of the code is
  * the same. The hd- and floppy-interrupts are also here.
  *
  * NOTE: This code handles signal-recognition, which happens every time
  * after a timer-interrupt and after each system call. Ordinary interrupts
  * don't handle signal-recognition, as that would clutter them up totally
  * unnecessarily.
  *
  * Stack layout in 'ret_from_system_call':
  *
  *     0(%esp) - %eax
  *     4(%esp) - %ebx
  *     8(%esp) - %ecx
  *     C(%esp) - %edx
  *    10(%esp) - original %eax         (-1 if system call)
  *    14(%esp) - %fs
  *    18(%esp) - %es
  *    18(%esp) - %ds
  *    20(%esp) - %eip
  *    24(%esp) - %cs
  *    28(%esp) - %eflags
  *    2C(%esp) - %oldesp
  *    30(%esp) - %oldss
  */
// 上面Linus原注释中一般中断过程是指除了系统调用中断（int 0x80）和时钟中断（int 0x20）
// 以外的其他中断。这些中断会在内核态或用户态随机发生，若在这些中断过程中也处理信号识别
// 的话，就有可能与系统调用中断和时钟中断过程中对信号的识别处理过程冲突，这违反了内核
// 代码非抢占原则。因此系统既无必要在这些“其他”中断中处理信号，也不允许这样做。

SIG_CHID        = 17            // 定义SIG_CHLD信号（子进程停止或结束）。

EAX             = 0x00          // 堆栈中各个寄存器的偏移位置。
EBX             = 0x04
ECX             = 0x08
EDX             = 0x0C
ORIG_EAX        = 0x10          // 如果不是系统调用（是其它中断）时，该值为-1。
FS              = 0x14
ES              = 0x18
DS              = 0x1C
EIP             = 0x20
CS              = 0x24
EFLAGS          = 0x28
OLDESP          = 0x2C          // 当特权级变化时，原堆栈指针也会入栈。
OLDSS           = 0x30

// 为方便在汇编程序中访问数据结构，这里给出了任务和信号数据结构中指定字段在结构中的偏移值。
// 下面这些是任务结构（task_struct）中字段的偏移值，参见 include/linux/sched.h,105行起。
state           = 0     // these are offsets into the tak-struct. 进程状态码。
counter         = 4     // 任务运行时间计数（递减）（滴答数），运行时间片。
priority        = 8     // 运行优先数。任务开始运行时counter=priority，越大则运行时间越长。
signal          = 12    // 是信号位图，每个比特位代表一种信号，信号值=位偏移值+1
sigaction       = 16    // MUST be 16（=len of sigaction） sigaction结构长度必须是16字节。
blocked         = (33*16)       // 受阻塞信号位图的偏移量。

// 以下定义sigaction结构中各字段的偏移值，参见include/signal.h, 第55行开始。
// offsets within sigaction
sa_handler      = 0     // 信号处理过程的句柄（描述符）。
sa_mask         = 4     // 信号屏蔽码。
sa_flags        = 8     // 信号集
sa_restorer     = 12    // 恢复函数指针，参见kernel/signal.c程序说明。

nr_system_calls = 82    // Linux 0.12版内核中的系统调用总数。

ENOSYS          = 38    // 系统调用号出错码。

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */

.globl system_call,sys_fork,timer_interrupt,sys_execve
.globl hd_interrupt,floppy_interrupt,parallel_interrupt
.globl device_not_available,coprocessor_error

// 系统调用号错误时将返回出错码-ENOSYS。
.align 4                        // 内存4字节对齐。
bad_sys_call:
        pushl   $-ENOSYS        // eax中置-ENOSYS。
        // jmp     ret_from_sys_call

// 重新执行调试程序入口。调试程序schedule()在 kernel/sched.c，119行处开始。
// 当调试程序schedule()返回时就从ret_from_sys_call处（107行）继续执行。
.align 4
reschedule:
        // pushl   $ret_from_sys_call      // 将ret_from_sys_call的地址入栈（107行）。
        // jmp     _schedule

.align 4
coprocessor_error:
	nop	# todo


.align 4
device_not_available:
	nop	# todo

.align 4
parallel_interrupt:
	nop	# todo