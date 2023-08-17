# 0 "keyboard.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "keyboard.S"












 





	
.text
.global keyboard_interrupt	// 申明为全局变量，用于在初始化时设置键盘中断描述符。





// size是键盘缓冲区（缓冲队列）长度（字节数）。

size 	= 1024 				


// 以下是键盘缓冲队列数据结构tty_queue中各字段的偏移量（include/linux/tty.h，第22行）。
head 	= 4 			// 缓冲区头指针字段在tty_queue结构中的偏移。
tail	= 8 			// 缓冲区尾指针字段偏移。
proc_list = 12			// 等待该缓冲队列的进程字段偏移。
buf	= 16			// 缓冲区字段偏移。
	
// 在本程序中使用了3个标志字节。mode是键盘特殊键（ctrl、alt或caps）的按下状态标志；
// leds是用于表示键盘指示灯的状态标志。e0是当收到扫描码0xe0或0xe1时设置的标志。
// 每个字节标志中各位的含义见如下说明：
// （1）mode是键盘特殊键的按下状态标志。
// 表示大小写转换键（caps）、交换键（alt）、控制键（ctrl）和换档键（shift）的状态。
// 	位7 caps键按下；		位3右ctrl键按下；
// 	位6 cpas键的状态；	位2左ctrl键按下；
// 	位5 右alt键按下；		位1右shift键按下；
// 	位4 左alt键按下；		位0左shift键按下；
// （2）leds是用于表示键盘指示灯的状态标志。即表示数字锁键（num-lock）、大小写转换
// 键（caps-lcok）和滚动锁定键（scroll-lock）的LED发光管状态。
//	位7-3全0不用；		位1 num-lock（初始置1，即数字锁定键发光管亮）；
//	位2 caps-lock；		位0 scroll-lock。
// （3）e0是扫描前导码标志。当扫描码是0xe0或0xe1时，置该标志。表示其后还跟随着1个或2
// 个字符扫描码。若收到扫描码0xe0则意味着还有一个字符跟随其后；若收到扫描码0xe1则表示
// 后面还跟着2个字符。参见程序列表后说明。e0标志可用于区别键盘上两个相同的键或同一按
// 键上不同的功能，若e0标志是1，则表示两个相同按键中右边的一个按键，或者是数字小键盘上
// 方向键的功能。
// 	位1 = 1 收到0xe1标志；	位0 = 1 收到0xe0标志。
mode: 	.byte 0 	
leds:	.byte 2		
e0:	.byte 0









//// 键盘中断处理程序入口点。
// 当键盘控制器接收到用户的一个按键操作时，就会向中断控制器发出一键盘中断请求信号IRQ1。
// 当CPU响应该请求时就会执行键盘中断处理程序。该中断处理程序会从键盘控制器端口（0x60）
// 读入按键扫描码，并调用对应的扫描码子程序进行处理。
// 首先从端口0x60读取当前按键的扫描码，然后判断该扫描码是否是0xe0或0xe1。如果是的话
// 就立刻对键盘控制器作出应答，并向中断控制器发送中断结束（EOI）信号，以允许键盘控制器
// 能继续产生中断信号，从而让我们来接收后续的字符。如果接收到的扫描码该i结不是这两个特殊扫
// 描码，我们就根据扫描码值调用按键跳转表 key_table 中相应按键处理子程序，把扫描码对应
// 的字符放入读字符缓冲队列read_q中。然后，在对键盘控制器作出应答并发送EOI信号之后，
// 调用函数do_tty_interrupt()（实际上是调用 copy_to_cooked()）把read_p中的字符经过处理
// 后放到secondary辅助队列中。
keyboard_interrupt:
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	movl $0x10, %eax	// 将ds、es段寄存器置为内核数据段。
	mov %ax, %ds
	mov %ax, %es
	movl blankinterval, %eax
	movl %eax, blankcount	// 预置黑屏时间计数值为blankinterval（嘀嗒数）。
	xorl %eax, %eax		 
	inb $0x60, %al		// 读取扫描码->al。
	cmpb $0xe0, %al		// 扫描码是0xe0吗？若是则跳转到设置e0标志代码处。
	je set_e0
	cmpb $0xe1, %al		// 扫描码是0xe1吗？若是则跳转到设置e1标志代码处。
	je set_e1
	call *key_table(,%eax,4)	// 调用键处理程序 key_table + eax*4 （参见513行）。
	movb $0, e0		// 返回之后复位e0标志。

	// 下面这段代码（62-72行）针对使用8255A的PC标准键盘电路进行硬件复位处理。端口0x61
	// 是8255A输出口B的地址，该输出端口的第7位（PB7）用于禁止和允许对键盘数据的处理。
	// 这段程序用于对收到的扫描码做出应答。方法是首先禁止键盘，然后立刻重新允许键盘工作。
e0_e1:	inb $0x61, %al		// 取PPI端口B状态，其位7用于允许/禁止(0/1)键盘。
	jmp 1f			// 延迟一会。
1:	jmp 1f
1:	orb $0x80, %al		// al位7置位（禁止键盘工作）。
	jmp 1f
1:	jmp 1f
1:	outb %al, $0x61		// 使PPI PB7位置位。
	jmp 1f
1:	jmp 1f
1:	andb $0x7f, %al		// al位7复位。
	outb %al, $0x61		// 使PPI PB7位复位（允许键盘工作）。
	movb $0x20, %al		// 向8259中断芯片发送EOI（中断结束）信号。
	outb %al, $0x20
	pushl $0		// 控制台tty号=0，作为参数入栈。
	call do_tty_interrupt	// 将收到数据转换成规范模式并存放在规范字符缓冲队列中。
	addl $4, %esp		// 丢弃入栈的参数，弹出保留的寄存器，并中断返回。
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret
set_e0:	movb $1, e0 		// 收到扫描前导码0xe0时，设置e0标志的位0。
	jmp e0_e1
set_e1:	movb $2, e0 		// 收到扫描前导码0xe1时，设置e0标志的位1。
	jmp e0_e1









// 首先从缓冲队列地址表table_list（tty_io.c，81行）取控制台的读缓冲队列read_q地址
// 然后把al寄存器中的字符复制到读队列头指针处并把头指针前移1字节位置。若头指针移出
// 读缓冲区的末端，就让其回绕到缓冲区开始处。然后再看看此时缓冲队列是否已满，即比较
// 一下队列头指针是否与尾指针相等（相等表示满）。如果已满，就把ebx:eax中可能还有的
// 其余字符全部抛弃掉。如果缓冲区还未满，就把ebx:eax中数据联合右移8个比特（即把ah
// 值移到al，bl->ah，bh->bl），然后重复上面对al的处理过程。直到所有字符都处理完后，
// 就保存当前头指针值，再检查一下是否有进程等待着读队列，如果有就唤醒之。
put_queue:
	pushl %eax
	pushl %edx		// 下名取控制台tty结构中读缓冲队列指针。
	movl table_list, %edx	// read-queue for console
	movl head(%edx), %ecx	// 取队列头指针->ecx。
1:	movb %al, buf(%edx, %ecx)	// 将al中的字符放入头指针位置处。
	incl %ecx			// 头指针前移1字节。
	andl $size-1, %ecx		// 调整头指针。若超出缓冲区末端则绕回开始处。
	cmpl tail(%edx), %ecx		// buffer full - discard everything
					// 头指针==尾指针吗？（即缓冲队列满了吗？）
	je 3f				// 如果已满，则后面未放入的字符全抛弃。
	shrdl $8,%ebx,%eax		// 将ebx中8个比特右移8位到eax中，ebx不变。
	je 2f				// 还有字符吗？若没有（等于0）则跳转。
	shrl $8, %ebx			// 将ebx值右移8位，并跳转到标号1继续操作。
	jmp 1b
2:	movl %ecx, head(%edx)		// 若已将所有字符都放入队列，则保存头指针。
	movl proc_list(%edx), %ecx	// 该队列的等待进程指针？
	testl %ecx, %ecx		// 检测是否有等待队列的进程。
	je 3f				// 无，则跳转；
	movl $0, (%ecx)			// 有，则唤醒进程（置该进程为就绪状态）。
3:	popl %edx
	popl %ecx
	ret
// 从这里开始是键跳转表key_table中指针对应的各个按键（或松键）处理子程序。供上面第
// 60行语句调用。键跳转表 key_talbe 在第513行开始。
//
// 下面这段代码根据ctrl或alt的扫描码，分别设置模式标志mode中相应位。如果在该扫描
// 码之前收到0xe0扫描码（e0标志置位），则说明按下的是键盘右边的ctrl或alt键，则
// 对应设置ctrl或alt在模式标志mode中的比特位。
ctrl:	movb $0x04, %al			// 0x4是mode中左ctrl键对应的比特位（位2）。
	jmp 1f
alt:	movb $0x10, %al			// 0x10是mode中左alt键对应的比特位（位4）。
1:	cmpb $0, e0			// e0置位了吗（按下的是右边的ctrl/alt键吗）？
	je 2f				// 不是则跳转。
	addb %al, %al			// 是，则改成置相应右键标志位（位3或位5）。
2:	orb %al, mode			// 设置mode标志中对应的比特位。
	ret
// 这段代码处理ctrl或alt键松开时的扫描码，复位模式标志mode中的对应比特位。在处理
// 时要根据e0标志是否置位来判断是否是键盘右边的ctrl或alt键。
unctrl:	movb $0x04, %al			// mode中左ctrl键对应的比特位（位2）。
	jmp 1f
unalt:	movb $0x10, %al			// 0x10是mode中左alt键对应的比特位（位4）.
1:	cmpb $0, e0			// e0置位了吗（释放的是右边的ctrl/alt键吗）？
	je 2f				// 不是则转。
	addb %al, %al			// 是，则改成复位相应的右键的标志位（位3或位5）。
2:	notb %al			// 复位mode标志中对应的比特位。
	andb %al, mode
	ret

// 这段代码处理左、右shift键按下和松开时的扫描码，分别设置和复位mode中的相应位。
lshift:
	orb $0x01, mode			// 是左shift键按下，设置mode中位0.
	ret
unlshift:
	andb $0xfe, mode		// 是左shift键松开，复位mode中位0。
	ret
rshift:
	orb $0x02, mode			// 是右shift键按下，设置mode中位1。
	ret
unrshift:
	andb $0xfd, mode		// 是右shift键松开，复位mode中位1。
	ret

// 这段代码对收到caps键扫描码进行处理。通过mode中位7可以知道caps键当前是否正处于
// 在按下状态。若是则返回，否则就翻转mode标志中caps键按下的比特位（位6）和leds标
// 志中caps-lock比特位（位2），设置mode标志中caps键已按下标志位（位7）。
caps:	testb $0x80, mode		// 测试mode中位7是否已置位（即在按下状态）。
	jne 1f				// 如果已处于按下状态，则返回（186行）。
	xorb $4, leds			// 翻转leds标志中caps-lock比特位（位2）。
	xorb $0x40, mode		// 翻转mode标志中caps键按下的比特位（位6）。
	orb $0x80, mode			// 设置mode标志中caps键已按下标志位（位7）.
// 这段代码根据leds标志，开启或关闭LED指示器。	
set_leds:
	call kb_wait			// 等待键盘控制器输入缓冲空。
	movb $0xed, %al			
	outb %al, $0x60			// 发送键盘命令0xed到0x60端口。
	call kb_wait
	movb leds, %al			// 取leds标志，作为参数。
	outb %al, $0x60			// 发送该参数。
	ret
uncaps:	andb $0x7f, mode		// caps键松开，则复位mode中的对应位（位7）。
	ret
scroll:
	testb $0x03, mode		// 若此时ctrl键也同时按下，则
	je 1f
	call show_mem			// 显示内存状态信息（mm/memory.c，457行）。
	jmp 2f
1:	call show_state			// 否则显示进程状态信息（kernel/sched.c，45行）。
2:	xorb $1, leds			// scroll键按下，则翻转leds中对应位（位0）。
	jmp set_leds			// 根据leds标志重新开启或关闭LED指示器。
num:	xorb $2, leds			// num键按下，则翻转leds中的对应位（位1）。
	jmp set_leds			// 根据leds标志重新开启或关闭LED指示器。







// 代码首先判断扫描码是不是键盘上右侧数字小键盘按键发出。若不是则退出该子函数。若按下
// 的是数字小键盘上的最后一个键Del（0x53），则接着判断有无“Ctrl-Alt-Del”组合键按下。
// 若有该组合键按下则跳转到重启系统程序处。
// 请参考本程序列表后10.2.3节中给出的XT键盘扫描码表，或者浏览附录中的第1套键盘扫描码
// 集表。由表中信息可知，键盘右侧的数字小键盘中按键的扫描码范围是[0x47 - 0x53]，并且当它
// 们作为方向键使用时，会带有前导e0。
cursor:
	subb $0x47, %al			// 若扫描码不在[0x47 - 0x53]范围内，则返回（198行）。
	jb 1f				// 即扫描码不是数字小键盘上的按键按下发出的就返回。
	cmpb $12, %al			// (0x53 - 0x47 = 12)
	ja 1f
	jne cur2			
// 若等于12，说明del键已被按下，则继续判断ctrl和alt是否也被同时按下。
	testb $0x0c, mode		// 有ctrl键按下了吗？无，则跳转。
	je cur2
	testb $0x30, mode		// 有alt键按下吗？
	jne reboot			// 有，则跳转到重启动处理（594行）。

// 接下来代码判断所按小键盘按键是否作为方向（翻页、插入、删除等）键使用。若按下的按键确
// 实是小键盘上的其它按键而不是上述组合键，此时若收到有e0标志的前导扫描码e0，则表示
// 小键盘上按键被用作方向（翻页、插入或删除等）按键。于是跳转去处理光标移动或插入/删除
// 按键。若e0没有置位，则先查看一下num-lock的LED灯是否亮着，若没有亮则也进行光标移动
// 操作。但是若num-lock灯亮着（表示小键盘用作数字键），并且同时也按下了shift键，那么
// 我们把此时的小键盘按键也当作光标移动操作来处理。
cur2:	cmpb $0x01, e0			
					// e0标志置位了吗？
	je cur				// 置位了，则跳转光标移动处理处cur。
	testb $0x02, leds		
					
					// 测试leds中标志num-lock键标志是否置位。
	je cur				// 若没有置位（num的LED不亮），则也处理光标移动。
	testb $0x03, mode		
					// 测试模式标志mode中shift按下标志。
	jne cur				// 如果有shift键按下，则也进行光标移动处理。

// 最后说明数字小键盘按键被当作数字键键来使用。于是查询数字表num_table获得对应按键的数字
// 字符，放入缓冲队列中。由于要放入队列的字符数<=8，因此在执行put_queue前需把ebx清零，
// 见95行上的注释。
	xorl %ebx, %ebx			// 查询小数字表（210行），取键的数字ASCII码。
	movb num_table(%eax), %al	// 以eax作为索引值，取对应数字字符->al。
	jmp put_queue			// 字符放入缓冲队列中。
1:	ret

// 这段代码处理光标移动或插入/删除按键。这里首先取得光标字符表中相应键的代表字符。如果
// 该字符<='9'（5、6、2或3），说明是上一页、下一页、插入或删除键，则功能字符序列中要添
// 入字符‘~’。不过本内核并没有对它们进行识别和处理。然后就将ax中内容移到eax高字中，把
// 'esc ['放入ax，与eax高字中字符组成移动序列。最后把该字符序列放入字符队列中。
cur:	movb	cur_table(%eax), %al	// 取光标字符表中相应键的代表字符->al。
	cmpb	$'9, %al		// 若字符<='9'（5、6、2或3），说明是Page UP/Dn、
	ja 	ok_cur			// Ins或Del键，则功能字符序列中要添入字符‘~’。
	movb 	$'~, %ah		//
ok_cur:	shll	$16, %eax		// 将ax中内容移到eax高字中。
	movw	$0x5b1b, %ax		// 把’esc [’放入ax，与eax高字中字符组成移动序列。
	xorl	%ebx, %ebx		// 由于只需把eax中字符放入队列，因此需要把ebx清零。
	jmp	put_queue		// 将该字符放入缓冲队列中。





num_table:
	.ascii "789 456 1230,"

cur_table:	
	.ascii "HA4 DGC YB623"		// 小键盘上方向键或插入删除键对应的移动表示字符表。






// 该子我就能把功能键扫描码变换成转义字符序列，并存放到读队列中。代码检查的功能键范围是
// F1--F12。F1--F10的扫描码是0x3B--0x44，F11、F12扫描码是0x57，0x58。下面代码首先把
// F1--F12按键的扫描码转换成序号0--11，然后查询功能键表func_table得到对应的转义序列，
// 并放入字符队列中。若在按下一个功能键的同时还按下了一个Alt键，则只去执行相应的控制
// 终端切换操作。
func:
	subb 	$0x3B, %al		// 键‘F1’的扫描码是0x3B，因此al中是功能键索引号
	jb	end_func		// 如果扫描码小于0x3b，则不处理，返回。
	cmpb	$9, %al			// 功能键是F1--F10？
	jbe	ok_func			// 是，则跳转。
	subb	$18, %al		// 是功能键F11，F12吗？F11、F12扫描码是0x57、0x58。
	cmpb 	$10, %al		// 是功能键F11？
	jb	end_func		// 不是，则不处理，返回。
	cmpb	$11, %al		// 是功能键F12？
	ja	end_func		// 不是，则不处理，返回。
ok_func:
	testb	$0x10, mode		// 左alt键同时按下了吗？
	jne	alt_func		// 是则跳转处理更换虚拟控制终端。
	cmpl	$4, %ecx		
					
	jl	end_func		// 需要放入4个字符，如果放不下，则返回。
	movl	func_table(,%eax,4), %eax	// 取功能键对应字符序列。
	xorl 	%ebx, %ebx
	jmp	put_queue
// 下面处理alt + Fn组合键，改变当前虚拟控制终端。此时eax中是功能键索引号（F1 -- 0），
// 对应虚拟控制终端号。
alt_func:
	pushl	%eax			// 虚拟控制终端号入栈，作为参数。
	call 	change_console		// 更改当前虚拟控制终端（chr_dev/tty_io.c，87行）。
	popl	%eax			// 丢弃参数。
end_func:
	ret






func_table:
	.long 0x415b5b1b,0x425b5b1b,0x435b5b1b,0x445b5b1b
	.long 0x455b5b1b,0x465b5b1b,0x475b5b1b,0x485b5b1b
	.long 0x495b5b1b,0x4a5b5b1b,0x4b5b5b1b,0x4c5b5b1b

// 扫描码-ASCII字符映射表。
// 根据前面定义的键盘类型（ FINNISH, US, GERMEN, FRANCH），将相应键的扫描码映射到
// ASCII字符。
# 413 "keyboard.S"

key_map:
	.byte 0,27
	.ascii "1234567890-="
	.byte 127,9
	.ascii "qwertyuiop[]"
	.byte 13,0
	.ascii "asdfghjkl;'"
	.byte '`,0
	.ascii "\\zxcvbnm,./"
	.byte 0,'*,0,32			/* 36-39 */
	.fill 16,1,0			
	.byte '-,0,0,0,'+		
	.byte 0,0,0,0,0,0,0		
	.byte '<
	.fill 10,1,0

shift_map:	
	.byte 0,27
	.ascii "!@#$%^&*()_+"
	.byte 127,9
	.ascii "QWERTYUIOP{}"
	.byte 13,0
	.ascii "ASDFGHJKL:\""
	.byte '~,0
	.ascii "|ZXCVBNM<>?"
	.byte 0,'*,0,32			/* 36-39 */
	.fill 16,1,0			
	.byte '-,0,0,0,'+		
	.byte 0,0,0,0,0,0,0		
	.byte '>
	.fill 10,1,0

alt_map:
	.byte 0,0
	.ascii "\0@\0$\0\0{[]}\\\0"
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0			
	.fill 16,1,0			
	.byte 0,0,0,0,0			
	.byte 0,0,0,0,0,0,0		
	.byte '|
	.fill 10,1,0

# 566 "keyboard.S"






// 代码首先根据mode标志选择一张相应的字符映射表（alt_map、shift_map或key_map）。然后
// 根据按键的扫描码查找该映射表，得到对应的ASCII码字符。接下来再根据当前字符是否与ctrl
// 或alt按键同时按下以及字符ASCII码值进行一定的转换。最后将转换所得结果字符存入读缓冲
// 队列中。
// 首先根据mode标志字节选择alt_map、shift_map或key_map映射表之一。
do_self:
	lea 	alt_map, %ebx		// 映射表alt_map基址->ebx（选用alt_map表）。
	testb	$0x20, mode		  
	jne	1f			// 是，则向前跳转到标号1处去映射字符。
	lea	shift_map, %ebx		// 否则，先用shift_map映射表。
	testb	$0x03, mode		// 有shift键同时按下了吗？
	jne	1f			// 有，则跳转到标号1处。
	lea	key_map, %ebx		// 否则，先用普通映射表key_map。

// 现在已选择好使用的映射表。接下来根据扫描码来取得映射表中对应的ASCII字符。若没有对应
// 字符，则返回（转none）。
1:	movb	(%ebx, %eax), %al	// 将扫描码作为索引值，取对应的ASCII码->al。
	orb	%al, %al		// 检测看是否有对应的ASCII码（不为0）。
	je	none			// 若没有（对应的ASCII码=0），则返回。
// 若此时ctrl键也同时按下或caps键锁定，并且字符在‘a’--’}’[0x61--0x7D]范围内，则
// 将其减去0x20（32），从而转换成相应的大写字符等[0x41--0x5D]。
	testb	$0x4c, mode		 
	je	2f			// 没有，则向前跳转标号2处。
	cmpb	$'a, %al		// 将al中的字符与‘a’比较。
	jb	2f			// 若al值<'a'，则跳转标号2处。
	cmpb	$'}, %al		// 将al中的值与’}‘比较。
	ja	2f			// 若al值>'}'，则转标号2处。
	subb 	$32, %al		// 将al转换为大写字符等（减0x20）。
// 若ctrl键已按下，并且字符在‘@’--’_’[0x40--0x5F]范围内，则将其减去0x40从而转换成
// 值[0x00--0x1F]。这是控制字符的ASCII码值范围。这表示当同时按下ctrl键和[‘@’--‘_’]范
// 围内的一个字符，可产生[0x00--0x1F]范围内对应的控制字符。例如，按下ctrl+‘M’会产生回车
// 字符（0x0D，即13）。
2:	testb	$0x0c, mode		
	je	3f			// 若没有则向前跳转标号3。
	cmpb	$64, %al		// 将al与‘A’前的‘@’（64）比较，即判断字符所属范围。
	jb	3f			// 若值<‘@’，则跳转标号3。
	cmpb	$64+32, %al		// 将al与‘_’后的‘`’（96）比较，即判断字符所属范围。
	jae	3f			// 若值>=‘`’，则转标号3。
	subb	$64, %al		// 否则减0x40，转换成0x00--0x1f范围的控制字符。
// 若左alt键同时按下，则将字符的位7置位。即此时可生成值大于0x7f的扩展字符集中的字符。
3:	testb	$0x10, mode		 
	je	4f			// 没有，则跳转标号4。
	orb	$0x80, %al		// 字符的位7置位。
// 将al中的字符放入读缓冲队列中。
4:	andl	$0xff, %eax		// 清eax的高字节和ah。
	xorl	%ebx, %ebx		// 由于放入队列字符数<=4，因此需把ebx清零。
	call	put_queue		// 将字符放入缓冲队列中。
none:	ret









// 注意，对于荷兰语和德语键盘，扫描码0x35对应的是‘-’键。参见第264和364行。
minus:	cmpb	$1, e0			// e0标志置位了吗？
	jne	do_self			// 没有，则调用do_self对减号符进行普通处理。
	movl	$'/, %eax		// 否则用‘/’替换减号‘-’->al。
	xorl	%ebx, %ebx		// 由于放入队列字符数<=4，因此需把ebx清零。
	jmp 	put_queue		// 并将字符放入缓冲队列中。


# 645 "keyboard.S"
key_table:
	.long none,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,ctrl,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,lshift,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,minus,rshift,do_self	
	.long alt,do_self,caps,func		
	.long func,func,func,func		
	.long func,func,func,func		
	.long func,num,scroll,cursor		
	.long cursor,cursor,do_self,cursor	
	.long cursor,cursor,do_self,cursor	
	.long cursor,cursor,cursor,cursor	
	.long none,none,do_self,func		
	.long func,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,unctrl,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,unlshift,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,unrshift,none		
	.long unalt,none,uncaps,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		








kb_wait:
	pushl	%eax
1:	inb	$0x64, %al			// 读键盘控制器状态。
	testb	$0x02, %al			// 测试输入缓冲器是否为空（等于0）。
	jne	1b				// 若不空，则跳转循环等待。
	popl 	%eax
	ret







// 该子程序往物理内存地址0x472处写值0x1234。该位置是启动模式（reboot_mode）标志字。
// 在启动过程中ROM BIOS会读取该启动模式标志值并根据其值来指导下一步的执行。如果该
// 值是0x1234，则BIOS就会跳过内存检测过程而执行热启动（Warm-boot）过程。如果若该
// 值为0，则执行冷启动（Cold-boot）过程。
reboot:
	call 	kb_wait				// 首先等待键盘控制器输入缓冲器空
	movw	$0x1234, 0x472			
	movb	$0xfc, %al			
	outb	%al, $0x64			// 向系统复位引脚和A20线输出负脉冲。
die:	jmp 	die				// 停机
