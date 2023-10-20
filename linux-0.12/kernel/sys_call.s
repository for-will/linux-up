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
  *    1C(%esp) - %ds
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
        jmp     ret_from_sys_call

// 重新执行调度程序入口。调试程序schedule()在 kernel/sched.c，119行处开始。
// 当调试程序schedule()返回时就从ret_from_sys_call处（107行）继续执行。
.align 4
reschedule:
        pushl   $ret_from_sys_call      // 将ret_from_sys_call的地址入栈（107行）。
        jmp     schedule

#### int 0x80 -- linux系统调用入口点（调用中断 int 0x80，eax中是调用号）。
.align 4
system_call:
        push    %ds             // 保存原段寄存器值。
        push    %es
        push    %fs
        pushl   %eax            // save the orig_eax    保存eax原值

// 一个系统调用最多带有3个参数，也可以不带参数。下面入栈的ebx、ecx和edx中放着系统
// 调用相应C语言函数（见第99行）的调用参数。这几个寄存器入栈的顺序由GNU gcc规定，
// ebx中可存放第1个参数，ecx中存放第2个参数，edx中存放第3个参数。
// 系统调用语句可见头文件 include/unistd.h中第150到200行的系统调用宏。
        pushl   %edx
        pushl   %ecx            // push %ebx,%ecx,%edx as parameters
        pushl   %ebx            // to the system call

// 在保存过段寄存器之后，让ds，es指向内核数据段，而fs指向当前局部数据段，即指向执行本
// 次系统调用的用户程序的数据段。注意，在Linux 0.12中内核给任务分配的代码和数据内存段
// 是重叠的，它们的段基址和段限长相同。参见fork.c程序中copy_mem()函数。
        movl    $0x10, %edx     // set up ds,es to kernel space
        mov     %dx, %ds
        mov     %dx, %es
        movl    $0x17, %edx     // fs points to local data space
        mov     %dx, %fs
        cmpl    NR_syscalls, %eax       // 调用号如果超出范围的话就跳转。
        jae     bad_sys_call

// 下面这句操作数的含义是：调用地址=[_sys_call_table + %eax * 4]。请参见程序后的说明
// 和第3章3.2.3节内容。
// sys_call_table[]是一个指针数组，定义在include/linux/sys.h中，该数组中设置了内核
// 所有82个系统调用C处理函数的地址。
        call    sys_call_table(,%eax,4) // 间接调用指定功能C函数。
        pushl   %eax                    // 把系统调用返回值入栈。

// 下面 101-106行查看当前任务的运行状态。如果不在就绪状态（state不等于0）就去执行调试
// 程序。如果该任务在就绪状态，但是其时间片已经用完（counter=0），则也去执行调试程序。
// 例如当后台进程组中的进程执行控制终端读写操作时，那么默认条件下该后台进程组所有进程
// 会收到SIGTTIN或SIGTTOU信号，导致进程组中所有进程处于停止状态，而当前进程则会立刻
// 返回。
2:
        movl    current, %eax           // 取当前任务（进程）数据结构指针->eax。
        cmpl    $0, state(%eax)         // state
        jne     reschedule
        cmpl    $0, counter(%eax)       // counter
        je      reschedule
// 以下这段代码执行从系统调用的C函数返回后，对信号进行识别处理。其他中断服务程序退出时也
// 将跳转到这里进行处理后才退出中断过程，例如后面131行上的处理器出错中断 int 16。
// 首先判别当前任务是否是初始任务task0，如果是则不必对其进行信号方面的处理，直接返回。
// 109行上的_task对应C程序中的task[]数组，直接引用task相当于引用task[0]。
ret_from_sys_call:
        movl    current, %eax
        cmp     task, %eax              // task[0] cannot have signals
        je      3f                      // 向前（forward）跳转到标号3处退出中断处理。

// 通过对调用程序代码选择符的检查来判断调用程序是否是用户任务。如果不是则直接退出中断
// （因为任务在内核态执行时不可抢占）。否则对任务进行信号识别处理。这里通过比较选择符是
// 否为用户代码段的选择符 0x000f（RPL=3，局部表，代码段）来判断是否为用户任务。如果不是
// 则说明是某个中断服务程序（例如中断 16）跳转到第107行执行到此，于是跳转退出中断程序。
// 另外，如果原堆栈段选择符不为0x17（即原堆栈不在用户段中），也说明本系统调用的调用者
// 不是用户任务，则也退出。
        cmpw    $0x0f, CS(%esp)         // was old code segment supervisor ?
        jne     3f
        cmpw    $0x17, OLDSS(%esp)      // was stack segment = 0x17 ?
        jne     3f

// 下面这段代码（行115-128）用于处理当前任务的信号。首先取当前任务结构中的信号位图（32位，
// 每位代表1种信号），然后用任务结构中的信号阻塞（屏蔽）码，阻塞不允许的信号位，取得数值
// 最小的信号值，再把信号位图中该信号对应的位复位（置0），最后将该信号值作为参数之一调
// 用 do_signal()。do_signal()在（kernel/signal.c,128）中，其参数包括13个入栈的信息。
// 在do_signal()或信号处理函数返回之后，若返回值不为0则再看看是否需要切换进程或继续处理
// 其它信号。
        movl    signal(%eax), %ebx      // 取信号位图->ebx，第1位代表1种信号，共32个信号。
        movl    blocked(%eax), %ecx     // 取阻塞（屏蔽）信号位图->eax。
        notl    %ecx                    // 每位取反。
        andl    %ebx, %ecx              // 获取许可的信号位图。
        bsfl    %ecx, %ecx              // 从低位（位0）开始扫描位图，看是否有1的位，
                                        // 若有，则ecx保留该位的偏移值（即第几位0--31）。
        je      3f                      // 如果没有信号则向前跳转退出。
        btrl    %ecx, %ebx              // 复位该信号（ebx含有原signal位图）。
        movl    %ebx, signal(%eax)      // 重新保存signal位图信息 -> current->signal
        incl    %ecx                    // 将信号调整为从1开始
        pushl   %ecx                    // 信号值入栈作为调用do_signal的参数之一。
        call    do_signal               // 调用C函数信号处理程序（kernel/signal.c，128）
        popl    %ecx                    // 弹出入栈的信号值。
        testl   %eax, %eax              // 测试返回值，若不为0则跳转到前面标号2（101行）处。
        jne     2b                      // see if we need to switch tasks, or do more signals

3:
        popl    %eax                    // eax 中含有第100行入栈的系统调用返回值。
        popl    %ebx
        popl    %ecx
        popl    %edx
        addl    $4, %esp                // skip orig_eax 跳过（丢弃）原eax值。
        pop     %fs
        pop     %es
        pop     %ds
        iret

#### int16 -- 处理器错误中断。  类型：错误；无错误码。
// 这是一个外部的基于硬件的异常。当协处理器检测到自己发生错误时，就会通过ERROR引脚
// 通知CPU。下面代码用于处理协处理器发出的出错信息。并跳转去执行C函数math_error()
// （kernel/math/error.c, 11）。返回后跳转到标号ret_from_sys_call处继续执行。
.align 4
coprocessor_error:
	push    %ds
        push    %es
        push    %fs
        pushl   $-1             // fill in -1 for orig_eax 填-1，表示不是系统调用。
        pushl   %edx
        pushl   %ecx
        pushl   %ebx
        pushl   %eax
        movl    $0x10, %eax     // ds,es置为指向内核数据段。
        mov     %ax, %ds
        mov     %ax, %es
        movl    $0x17, %eax     // fs置为指向局部数据段（出错程序的数据段）。
        mov     %ax, %fs
        pushl   $ret_from_sys_call      // 把下面调用返回的地址入栈。
        jmp     math_error      // 执行math_error()（kernel/math/error.c，11）。

#### int7 -- 设备不存在或协处理器不存在。       类型：错误； 无错误码。
// 如果控制寄存器 CR0 中 EM（模拟）标志置位，则当CPU执行一个协处理器指令时就会引发该
// 中断，这样CPU就可以有机会让这个中断处理程序模拟协处理器指令（181行）。
// CR0的交换标志TS是在CPU执行任务转换时设置的。TS可以用来确定什么时候协处理器中的
// 内容与CPU正在执行的任务不匹配了。当CPU在运行一个协处理器转义指令时发现TS置位时，
// 就会引发该中断。此时就可以保存前一个任务的协处理器内容，并恢复新任务的协处理器执行
// 状态（176行）。参见kernel/sched.c，92行。该中断最后将转移到标号ret_from_sys_call
// 处执行下去（检测并处理信号）。
.align 4
device_not_available:
	push    %ds
        push    %es
        push    %fs
        pushl   $-1             // fill in -1 for orig_eax 填-1，表明不是系统调用。
        pushl   %edx
        pushl   %ecx
        pushl   %ebx
        pushl   %eax
        movl    $0x10, %eax     //ds,es置为指向内核数据段。
        mov     %ax, %ds
        mov     %ax, %es
        movl    $0x17, %eax     // fs置为指向局部数据段（出错程序的数据段）。
        mov     %ax, %fs
// 清CR0中任务已交换标志TS，并取CR0值。基其中协处理器仿真标志EM没有置位，说明不是
// EM引起的中断，则恢复任务协处理器状态。执行C函数 math_state_restore()，并在
// 返回时去执行 ret_from_sys_call 处的代码。
        pushl   $ret_from_sys_call      // 把下面跳转或调用的返回地址入栈。
        clts                    // clear TS so that we can use math
        movl    %cr0, %eax
        testl   $0x4, %eax      // EM (math emulation bit)
        je      math_state_restore      // 执行 math_state_restore()（kernel/sched.c，92行）。
// 若EM标志置位，则去执行数学访真程序math_emulate()。
        pushl   %ebp
        pushl   %esi
        pushl   %edi
        pushl   $0              // temporary storage for ORIG_EIP
        call    math_emulate    // 调用以函数（math/math_emulate.c，476行）。
        addl    $4, %esp        // 丢弃临时存储。
        popl    %edi
        popl    %esi
        popl    %ebp
        ret                     // 这里的ret将跳转到 ret_from_sys_call（107行）。

#### int32 -- （int 0x20）时钟中断处理程序。中断频率设置为100Hz（include/linux/sched.h，4），
// 定时芯片8253/8254是在（kernel/sched.c，438）处初始化的。因此这里 jiffies每10毫秒加1。
// 这段代码将 iffies增1，发送结束中断指令给8259控制器，然后用当前特权级作为参数调用C函数
// do_timer(long CPL)。当调用返回时转去检测并处理信号。
.align 4
timer_interrupt:
        push    %ds             // save ds,es and put kernel data space
        push    %es             // into them. %fs is used by _system_call
        push    %fs             // 保存 ds、es并让其指向内核数据段。fs将用于system_call。
        pushl   $-1             // fill in -1 for orig_eax 填-1，表示不是系统调用。

// 下面我们保存寄存器eax、ecx和edx。这是因为gcc编译器在调用函数时不会保存它们。这里也
// 保存了ebx寄存器，因为在后面ret_from_sys_call中会用到它。
        pushl   %edx            // we save %eax,%ecx,%edx as gcc doesn't
        pushl   %ecx            // save those across function calls. %ebx
        pushl   %ebx            // is saved as we use that in ret_sys_call
        pushl   %eax
        movl    $0x10, %eax     // ds,es置为指向内核数据段。
        mov     %ax, %ds
        mov     %ax, %es
        movl    $0x17, %eax     // fs置为指向局部数据段（程序的数据段）。
        mov     %ax, %fs
        incl    jiffies
// 由于初始化中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断。
        movb    $0x20, %al      // EOI to interrupt controller #1
        outb    %al, $0x20

// 下面从堆栈中取出执行系统调用代码的选择符（CS段寄存器值）中的当前特权级别（0或3）并压入
// 堆栈，作为do_timer的参数。do_timer()函数执行任务切换、计时等工作，在kernel/sched.c,
// 324行实现。
        movl    CS(%esp), %eax
        andl    $3, %eax        // %eax is CPL (0 or 3, 0=supervisor)
        pushl   %eax
        call    do_timer        // 'do_timer(long CPL)' doese eveything from
        addl    $4, %esp        // task switching to accounting ...
        jmp     ret_from_sys_call

#### 这是sys_execve()系统调用。取中断调用程序的代码指针作为参数调用C函数do_execve()。
// do_execve()在fs/exec.c，207行。
.align 4
sys_execve:
        lea     EIP(%esp), %eax // eax 指向堆栈中保存用户程序eip指针处。
        pushl   %eax
        call    do_execve
        addl    $4, %esp        // 丢弃调用时压入栈的EIP值。
        ret

#### sys_fork()调用，用于创建子进程，是system_call功能2.原形在include/linux/sys.h中。
// 首先调用C函数find_empty_process()，取得一个进程号 last_pid。若返回负数则说明目前任务
// 数组已满。否则调用copy_process()复制进程。
.align 4
sys_fork:
        call    find_empty_process      // 为新进程取得进程号 last_pid。（kernel/fork.c，143）。
        testl   %eax, %eax              // 在eax返回进程号。若返回负数则退出。
        js      1f
        push    %gs
        pushl   %esi
        pushl   %edi
        pushl   %ebp
        pushl   %eax
        call    copy_process            // 调用C函数 copy_process()（kernel/fork.c, 68）。
        addl    $20, %esp               // 丢弃这里所有压栈内容
1:      ret

#### int 46 -- (int 0x2E) 硬盘中断处理程序，响应硬件中断请求 IRQ14。
// 当请求的硬盘操作完成或出错就会发出此中断信号。（参见kernel/blk_drv/hd.c）。
// 首先向8259A中断控制从芯片发送结束硬件中断指令（EOI），然后取变量do_hd中的函数指针放入edx
// 寄存器中，并置do_hd为NULL，接着判断edx函数指针是否为空。如果为空，则给edx赋值指向
// unexcpected_hd_interrupt(),用于显示出错信息。随后向8259A主芯片发送EOI指令，并调用edx中
// 指针指向的函数：read_intr()、write_intr()或unexcpected_hd_interrupt()。
hd_interrupt:
        pushl   %eax
        pushl   %ecx
        pushl   %edx
        push    %ds
        push    %es
        push    %fs
        movl    $0x10, %eax             // ds,es置为内核数据段。
        mov     %ax, %ds
        mov     %ax, %es
        movl    $0x17, %eax             // fs置为调用程序的局部数据段。
        mov     %ax, %fs
// 由于初始化中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断。
        movb    $0x20, %al
        outb    %al, $0xA0              // EOI to interrupt controller 送从8259A。
        jmp     1f                      // give port chance to breathe
1:      jmp     1f
// do_hd定义为一个函数指针，将被赋值read_intr()或write_intr()函数地址。放到edx寄存器后
// 就将do_hd指针变量置为NULL。然后测试得到的函数指针，若该指针为空，则赋予该指针指向C
// 函数 unexpected_hd_interupt()，以处理未知硬盘中断。
1:      xorl    %edx, %edx
        movl    %edx, hd_timeout        // hd_timeout置为0。表示控制器已在规定时间内产生了中断。
        xchg    do_hd, %edx
        testl   %edx, %edx
        jne     1f                      // 若空，则让指针指向C函数 unexpected_hd_interrupt()。
        movl    $unexpected_hd_interrupt, %edx
1:      outb    %al, $0x20              // 送8259A主芯片EOI指令（结束硬件中断）。
        call    *%edx                   // "interesting" way of handling intr.
        pop     %fs                     // 上句调用do_hd指向的C函数。
        pop     %es
        pop     %ds
        popl    %edx
        popl    %ecx
        popl    %eax
        iret

#### int38 -- (int 0x26) 软盘驱动器中断处理程序，响应硬件中断请求 IRQ6。
// 其处理过程与上面对硬盘的处理基本一样。（kernel/blk_drv/floppy.c）。
// 首先向8259A中断控制器主芯片发送EOI指令，然后取变量do_floppy中的函数指针放入eax
// 寄存器中，并置do_floppy为NULL，接着判断eax函数指针是否为空。如为空，则给eax赋值指向
// unexpected_floppy_interrupt()，用于显示出错信息。随后调用eax指向的函数：rw_interrupt,
// seek_interrupt,recal_interrupt,reset_interrupt或unexpected_floppy_interrupt。
floppy_interrupt:
        pushl   %eax
        pushl   %ecx
        pushl   %edx
        push    %ds
        push    %es
        push    %fs
        movl    $0x10, %eax             // ds,es置为内核数据段。
        mov     %ax, %ds
        mov     %ax, %es
        movl    $0x17, %eax             // fs置为调用程序的局部数据段。
        mov     %ax, %fs
        movb    $0x20, %al              // 送主8259A中断控制器EOI指令（结束硬件中断）。
        outb    %al, $0x20              // EOI to interrupt controller #1
// do_floppy为一函数指针，将被赋值实际处理C函数指针。该指针在被交换放到eax寄存器后就将
// do_floppy变量置空。然后测试eax中原指针是否为空，若是则使指针指向C函数
// unexpected_floppy_interupt()。
        xorl    %eax, %eax
        xchgl   do_floppy, %eax
        testl   %eax, %eax              // 测试函数指针是否=NULL？
        jne     1f                      // 若空，则使指针指向C函数unexpected_floppy_interrupt()。
        movl    $unexpected_floppy_interrupt, %eax
1:      call    *%eax                   // "interesting" way of hanling intr.
        pop     %fs
        pop     %es
        pop     %ds
        popl    %edx
        popl    %ecx
        popl    %eax
        iret

#### int 39 -- (int 0x27) 并行口中断处理程序，对应硬件中断表示信号 IRQ7。
// 本版本内核还未实现。这里只是发送EOI指令。
parallel_interrupt:
	pushl   %eax
        movb    $0x20, %al
        outb    %al, $0x20
        popl    %eax
        iret
