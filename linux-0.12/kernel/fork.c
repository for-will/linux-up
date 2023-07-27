/* 
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* 
 * 'fork.c' contans the help-routines for the 'fork' system call
 * (see als system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>              // 错误号头文件。包含系统中各种出错号。

#include <linux/sched.h>        // 调度程序头文件。定义了任务结构task_struct、任务0的数据。
#include <linux/kernel.h>       // 内核头文件。含有一些内核常用函数的原形定义。
#include <asm/segment.h>        // 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。
#include <asm/system.h>         // 系统头文件。定义了设置或修改描述符/中断门等的出入式汇编宏。

// 写页面验证。若页面不可写，则复制页面。定义在mm/memory.c第274行开始。
extern void write_verify(unsigned long address);

long last_pid = 0;              // 最新进程号，其值会由get_empyty_process()生成。

//// 进程空间区域写前验证函数。
// 对于80386 CPU，在执行特权级0代码时不会理会用户空间中的页面是否是页保护的。因此
// 在执行内核代码时用户空间中数据页面保护标志起不了作用，定时复制机制也就失去了作用。
// verify_area()函数正是用于解决这个问题。但对于80486或后来的CPU，其控制寄存器CR0中
// 有一个写保护标志WP（位16），内核可以通过设置该标志来禁止特权级0的代码向用户空间只
// 读页面执行写数据。从而486以上CPU可以通过设置该椟来达到使用本函数同样的目的。
// 
// 该函数对当前进程逻辑地址从 addr 到 addr + size 这一段范围执行写操作前的检测操作。
// 由于检测是以页面为单位进行操作，因此程序首先需要找出addr所在页面开始地址start，
// 然后 start 加上进程数据段基址，使这个start变换成CPU 4G线性空间中的地址。最后循环
// 调用write_verify()对指定大小的内存空间进行写前验证。若页面是只读的，则执行共享检
// 验和复制页面操作（写时复制）。
void verify_area(void * addr, int size)
{
        unsigned long start;

// 首先将起始地址start高速为所在页面开始位置，同时相应地调整验证区域大小size。
// 下句中的start & 0xfff 用来获得指定起始位置addr在页面中的偏移值，原验证范围size
// 加上这个偏移值即扩展成以页面起始位置开始的范围值。因此在30行上也需要把验证开始位置
// start调整成页面边界值。参见前面的图8-13所示。
        start = (unsigned long) addr;
        size += start & 0xfff;
        start &= 0xfffff000;            // 此时start是当前进程空间中的逻辑地址。
// 下面加上进程数据段基址，就把start变成CPU整个线性地址空间中的地址位置。然后循环
// 进行写页面验证。若页面不可写，则复制页面。（mm/memory.c，274行）
        start += get_base(current->ldt[2]);     // include/linux/sched.h，277行。
        while (size > 0) {
                size -= 4096;
                write_verify(start);
                start += 4096;
        }
}

// 复制内存页表。
// 参数nr是新任务号；p是新任务数据结构指针。该函数为新任务在线性地址空间中设置代码
// 段和数据段基址、限长，并复制页表。  由于Linux系统采用了写时复制（copy on write）
// 技术，因此这里仅为新进程设置自己的页目录表项和页表项，并没有为新进程分配实际物理
// 内存页面。此时亲进程与其父进程共享所有内存页面。操作成功返回0，否则返回出错号。
int copy_mem(int nr, struct task_struct * p)
{
        unsigned long old_data_base,new_data_base,data_limit;
        unsigned long old_code_base,new_code_base,code_limit;

// 首先取当前进程局部描述符表中代码描述符和数据段描述符项中的段限长（字节数）。
// 0x0f是代码段我把符；0x17是数据段选择符。然后取当前进程代码段和数据段在线性地址
// 空间中的基地址。由于Linux 0.12内核还不支持代码段和数据段分立的情况，因此这里需要
// 检查代码段和数据段基地址是否都相同，并且要求数据段的长度至少不小于代码段的长度
// （参见图5-12），否则内核显示出错信息，并停止运行。
// get_limit()和get_base()定义在include/linux/sched.h第277行和279行处。
        code_limit = get_limit(0x0f);
        data_limit = get_limit(0x17);
        old_code_base = get_base(current->ldt[1]);
        old_data_base = get_base(current->ldt[2]);
        if (old_data_base != old_code_base)
                panic("We don't support separate I&D");
        if (data_limit < code_limit)
                panic("Bad data limit");
// 然后设置新建进程在线性地址空间中的基地址（等于64MB * 其任务号），并用该值设置新
// 进程LDT中段描述符中的基地址字段值。接着设置新进程的页目录表项和页表项，即复制
// 当前进程（父进程）的页目录表和页表项。  此时子进程共享父进程的内存页面。正常情
// 况下 copy_page_tables()返回0。则表示出错，则释放刚申请的页表项。
        new_data_base = new_code_base = nr * TASK_SIZE;
        p->start_code = new_code_base;
        set_base(p->ldt[1], new_code_base);
        set_base(p->ldt[2], new_data_base);
        if (copy_page_tables(old_data_base, new_data_base, data_limit)) {
                free_page_tables(new_data_base, data_limit);
                return -ENOMEM;
        }
        return 0;
}

/* 
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segmetn in it's entirety.
 */
// 复制进程信息。
// 该函数的参数是从进入系统调用中断处理过程开始，直到调用本系统调用sys_fork的处理过程
// （sys_call.s第222行）和调用本函数前（第231行）逐步压入进程内核态栈的各寄存器的值。
// 这些在sys_call.s程序中入栈的值（参数）包括：
// ① CPU执行中断指令压入用户栈地址ss和esp、标志eflags和返回地址cs和eip；
// ② 第85--91行在刚进入system_call时入栈的段寄存器ds、es、fs和edx、ecx、ebx；
// ③ 第97行上调用sys_call_table中sys_fork函数时入栈的返回地址（参数none表示）；
// ④ 第226--230行在调用copy_process()之前入栈的gs、esi、edi、ebp、和eax（nr）。
// 其中参数nr是调用find_empty_process()分配的任务数组项号。
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
                long ebx, long ecx, long edx, long orig_eax,
                long fs, long es, long ds,
                long eip, long cs, long eflags, long esp, long ss)
{
        struct task_struct *p;
        int i;
        struct file *f;

// 首先为新任务数据结构分配内存（如果分配出错，则返回出错码并退出）。然后将新任务
// 结构指针放入任务数组的nr项中。其中nr为任务号，它由前面find_empty_process()返回。
// 接着把当前进程任务结构内容复制到刚申请到的内存页面p开始处。
        p = (struct task_struct *) get_free_page();
        if (!p)
                return -EAGAIN;
        task[nr] = p;
        *p = *current;  /* NOTE! this doesn't copy the supervisor stack */
// 随后对复制来的进程结构内容进行一些修改，作为新进程的任务结构。先将新进程的状态
// 置为不可中断等待状态，以防止内核调度其执行。然后设置新进程的进程号pid，并初始
// 化进程运行时间片值等于priority值（一般为15个嘀嗒）。接着复位新进程的信号
// 位图、报警定时值、会话（session）领导标志leader、进程及其子进程在内核和用户
// 态运行时间统计值，还设置进程开始运行的系统时间start_time。请参见5.7节内容。
        p->state = TASK_UNINTERRUPTIBLE;
        p->pid = last_pid;              // 新进程号。也由find_empty_process()得到。
        p->counter = p->priority;       // 运行时间片值（嘀嗒数）。
        p->signal = 0;                  // 信号位图。
        p->alarm = 0;                   // 报警定时值（嘀嗒数）。
        p->leader = 0;                  /* process leadership doesn't inherit */
        p->utime = p->stime = 0;        // 用户态时间和核心态运行时间。
        p->cutime = p->cstime = 0;      // 子进程用户态和核心态运行时间。
        p->start_time = jiffies;        // 进程开始运行时间（当前时间滴答数）。

// 再修改任务状态段TSS内容（参见列表后说明）。由于系统给任务结构p分配了1页新
// 内存，所以（PAGE_SIZE + (long) p）让esp0正好指向该页项端。 ss0:esp0用作程序
// 在内核态执行时的栈。另外，在第5章中我们已经知道，每个任务在GDT表中都有两个
// 段描述符，一个是任务的TSS段描述符，另一个是任务的LDT表段描述符。下面110行
// 语句就是把GDT中本任务LDT段描述符的选择符保存在本任务的TSS段中。当执行任务
// 切换时，CPU会自动从TSS中把LDT段描述符的选择符加载到ldtr寄存器中。
        p->tss.back_link = 0;
        p->tss.esp0 = PAGE_SIZE + (long) p;     // 任务内核态栈指针。
        p->tss.ss0 = 0x10;                      // 内核态栈的段选择符（与内核数据段相同）。
        p->tss.eip = eip;                       // 指令代码指针。
        p->tss.eflags = eflags;                 // 标志寄存器。
        p->tss.eax = 0;                         // 这是当fork()返回时新进程会返回0的原因所在。
        p->tss.ecx = ecx;
        p->tss.edx = edx;
        p->tss.ebx = ebx;
        p->tss.esp = esp;
        p->tss.ebp = ebp;
        p->tss.esi = esi;
        p->tss.edi = edi;
        p->tss.es = es & 0xffff;                // 段寄存器仅16位有效。
        p->tss.cs = cs & 0xffff;
        p->tss.ss = ss & 0xffff;
        p->tss.ds = ds & 0xffff;
        p->tss.fs = fs & 0xffff;
        p->tss.gs = gs & 0xffff;
        p->tss.ldt = _LDT(nr);                  // 任务LDT描述符的选择符（LDT描述符在GDT中）
        p->tss.trace_bitmap = 0x80000000;       // （高16位有效）。
// 如果当前任务使用了协处理器，就保存其上下文。指令CLTS用于清除控制寄存器CR0
// 中的任务已交换（TS）标志。每当发生任务切换，CPU都会设置该标志。该标志用于管理
// 数学协处理器：如果该标志置位，那么每个ESC指令都会被捕获（异常7）。如果协处理
// 器存在标志MP也同时置位的话，那么WAIT指令也会捕获。因此，如果任务切换发生在一
// 个ESC指令开始执行之后，则协处理器的内容就可能需要在执行新的ESC指令之前保存
// 起来。捕获处理句柄会保存协处理器的内容并复位TS标志。指令fnsave用于把协处理器
// 的所有状态保存到目的操作数指定的内存区域中（rss.i387）。
        if (last_task_used_math == current)
                __asm__("clts ; fnsave %0 ; frstor %0"::"m" (p->tss.i387));

// 接下来复制进程页表。即在线性地址空间中设置新任务代码段和数据段描述符中的基址
// 和限长，并复制页表。如果出错（返回值不是0），则复位任务数组中相应项并释放为
// 该新任务分配的用于任务结构的内存页。
        if (copy_mem(nr, p)) {          // 返回不为0表示出错。
                task[nr] = NULL;
                free_page((long) p);
                return -EAGAIN;
        }
// 因为新创建的子进程与父进程共享打开着的文件，所以父进程若有打开着的文件，则需将对应
// 文件的打开次数增1。同样道理，也需要将当前进程（父进程）的pwd、root和executable
// 这些i节点的引用次数都增1。
        for (i = 0; i < NR_OPEN; i++)
                if (f = p->filp[i])
                        f->f_count++;
        if (current->pwd)
                current->pwd->i_count++;
        if (current->root)
                current->root->i_count++;
        if (current->executable)
                current->executable->i_count++;
        if (current->library)
                current->library->i_count++;
// 随后在GDT表中设置新任务TSS段和LDT段描述符项。这两个段的限长均被设置成104
// 字节。参见 include/asm/system.h，52--66行代码。然后设置进程之间的关系链表
// 指针，即把新进程插入到当前进程的子进程链表中。把新进程的父进程设置为当前进程，
// 把新进程的最新子进程指针p_cptr和年轻兄弟进程指针p_ysptr置空。接着让新进程
// 的老兄进程指针p_osptr设置等于父进程的最新子进程指针。若当前进程确实还有其他
// 子进程，则让比邻老兄进程的最年轻进程指针p_ysptr指向新进程。最后把当前进程
// 的最新子进程指针指向这个新进程。然后把新进程设置成就绪态。最后返回新进程号。
// 另外，set_tss_desc()和set_ldt_desc()定义在include/asm/system.h文件中。
// “gdt+(nr<<1)+FIRST_TSS_ENTRY”是任务nr的TSS描述符项在全局表中的地址。因为
// 每个任务占用GDT表中2项，因此上式中要包括‘(nr<<1)’。
// 请注意，在任务切换时，任务寄存器TR会由CPU自动加载。
        set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY, &(p->tss));
        set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY, &(p->ldt));
        p->p_pptr = current;                    // 设置新进程的父进程指针
        p->p_cptr = 0;                          // 复位新进程的最新子进程指针。
        p->p_ysptr = 0;                         // 复位新进程的比邻年轻兄弟进程指针。
        p->p_osptr = current->p_cptr;           // 设置新进程的比邻老兄兄弟进程指针。
        if (p->p_osptr)                         // 若新进程有老兄兄弟进程，则让其
                p->p_osptr->p_ysptr = p;        // 年轻进程兄弟指针指向新进程。
        current->p_cptr = p;                    // 让当前进程最新子进程指针指向新进程。
        p->state = TASK_RUNNING;                /* do this last, just in case */
        return last_pid;
}

// 为新进程取得不重复的进程号last_pid。函数返回在任务数组中的任务号（数组项）。
int find_empty_process(void)
{
        int i;

// 首先获取新的进程号。如果last_pid增1后超出进程号的正常数表示范围，则重新从1开始
// 使用pid号。然后在任务数组中搜索刚设置的pid号是否已经被任何任务使用。如果是则
// 跳转到函数开始处重新获得一个pid号。接着在任务数组中为新任务寻找一个空闲项，并
// 返回项号。 last_pid是一个全局变量，不用返回。如果此时任务数组中64个项全已经被全
// 部占用，则返回出错码。
repeat:
        if ((++last_pid) < 0) last_pid = 1;
        for (i = 0; i < NR_TASKS; i++)
                if (task[i] && ((task[i]->pid == last_pid) ||
                                (task[i]->pgrp == last_pid)))
                        goto repeat;
        for (i = 1; i < NR_TASKS; i++)          // 任务0项被排除在外。
                if (!task[i])
                        return i;
        return -EAGAIN;       
}