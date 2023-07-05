; 
; head.s contains the 32-bit startup code.
; 
; NOTE!!! Startup happens at absolute address 0x00000000, which is also where
; the page directory will exist. The startup code will overwritten by
; the page directory. 

section .text
; [global _idt, _gdt, _pg_dir, _tmp_floppy_area]
extern stack_start, _main, printk
global _idt, _gdt, _pg_dir, _tmp_floppy_area, startup_32
_pg_dir:                                        ; 页目录将会存放在这里。

; 再次注意！！这里已经处于32位运行模式，因此这里$0x10现在是一个选择符。这里的移动指令
; 会把相应描述符内容加载进段寄存器。这里$0x10的含义是：
; 请求特权级为0（位0-1=0）、选择全局描述符表（位2=0）、选择表中第2项（位3-15=2）。它正好
; 指向表中的数据段描述符项。
; 下面代码的含义是：设置ds，es，fs，gs为setup.s中构造的内核数据段选择符=0x10（对应全局
; 段描述符表第3项），并将堆栈放置在stack_start指向的user_stack数组区，然后使用本程序
; 后面定义的新中断描述符表和全局段描述表。新全局段描述表中初始
; 内容与setup.s中基本一样，仅段限长从8MB改成了16MB。stack_start定义在kernel/sched.c。
; 它是指向user_stack数组末端的一个长指针。第23行设置这里使用的栈，姑且称为系统栈。但在移动
; 到任务0执行（init/main.c中137行）以后该栈就被用作任务0和任务1共同使用的用户栈了。

startup_32:
        mov     eax, 0x10
        mov     ds, ax
        mov     es, ax
        mov     fs, ax
        mov     gs, ax
        lss     esp, [stack_start]              ; 表示_stack_start->ss:esp,设置系统堆栈
                                                ; stack_start定义在kernel/sched.c,82--87行。
        call    setup_idt                       ; 调用设置中断描述符表子程序
        call    setup_gdt                       ; 调用设置全局描述符表子程序
        mov     eax, 0x10                       ; reload all the segment registers
        mov     ds, ax                          ; after changing gdt. CS was already
        mov     es, ax                          ; reloaded in 'setup_gdt'
        mov     fs, ax
        mov     gs, ax

; 由于段描述符中的段限长从setup.s中的8MB改成了本程序设置的16MB(见setup.s 567-568行
; 和本程序后面的235-236行)，因此这里必须再次对所有寄存器执行重加载操作。另外，通过
; 使用bochs仿真软件跟踪观察，如果不对CS再次执行加载，那么在执行到26行时CS代码段不可
; 见部分中的限长不是8MB。这样看来应该重新加载CS。但是由于setup.s中的内核代码段描述符
; 与本程序中重新设置的代码段描述符仅是段限长不同，其余部分完全一样。所以8MB的段限长在
; 内核初始化阶段不会有问题。另外，在以后内核执行过程中段间跳转指令会重新加载CS，所以这
; 里没有加载px并不会导致以后内核出错。
; 针对该问题，目前内核中就在第25行之后添加了一条长跳转指令：‘ljmp $(__KERNEL_CS), $1f’，
; 跳转到第26行来确保CS确实又被重新加载。

        lss     esp, [stack_start]

; 32-36行用于测试A20地址线是否已经开启。采用的方法是向内存地址 0x00000000处写入任意
; 一个数值，然后看内存地址0x100000（1M）处是否也是这个数值。如果一直相同的话，就一直
; 比较下去，也即死循环、死机。表示地址A20线没有选通，结果内核就不能使用1MB以上内存。
; 
; 33行上的‘1：’是一个局部符号构成的标号。标号由符号后跟一个冒号组成。此时该符号表示活动
; 位置计数（Active location counter）的当前值，并可以作为指令的操作数。局部符号用于帮助
; 编译器和编程人员临时起用一些名称。共有10个局部符号，可在整个程序中重复使用。这些符号
; 名使用名称‘0’、‘1’、...、‘9’来引用。为了定义一个局部符号，需把标号写成‘N：’形式（其中N
; 表示一个数字）。为了引用先前最近定义的这个符号，需要写成‘Nb’。为了引用一个局部标号的
; 下一个定义，需要写成‘Nf’，这里N是10个前向引用之一。上面‘b’表示“向后（backwards）”，
; ‘f’表示“向前（forwards）”。在汇编程序的某一处，我们最大可以向后/向前引用10个标号
; （最远第10个）。

        xor     eax, eax
.1:     inc     eax
        mov     [0x000000], eax         ; check that A20 really IS enabled
        cmp     [0x100000], eax         ; loop forever if it isn't
        je      .1

; NOTE!486 should set bit 16, to check for write-protect in supervisor
; mode. Then it would be unnecessary with the "verify_area()"-calls.
; 486 users probably want to set the NE (#5) bit also, so as to use
; int 16 for math errors.
; 
; 上面原注释中提到的 486 CPU 中 CR0 控制寄存器的位16是写保护标志WP(Write-Protect),
; 用于禁止超级用户级的程序向一般用户只读页面中进行写操作。该标志主要用于操作系统在创建
; 新进程时实现写时复制（copy-on-write）方法。
; 下面这段程序（43-65）用于检查数数协处理器芯片是否存在。方法是修改控制寄存器CR0，在
; 假设存在协处理器的情况下执行一个协处理器指令，如果出错的话则说明协处理器芯片不存在，
; 需要设置CR0中的协处理器仿真位EM（位2），并复位协处理器存在标志位MP（位1）。
        
        mov     eax, cr0                ; check math chip
        and     eax, 0x80000011         ; Save PG,PE,ET
; "orl $0x10020, %eax" here for 486 might be good
        or      eax, 2                  ; set MP
        mov     cr0, eax
        call    check_x87
        jmp     after_page_tables       ; 跳转到135行

; We depend on ET to be correct. This checks for 287/387.
; 我们依赖于ET标志的正确性来检测287/387存在与否。

; 下面fninit和fstsw是数学协处理器（80287/80387）的指令。
; fninit 向协处理器发出初始化命令，它会把协处理器置于一个未受以前操作影响的已知状态，设置
; 其控制字为默认值、清除状态字和所有浮点栈式寄存器。非等待形式的这条指令（fninit）还会让
; 协处理器终止执行当前正在执行的任何先前的算术操作。fstsw指令取协处理器的状态字。如果系
; 统中存在协处理器的话，那么在执行了fninit指令后其状态字低字节肯定为0。

check_x87:
        fninit                          ; 向协处理器发出初始化命令。
        fstsw   ax                      ; 取协处理器状态字到ax寄存器中。
        cmp     al, 0                   ; 初始化后状态字应该为0，否则说明协处理器不存在。
        je      .1                      ; no coprocessor: have to set bits
        mov     eax, cr0                ; 如果存在则向前跳转到标号1处，否则改写cr0。
        xor     eax, 6                  ; reset MP, set EM
        mov     cr0, eax
        ret

; .align 是一汇编指示符。其含义是指存储边界对齐调整。“2”表示把随后的代码或数据的偏移位置
; 调整到地址值最后2比特位为零的位置（2^2），即按4字节方式对齐内存地址。不过现在 GNU as
; 直接是写出对齐的值而非2的次方值了。使用该指示符的目的是为了提高32位CPU访问内存中代码
; 或数据的速度和效率。
; 下面的两字节值是80287协处理器指令fsetpm的机器码。其作用是把80287设置为保护模式。
; 80387无需该指令，并且会把该指令看作是空操作。

; .lignn 2
align 4
; 1:    .byte 0xDB, 0xE4                ; fsetpm for 287, ignored by 387
.1:     fsetpm                          ; 287 协处理器码
        ret

;
; setup_idt
;
; set up a idt with 256 entries pointing to 
; ignore_int, interrupt gates. It then loads
; idt. Everything that wants to install itself
; in the idt-table may do so themselves. Interrupts
; are enabled elsewhere, when we can be relatively
; sure everything is ok. This routine will be over-
; written by the page tables.

; 中断描述符表中的项虽然也是8字节组成，但其格式与全局表中的不同，被称为门描述符
; (Gate Descriptor)。它的0-1，6-7字节是偏移量，2-3字节是选择符，4-5字节是一些标志。
; 这段代码首先在 edx、eax中组合设置出8字节默认的中断描述符值，然后在idt表每一项中
; 都放置该描述符，共256项。eax含有描述符低4字节，edx含有高4字节。内核在随后的初始
; 化过程中会替换安装那些真正实用的中断描述符项。

setup_idt:
        lea     edx, [ignore_int]       ; 将ignore_int的有效地址（偏移值）值 -> edx寄存器
        mov     eax, 0x00080000         ; 将选择符0x0008置入eax的高16位中。
        mov     dx, ax                  ; selector = 0x0008 = cs
                                        ; 偏移值的低16位置入eax的低16位中。此时eax含有
                                        ; 门描述符低4字节的值。
        mov     dx, 0x8E00              ; interrupt gate - dpl=0,present
                                        ; 此时edx含有门描述符高4字节的值。
        lea     edi, [_idt]             ; _idt是中断描述符的地址。
        mov     ecx, 256
rp_sidt:
        mov     [edi], eax              ; 将哑中断门描述符存入表中。
        mov     [edi+4], edx            ; eax内容放到 edi+4 所指向内存位置处。
        add     edi, 8                  ; edi指向表中下一项。
        dec     ecx
        jne     rp_sidt
        lidt    [idt_descr]             ; 加载中断描述符表寄存器值。
        ret

; 
; setup_gdt
;
; This routines sets up a new gdt and loads it.
; Only two entries are currently buit, the same
; ones that were build in init.s. The routine
; is VERY complicated at two whole lines, so this
; rather long comment is certainly needed :-).
; This routine will beoverwritten by the page tables.
setup_gdt:
        lgdt    [gdt_descr]             ; 加载全局描述符表寄存器
        ret

; I put the kernel page tables right after the page directory,
; using 4 of them to span 16 Mb of physical memory. People with
; more than 16MB will havt to expand this.
; Linus将内核的内存表直接放在页目录之后，使用了4个表来寻址16MB的物理内存。
; 如果你有多玩16MB的内存，就需要在这里进行扩充修改。

; 每个页表长度为4KB（一页内存页面），而每个页表项需要4个字节，因此一个页表共可以存放
; 1024个表项。如果一个页表项寻址4KB的地址空间，则一个页表就可以寻址4MB的物理内存。
; 页表项的格式为：项的前0-11位存放一些标志，例如是否在内存中（P位0）、读写许可（R/W位1）、
; 普通用户不是超级用户使用（U/S位2）、是否修改过（是否脏了）（D位6）等；表项的位12-31是
; 页框地址，用于指出一页内存的物理起始地址。

times 0x1000-($-$$) db 0
pg0:

times 0x2000-($-$$) db 0
pg1:

times 0x3000-($-$$) db 0
pg2:

times 0x4000-($-$$) db 0
pg3:

times 0x5000-($-$$) db 0        ; 定义下面的内存数据块从0x5000处开始
;
; tmp_floppy_area is used by the ploppy-driver when DMA cannot
; reach to a buffer-block. It needs to be aligned, so that it isn't
; on a 64KB border.
; 当DMA（直接存储器访问）不能访问缓冲块时，下面的tmp_floppy_area内存块
; 就可以供轮船驱动程序使用。其地址需要对齐调整，这样就不会跨越64KB边界。
_tmp_floppy_area:
        times 1024 db 0         ; 共保留1024项，每项1字节，填充数值0.

; 下面这几个入栈操作用于为跳转到init/main.c中的 main() 函数作准备工作。第139行上的指令
; 在栈中压入了返回地址（标号L6），而第140行则压入了main()函数代码的地址。当head.s最后
; 在第218行执行ret指令时就会弹出main()函数的地址，并把控制权转移到 init/main.c程序中。
; 参见第3章中有关C函数调用机制的说明。
; 前面3个入栈0值分别表示main函数的参数envp、argv指针和argc，但main()没有用到。
; 139行的入栈操作是模拟调用main程序时将返回地址入栈的操作，操心如果main.c程序
; 真的退出时，就会返回到这里的标号L6外继续执行下去，也即死循环。140行将main.c的地址
; 压入堆栈，这样，在设置分布处理（setup_paging）结束后执行‘ret’返回指令时就会将main.c
; 程序的地址弹出堆栈，并去执行main.c程序了。
after_page_tables:
        push    0                       ; These are the parameters to main :-)
        push    0                       ; 这些是调用main程序的参数（指init/main.c）。
        push    0                       ; 其中的‘$’符号表示这是一个立即操作数。
        push    L6                      ; return address for main, if it decides to.
        push    _main                   ; '_main'是编译程序时对main的内部表示方法。
        jmp     setup_paging
L6:
        jmp     L6                      ; main should never return here, but
                                        ; just in case, we know what happens.

; This is the default interrupt "handler" :-)
int_msg:
        db "Unknown interrupt", 0x0a, 0x0d

align 4                         ; .align 2
ignore_int:     
        push    eax
        push    ecx
        push    edx
        push    ds                      ; 这里请注意！！ds，es，fs，gs等虽然是16位的寄存器，但入栈后
        push    es                      ; 仍然会以32位的形式入栈，也即需要占用4个字节的堆栈空间。
        push    fs                      
        mov     eax, 0x10               ; 置段选择符（使ds，es，fs指向gdt表中的数据段）。
        mov     ds, ax
        mov     es, ax
        mov     fs, ax
        push    int_msg                 ; 把调用printk函数的参数指针（地址）入栈。注意！若int_msg
        call    printk                  ; 前不加‘$’,则表示把int_msg符号处的长字（‘Unkn’）入栈。
        pop     eax                     ; 该函数在/kernel/printk.c中。‘_printk’是printk编译后模块中
        pop     fs                      ; 内部表示法
        pop     es
        pop     ds
        pop     edx
        pop     ecx
        pop     eax
        iret                            ; 中断返回（把中断调用时压入栈的CPU标志寄存器（32位）值也弹出）。


; 
; Setup_paging
; 
; This routine sets up paging by setting the page bit
; in cr0. The page tables are set up, identity-mapping
; the first 16MB. The pager assumes that no illegal
; addresses are produced (ie >4Mb on a 4Mb machine).
;
; NOTE! Although all physical memory should be identity
; mapped by this routine, only the kernel page functions
; use the >1Mb addresses directly. All "normal" functions
; use just the lower 1Mb, or the local data space, which
; will be mapped to some other place - mm keeps track of
; that.
; for those with more memory than 16 Mb - tough luck. I've
; not got it, why sould you :-) The source is here. Change
; it. (Seriously - it shouldn't be too difficult. Mostly
; change some constants etc. I left it at 16Mb, as my machne
; even cannot be extended past that (ok, but it was cheap :-)
; I've tried to show which constants to change by having
; some kind of marker at them (search for "16Mb"), but I
; won't guarantee that's all :-( )
; 上面英文注释第2段的含义是指在机器物理内存中大于 1MB的内存空间要被用于主内存区。
; 主内存空间mhmm模块管理。它涉及到页面映射操作。内核中所有其他函数就是这里指的一般
; (普通)函数。若要使用主内存区的页面，就需要使用get_free_page()等函数获取。因为主内
; 存区中内存页面是共享资源，必须有程序进行统一管理以避免资源争用和竞争。
; 
; 在内存物理地址0x0处开始存放1页页目录表和4页页表。页目录是系统所有进程公用的，而
; 这里的4页页表则属于内核专用，它们一一映射线性地址起始16MB空间范围到物理内存上。对于
; 新建的进程，系统会在主内存区为其申请页面存放页表。另外，1页内存长度是4096字节。

align 4                                         ; 按4字节方式对齐内存地址边界。
setup_paging:                                   ; 首先对5页内存（1页目录 + 4页页表）清零
        mov     ecx, 1024*5                     ; 5 pages - pg_dir+4 page tables
        xor     eax, eax
        xor     edi, edi                        ; pg_dir is at 0x000
        cld                                     ; 页目录从0x000地址开始。
        rep stosd                               ; eax内容存到es:edi所指内存位置处，且edi增4。

; 下面4句设置面目录表中的项。因为我们（内核）共有4个页表，所以只需设置4项。
; 页目录项的结构与页表中项的结构一样，4个字节为1项。参见上面113行下的说明。
; 例如“$pg0+7”表示：0x00001007,是页目录表中的第1项。
; 则第1个页表所在的地址 = 0x00001007 & 0xfffff000 = 0x1000;
; 第1个页表的属性标志 = 0x00001007 & 0x00000fff = 0x07, 表示该页存在、用户可读写。
        mov     dword [_pg_dir], pg0+7          ; set present bit/user r/w
        mov     dword [_pg_dir+4], pg1+7
        mov     dword [_pg_dir+8], pg2+7
        mov     dword [_pg_dir+12], pg3+7

; 下面6行填写4个页表中所有项的内容，共有：4（页表）*1024（项/页表）=4096项（0-0xfff），
; 也即能映射物理内存 4096*4KB = 16MB。
; 每项的内容是：当前项所映射的物理内存地址 + 该页的标志（这里均为7）。
; 填写使用的方法是从最后一个页表的最后一项开始按倒退顺序填写。每一个页表中最后一项在表中
; 的位置是 1023*4 = 4092。因此最后一页的最后一项的位置就是$pg3+4092。

        mov     edi, pg3+4092                   ; edi->最后一页的最后一项
        mov     eax, 0xfff007                   ; 16Mb - 4096 + 7 (r/w user,p)  
                                                ; 最后1项对应物理内存页面的地址是0xfff000
                                                ; 加上属性标志7，即为0xfff007
        std                                     ; 方向位置位，edi值递减（4个字节）。
.1:     stosd                                   ; fill pages backwards- more efficient :-)
        sub     eax, 0x1000                     ; 每填写好一项，物理地址值减0x1000。
        jge     .1                              ; 如果小于0则说明全填写好了。
; 现在设置页目录表基址寄存器cr3，指向页目录表。cr3中保存的是页目录表的物理地址，然后
; 再设置启动使用分页处理（cr0的PG标志，位31）
        xor     eax, eax                        ; pg_dir is at 0x0000   ; 页目录表在0x000处
        mov     cr3, eax                        ; cr3 - page directory start
        mov     eax, cr0
        or      eax, 0x80000000                 ; 添上PG标志
        mov     cr0, eax                        ; set paging (PG) bit
        ret                                     ; this also flushes prefetch-queue

; 在改变分页处理标志后再使用转移指令刷新刷新预取指令队列。这里用的是返回指令ret。
; 该返回指令的另一个作用是将140行压入堆栈中的main程序的地址弹出，并跳转到/init/main.c
; 程序去运行。本程序到此就真正结束了。

align 4                                         ; 按4字节对齐内存地址边界。
dw 0                                            ; 这里先空出2字节，这样224行上的长字是4字节对齐的

; 下面是加载中断描述符表寄存器idtr的指令lidt要求的6字节操作数。前2字节是idt表的限长，
; 后4字节是idt表在线性地址空间中的32位基地址。
idt_descr:
        dw      256*8-1                         ; idt contains 256 entires ; 共256项，限长=长度-1。
        dd      _idt
align 4
dw 0

; 下面加载全局描述符表寄存器gddtr的指令lgdt要求的6字节操作数。前2字节是gdt表的限长，
; 后4字节是gdt表的线性基地址。这里全局表长度设置为2KB字节（0x7ff即可），因为每8字节
; 组成一个描述符项，所以表中共可有256项。符号_gdt是全局表在本程序中的偏移位置，见234行。
gdt_descr:
        dw      256*8-1                         ; so does gdt (note that that's any
        dd      _gdt                            ; magic number, but it works for me :^)

align 8                                         ; 按8（2^3）字节方式对齐内存地址边界。
_idt:   times 256 dq 0                          ; idt is uninitialized  ; 256项，每项8字节，填0.

; 全局描述符表。其前4项分别是：空项（不用）、代码段描述符、数据段描述符、系统调用段描述符，
; 其中系统调用段描述符并没有派用处，Linus当时可能曾想把系统调用代码放在这个独立段中。
; 后面还预留了252项的空间，用于放置亲创建任务的局部描述符（LDT）和对应的任务段TSS
; 的描述符。
; (0-nul, 1-cs, 2-ds, 3-syscall, 4-TSS0, 5-LDT0, 6-TSS1, 7LDT1, 8-TSS2 etc...)

_gdt:   dq      0x0000000000000000              ; NULL descriptor
        dq      0x00c09a0000000fff              ; 16Mb          ; 0x08,内核代码段最大长度16MB。
        dq      0x00c0920000000fff              ; 16Mb          ; 0x10,内核数据段最大长度16MB。
        dq      0x0000000000000000              ; TEMPORARY - don't use
        times 252 dq 0                          ; space for LDT's and TSS's etc ; 预留空间