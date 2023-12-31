/*
 * linux/init/main.c
 *
 * (C) 1991 Linus Torvalds
 */

// 若*.h头文件在默认目录include/中，则在代码中就不用明确指明其位置。如果不是UNIX类的
// 标准头文件，则需要指明所在的目录，并用双引号括住。unistd.h是标准符号常数与类型文件。
// 其中定义了各种符号常数和类型，并声明了各种函数。如果还定义了符号__LIBRARY__，则还会
// 包含系统调用号和内嵌汇编代码syscall0()等。
#define __LIBRARY__
#include <unistd.h>
#include <time.h>	// 时间类型头文件。其中最主要定义了tm结构和一些有关时间的函数原形。

/*
 * we need this inline - forking from kernel space wll result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
// Linux在内核空间创建进程时不使用写时复制技术（Copy on write）。main()在移动到用户
// 模式（到任务0）后使用内嵌方式的fork()和pause()，因此可保证不使用任务0的用户栈。
// 在执行moveto_user_mode()之后，本程序main()就以任务0的身份在运行了。而任务0是所
// 有将创建子进程的父进程。当它创建一个子进程时（init进程），由于任务1代码也在内核
// 空间，因此没有使用写时复制功能。此时任务0和任务1共同使用同一个用户栈空间。因此
// 希望在任务0环境下运行时不要有对堆栈的任何操作，以免弄乱堆栈。而在再次执行fork()
// 并执行过execve()函数后，被加载程序已不属于内核空间，因此可以使用写时复制技术了。
// 请参见5.3节“Linux内核对内存的管理和使用”。

// 下面_syscall0()是unistd.h中定义的内嵌宏代码。以嵌入汇编的形式调用Linux的系统调用
// 中断0x80。该中断是所有系统调用的入口。该条语句实际上是int fork()创建进程系统调用。
// 可展开看之就会立刻明白。syscall0名称中最后的0表示无参数，1表示1个参数。
// 参见include/unistd.h, 150-161行。
// int pause()系统调用：暂停进程的执行，直到收到一个信号。
// int setup(void * BIOS)系统调用，仅用于linux初始化（仅在这个程序中被调用）。
// int sync()系统调用：更新文件系统。
/* static */
inline _syscall0(int,fork)
/* static */
inline _syscall0(int,pause)
/* static */
static inline _syscall1(int,setup,void *,BIOS)
/* static */
inline _syscall0(int,sync)

#include <linux/tty.h>          // tty头文件，定义了有关tty_io，串行通信方面的参数、常数。
#include <linux/sched.h>        // 调度程序头文件，定义了任务结构 task_struct、第1个被始任务
                                // 的数据。还有一些以宏的形式定义的有关描述符参数设置和获取的
                                // 嵌入汇编函数程序。
#include <linux/head.h>         // head头文件，定义了段描述符的简单结构，和几个选择符常量。
#include <asm/system.h>         // 系统头文件。以宏形式定义了许多有关设置或修改描述符/中断门
                                // 等的嵌入式汇编子程序。
#include <asm/io.h>             // io头文件。以宏的嵌入汇编程序形式定义对io端口操作的函数。

#include <stddef.h>             // 标准定义头文件。定义了NULL，offsetof(TYPE, MEMBER)。
#include <stdarg.h>             // 标准参数头文件。以宏的形式定义变量参数列表。主要说明了一个
                                // 类型(va_list)和三个宏(va_start, va_arg和va_end),vsprintf、
                                // vprintf、vfprintf。
#include <unistd.h>
#include <fcntl.h>              // 文件控制头文件。用于文件及其描述符的操作控制常数符号的定义。
#include <sys/types.h>          // 类型头文件。定义了基本的系统数据类型。

#include <linux/fs.h>           // 文件系统头文件。定义文件表结构（file,buffer_head,m_inode等）。
                                // 其中有定义：extern int ROOT_DEV。
#include <string.h>             // 字符串头文件。主要定义了一些有关内存或字符串操作的嵌入函数。

static char printbuf[1024];             // 表态字符串数组，用作内核显示信息的缓存。

extern char *strcpy();                  // 外部函数或变量，定义在别处。
/* extern int vsprintf();                  // 送格式化输出到字符串中（vsprintf.c,92行）。 */
extern int vsprintf(char * buf, const char * fmt, va_list args);
extern int sprintf(char * str, const char *fmt, ...);
extern void init(void);                 // 函数原形，初始化（本程序168行）。
extern void blk_dev_init(void);         // 块设备初始化程序（blk_drv/ll_rw_blk.c,210行）
extern void chr_dev_init();             // 字符设备初始化（chr_drv/tty_io.c,402行）
extern void hd_init(void);              // 硬盘初始化程序（blk_drv/hd.c,378行）
extern void floppy_init();              // 软驱初始化程序（blk_drv/floppy.c,469行）
extern void mem_init(long start, long end);     // 内存管理初始化（mm/memory.c,443行）
extern long rd_init(long mem_start, int length);// 虚拟盘初始化（blk_drv/ramdisk.c,52行）
extern long kernel_mktime(struct tm * tm);      // 计算开机时间（kernel/mktime.c,41行）

// 内核专用sprintf()函数。该函数用于产生格式化信息并输出到指定缓冲区str中。参数‘*fmt’
// 指定输出将采用的格式，参见标准C语言书籍。该子程序正好是vsprintf如何使用的一个简单
// 例子。函数使用vsprintf()将格式化字符串放入str缓冲区，参见第179行上的printf()函数。
// int sprintf(char * str, const char *fmt, ...)
// {
//         va_list args;
//         int i;

//         va_start(args, fmt);
//         i = vsprintf(str, fmt, args);
//         va_end(args);
//         return i;
// }

/*
 * This is set up by the setup-routine at boot-time
 */
// 下面三行分别交指定的线性地址强行转换为给定类型的指针，并获取指针所指定内容。由于
// 内核代码段被映射到从物理地址零开始的地方，因此这些线性地址正好也是对应的物理地址。
// 这些指定地址处内存值的含义请参见第6章的表6-3（setup程序读取并保存的参数）。
// drive_info结构请参见下面第125行。
#define EXT_MEM_K (*(unsigned short *)0x90002)	       	/* 1MB以后的扩展内存大小（KB） */
#define CON_ROWS ((*(unsigned short *)0x9000e) & 0xff)	/* 选定的控制台屏幕行、列数 */ 
#define CON_COLS (((*(unsigned short *)0x9000e) &0xff00) >> 8)
#define DRIVE_INFO (*(struct drive_info *)0x90080) 	/* 硬盘参数表32字节内容 */ 
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)      /* 根文件系统所在设备号 */
#define ORIG_SWAP_DEV (*(unsigned short *)0x901FA)      /* 交换文件所在设备号 */

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

// 这段宏读取CMOS实时时钟信息。outb_p和inb_p是include/asm/io.h中定义的端口输入输出宏。
// 0x70是写地址端口号，0x71是读数据端口号。0x80|addr是要读取的CMOS内存地址。
// #define CMOS_READ(addr) ({ \		   //
// outb_p(0x80|addr, 0x70); \              // 向端口0x70输出要读取的CMOS内存位置（0x80|addr）。
// inb_p(0x71); \                          // 从端口0x71读取1字节，返回该字节。
// })
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr, 0x70); \
inb_p(0x71); \
})

// 定义宏。将BCD码转换成二进制数值。BCD码利用半个字节（4比特）表示一个10进制数，因此
// 一个字节表示2个10进制数。(val)&15取BCD表示的10进制个位数，而 (val)>>4取BCD表示
// 的10进制十位数，再乘以10.最后两者相加就是一个字节BCD码的实际二进制数值。
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

// 该函数取CMOS实时钟信息作为开机时间，并保存到全局变量startup_time(秒)中。参见后面对
// CMOS内存列表的说明。其中调用的函数kernel_mktime()用于计算从1970年1月1日0时起到
// 开机当日经过的秒数，作为开机时间（kernel/mktime.c 41行）。
static void time_init(void)
{
        struct tm time;                 // 时间结构tm定义在include/time.h中。

// CMOS的访问速度很慢。为了减小时间误差，在读取了下面循环中所有的数值后，若此时 CMOS中
// 秒值发生了变化，那么就重新读取所有值。这样内核就能把与CMOS时间误差控制在1秒之内。
        do {
                time.tm_sec = CMOS_READ(0);     // ovue时间秒值（均是BCD码值）。
                time.tm_min = CMOS_READ(2);     // 当前分钟值。
                time.tm_hour = CMOS_READ(4);    // 当前小时值。
                time.tm_mday = CMOS_READ(7);    // 一月中的当天日期。
                time.tm_mon = CMOS_READ(8);     // 当前月份（1--12）。
                time.tm_year = CMOS_READ(9);    // 当前年份。
        } while (time.tm_sec != CMOS_READ(0));
        BCD_TO_BIN(time.tm_sec);                // 转换成二进制数值。
        BCD_TO_BIN(time.tm_min);
        BCD_TO_BIN(time.tm_hour);
        BCD_TO_BIN(time.tm_mday);
        BCD_TO_BIN(time.tm_mon);
        BCD_TO_BIN(time.tm_year);
        time.tm_mon--;                          // tm_mon中月份范围是0--11。
        startup_time = kernel_mktime(&time);    // 计算开机时间。kernel/mktime.c 41行。
}

// 定义了一些表态变量，仅能被本程序访问。
static long memory_end = 0;                     // 机器具有的物理内存容量（字节数）。
static long buffer_memory_end = 0;              // 高速缓冲区末端地址。
static long main_memory_start = 0;              // 主内存（将用于分页）开始的位置。
static char term[32];                           // 终端设置字符串（环境参数）。

// 读取并执行/etc/rc 文件时所使用的命令行参数和环境参数。
static char * argv_rc[] = { "/bin/sh", NULL };          // 调用执行程序时参数的字符串数组。
static char * envp_rc[] = { "HOME=/", NULL, NULL};      // 调用执行程序时的环境字符串数组。 

// 运行登录 shell 时所使用的命令行参数和环境参数。
// 第122行 argv[0]中的字符“-”是传递给 shell 程序sh的一个标志。通过识别该标志，
// sh程序会作为登录 shell 执行。其执行过程与在 shell 提示符下执行sh不一样。
static char * argv[] = { "-/bin/sh", NULL };
static char * envp[] = { "HOME=/usr/root", NULL, NULL };

struct drive_info { char dummy[32]; } drive_info;       // 用于存放硬盘参数表信息。

// 内核初始化主程序。初始化结束后将以任务 0（idle任务即空闲任务）的身份运行。
// 英文注释含义是“这里确实是void，没错。在startup程序（head.s）中就是这样假设的”。参见
// head.s程序第136行开始的几行代码。
void _main(void)         /* This really IS void, no error here. */
{                        /* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enbable them
 */
// 首先保存根文件系统设备号和交换文件设备号，并根据setup.s程序中获取的信息设置控制台
// 终端屏幕行、列数环境变量 TERM，并用其设置初始init进程中执行etc/rc文件和shell程序
// 使用的环境变量，以及复制内存0x90080处的硬盘参数表（表参见6.3.3节表6-4）。
// 其中ROOT_DEV 已在前面包含进的 include/linux/fs.h文件206行上被声明为extern int,
// 而SWAP_DEV在 include/linux/mm.h 文件内也作了声明。这里mm.h文件并没有显式地列在
// 本程序前部，因为前面包含进的 include/linux/sched.h文件中已经含有它。
        ROOT_DEV = ORIG_ROOT_DEV;               // ROOT_DEV定义在fs/super.c,29行。
        SWAP_DEV = ORIG_SWAP_DEV;               // SWAP_DEV定义在mm/swap.c,36行。
        sprintf(term, "TERM=con%dx%d", CON_COLS, CON_ROWS);
        // ksprintf(term, "TERM=con%dx%d", CON_COLS, CON_ROWS);
        envp[1] = term;
        envp_rc[1] = term;
        drive_info = DRIVE_INFO;                // 复制内存 0x90080处的硬盘参数表。

// 接着根据机器物理内存容量设置调整缓冲区和主内存区的位置和范围。
// 高速缓存末端地址->buffer_memory_end; 机器内存容量->memory_end;
// 主内存开始地址->main_memory_start;
        memory_end = (1<<20) + (EXT_MEM_K<<10); // 内存大小=1MB + 扩展内存（K）*1024字节。
        memory_end &= 0xfffff000;               // 忽略不到4KB(1页)的内存数。
        if (memory_end > 16*1024*1024)          // 如果内存量走过16MB，则按16MB计。
                memory_end = 16*1024*1024;
        if (memory_end > 12*1024*1024)          // 如果内存>12MB,则设置缓冲区末端=4MB
                buffer_memory_end = 4*1024*1024;
        else if (memory_end > 6*1024*1024)      // 否则若内存>6MB,则设置缓冲区末端=2MB
                buffer_memory_end = 2*1024*1024;
        else                                    // 否则则设置缓冲区末端=1MB
                buffer_memory_end = 1*1024*1024;
        main_memory_start = buffer_memory_end;  // 主内存起始位置 = 缓冲区末端。

// 如果在Makefile文件中定义了内存虚拟符号 RAMDISK，则初始化虚拟盘。此时主内存将减少。
// 参见kernel/blk_drv/ramdisk.c。
#ifdef RAMDISK
        main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
// 以下是内核进行所有方面的初始化工作。阅读时最好跟着调用的程序深入进去看、若实在看
// 不下去了，就先放一下，继续看下一个初始化调用 -- 这是经纶之谈。
        mem_init(main_memory_start,memory_end);         // 主内存区初始化。（mm/memory.c:443）
        trap_init();            // 陷阱门（硬件中断向量）初始化。（kernel/trap.c:181）
        blk_dev_init();         // 块设备初始化。（blk_drv/ll_rw_blk.c:210）
        chr_dev_init();         // 字符设备初始化。（chr_drv/tty_io.c:402）
        tty_init();             // tty初始化。（chr_drv/tty_io.c:105）
        time_init();            // 设置开机启动时间。（见第92行）
        sched_init();           // 调度程序初始化（加载任务0的tr，ldtr）（kernel/sched.c:385）
        buffer_init(buffer_memory_end); // 缓冲管理初始化，建内存链表等。（fs/buffer.c:348）
        hd_init();              // 硬盘初始化。（blk_drv/hd.c:378）
        floppy_init();          // 软驱初始化。（blk_drv/floppy.c:469）
        sti();                  // 所有初始化工作都做完了，于是开启中断。

// 下面过程通过在堆栈中设置的参数，利用中断返回指令启动任务0执行。然后在任务0中立刻运行
// fork()创建任务1（又称init进程），并在任务1中执行init()函数。对于被新创建的子进程，
// fork()将返回0值，对于原进程（父进程）则返回子进程的进程号pid。
        move_to_user_mode();    // 移到用户模式下执行。（include/asm/system.h:1）
        if (!fork()) {          /* we count on this going ok */
                init();         // 在新建子进程（任务1）中执行。
        }

// 下面代码是在任务0中运行。
/*
 *  NOTE!!  For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other taks
 * can run). For tak0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 * 注意！！对于任何其他的任务，‘pause()’将意味着我们必等待收到一个信号
 * 才会返回就绪态，但任务0（task0）是唯一例外情况（参见‘schedule()’）,
 * 因为任务0在任何空闲时间里都会被激活（当没胡其他任务在运行时），因此
 * 对于任务0 ‘pause()’仅意味着我们返回来查看是否有其他任务可以运行，如果
 * 没有的话我们就回到这里，一直我不执行‘pause’。
 */
// pause()系统调用会把任务0转换成可中断等待状态，再执行高度函数。但是调度函数只要发现系统
// 中没有其他任务可运行时就会切换回任务0，而不依赖于任务0的状态。参见（kernel/sched.c:144）
        for(;;){
                // printk("pause\n\r");
                __asm__("int $0x80"::"a" (__NR_pause));    // 即执行系统调用pause()。
        }
}

// 下面函数产生格式化信息并输出到标准输出设备stdout(1)上显示。参数‘*fmt’指定输出采用的
// 格式，参见标准C语言书籍。该程序使用vsprintf()将格式化的字符串放入printbuf缓冲区，
// 然后使用write()将缓冲区的内容输出到标准输出输出设备（stdout）上。vsprintf()函数的实现见
// kernel/vsprintf.c。
/* static int printf(const char *fmt, ...) */
int printf(const char *fmt, ...)
{
        va_list args;
        int i;

        va_start(args, fmt);
        write(1,printbuf,i=vsprintf(printbuf, fmt, args));
        va_end(args);
        return i;
}

// init()函数运行在任务0和第1次创建的子进程1（任务1）中。它首先对第一个将要执行的
// 程序（shell）的环境进行初始化，然后以登录shell方式加载该程序并执行之。
void init(void)
{
        int pid,i;

// setup() 是一个系统调用。用于读取硬盘参数包括分区表信息并加载虚拟盘（若存在的话）和
// 安装根文件系统设备。该函数用25行上的宏定义，对应函数是sys_setup(),其实现请参见
// kernel/blk_drv/hd.c:74行。
        setup((void*) &drive_info);

// 下面以读写访问方式打开设备“/dev/tty0”，它对应终端控制台。由于这是第一次打开文件
// 操作，因此产生的文件句柄号（文件描述符）肯定是0。该句柄是UNIX类操作系统默认的控
// 制台标准输入句柄stdin(0)。这里再把它以读和写的方式分别打开是为了复制产生标准输出
// 句柄stdout(1)和标准错误输出句柄stderr(2)。函数前面的“(void)”前缀用于强制函数
// 无需返回值。
        (void) open("/dev/tty1",O_RDWR,0);
        (void) dup(0);                  // 复制句柄，产生句柄1号--stdout标准输出设备。
        (void) dup(0);                  // 复制句柄，产生句柄2号--stderr标准出错输出设备。

// 下面打印缓冲区数（每块104字节）和总字节数，以及主内存区空闲内存字节数。
        printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
                NR_BUFFERS*BLOCK_SIZE);
        printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);

// 下面再创建一个子进程（任务2），并在该子进程中运行/etc/rc文件中的命令。对于被创建的子
// 进程，fork()将返回0值，对于原进程（父进程）则返回子进程的进程号pid。所以第202-206行
// 是子进程中执行的代码。该子进程的代码首先把标准输入stdint重定向到/etc/rc文件，然后使用
// execve()函数运行/bin/sh程序。该程序从标准输入中读取rc文件中命令，并以解释方式执行
// 之。sh运行时所携带的参数和环境变量分别由argv_rc和envp_rc数组给出。
// 关闭句柄0并立刻打开/etc/rc文件的作用是把标准输入stdin重新定向到/etc/rc文件。这样通过
// 控制台读操作就可以读取/etc/rc文件中的内容。由于这里sh的运行方式是非交互式的。因此在
// 执行完rc文件后就会立刻退出，进程2也会随之结束。关于execve()函数说明请参见fs/exec.c
// 程序，207行。函数_exit()退出时的出错码1-操作未许可；2-文件或目录不存在。
        if(!(pid=fork())) {
                close(0);
                if (open("/etc/rc",O_RDONLY,0))
                        _exit(1);       // 若打开文件失败，则退出（lib/_exit.c:10）。
                execve("/bin/sh",argv_rc,envp_rc);      // 替换成/bin/sh程序并执行。
                _exit(2);                               // 若execve()执行失败则退出。
        }

// 下面是父进程（1）执行的语句。wait()等待子进程停止或终止，返回值应是子进程号
// (pid)。这三名的作用是父进程等待子进程的结束。&i是存放返回信息的位置。如果wait()
// 返回值不等于子进程号，则继续等待。
        if (pid>0)
                while (pid != wait(&i))
                        /* nothing */;  // 空循环

// 如果执行到这里，说明刚创建的子进程已执行完/etc/rc文件（或文件不存在），因此子进程
// 自动停止或终止。下面循环中会再次创建一个子进程，用于运行登录和控制台shell程序。
// 该新建子进程首先将关闭所有以前还遗留的句柄（stdin, stdout, stderr），新创建一个
// 会话，然后重新打开/dev/tty0作为stdin,并复制生成stdout和stderr。然后再次执行
// bin/sh程序。但这次执行所选用的参数和环境数组是另一套（见上122-123行）。此后父进程
// 再次执行 wait()等待。如果子进程又停止执行（例如用户执行exit命令），则在标准输出上
// 显示出错信息“子进程pid停止运行，返回码是i”，然后继续重试下去···，形成“大”死循环。
        while (1) {
                if ((pid=fork())<0) {
                        printf("Fork failed in init\r\n");
                        continue;
                }
                if (!pid) {                     // 新的子进程。
                        close(0);close(1);close(2);
                        setsid();               // 创建一个新的会话期，见后面说明。
                        (void) open("/dev/tty1",O_RDWR,0);
                        (void) dup(0);
                        (void) dup(0);
                        _exit(execve("/bin/sh",argv,envp));

                }
                while (1)
                        if (pid == wait(&i))
                                break;
                printf("\n\rchild %d died with code %04x\n\r",pid,i);
                sync();                         // 同步操作，刷新缓冲区。
        }
        _exit(0);       /* NOTE! _exit, not exit() */
// _exit()和exit()都用于正常终止一个函数。但_exit()直接是一个sys_exit()系统调用，而
// exit()则通常是普通函数库中的一个函数。它会先执行一些清除操作，例如调用执行各终止处理
// 程序、关闭所有标准IO等。然后调用sys_exit。
}
