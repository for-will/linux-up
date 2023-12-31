/*
 * linux/fs/exec.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 */

/*
 * Demand-loding implemented 01.12.91 - no need to read anything but
 * the header into memory. The inode of the executable is put into
 * "current->executable", and page faults do the actual loading. Clean.
 *
 * Once more I can proudly say that linux stood up to being changed: it
 * was less than 2 hours work to get demand-loading completely implemented.
 */

/*
 * 需求时加载实现于 1991.12.1 - 只需将执行文件头部读进内存而无须将整个
 * 执行文件都加载进内存。执行文件的i节点被放在当前进程的可执行字段中
 * “current->executable”页异常会进行执行文件的实际加载操作。这很完美。
 *
 * 我可以再一次自豪的说，linux经得起修改：只用了不到2小时的工作时间就
 * 完全实现了需求加载处理。
 */

#include "sys/types.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code); // 退出程序系统调用，kernel/exit.c，第365行。
extern int sys_close(int fd);	    // 关闭文件系统调用，fs/open.c，第219行。

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB!
 *
 * MAX_ARG_PAGES定义了为程序分配的给参数和环境变量使用的最大内存页数。
 * 32页内存应该足够了，这使得环境和参数（env+arg）空间的总和达到128kB！
 */
#define MAX_ARG_PAGES 32

/// 使用库文件系统调用。
// 参数：library - 库文件名。
// 为进程选择一个库文件，并替换进程当前库文件i节点字段值为这里指定文件名的i节点
// 指针。如果library指针为空，则把进程当前的库文件释放掉。
// 返回：成功返回0，否则返回出错码。
int sys_uselib(const char * library)
{
	struct m_inode * inode;
	unsigned long base;

// 首先判断当前进程是否普通进程。这里通过查看当前进程的空间长度来做到的。因为普通进
// 程的空间长度被设置为TASK_SIZE(64MB)。因此若进程逻辑地址空间长度不等于TASK_SIZE
// 则返回出错码（无效参数）。否则取库文件i节点inode。若库文件名指针空，则设置inode
// 等于NULL。
	if (get_limit(0x17) != TASK_SIZE)
		return -EINVAL;
	if (library) {
		if (!(inode = namei(library)))	/* get library inode */
			return -ENOENT;		/* 取库文件i节点 */
	} else
		inode = NULL;
/* we should check filetypes (header etc), but we dont't */
/* 我们应该检查一下文件类型（如头部信息等），但是我们还没有这样做 */
// 然后放回进程原库文件i节点，并预置进程库i节点字段为空。接着取得进程的库代码所在
// 位置，并释放原库代码的页表和所占用的内存页面。最后让进程库i节点字段指针新库i节
// 点，并返回0（成功）。与加载执行程序类似，实际库代码要到真正使用时才会读入内存中。
// 另外，库文件代码被放置在进程空间的末端处，大小是4MB的整数倍，参见linux/sched.h。
	iput(current->library);
	current->library = NULL;
	base = get_base(current->ldt[2]);
	base += LIBRARY_OFFSET;			// linux/sched.h，第26行。
	free_page_tables(base, LIBRARY_SIZE);
	current->library = inode;
	return 0;
}

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * address on the "stack", returning the new stack pointer value.
 *
 * create_tables()函数在新任务内存中解析环境变量和参数字符串，由此
 * 创建指针表，并将它们的地址放到“栈”上，然后返回新栈的指针值。
 */
/// 在新任务栈中创建参数和环境变量指针。参见前面图12-31至图12-33。
// 参数：p - 数据段中参数和环境信息偏移指针；argc - 参数个数；envc - 环境变量个数。
// 返回：栈指针值。
static unsigned long * create_tables(char * p, int argc, int envc)
{
	unsigned long * argv, * envp;
	unsigned long * sp;

// 栈指针是以4字节（1节）为边界进行寻址的，因此这里需让sp为4的整数倍数。此时sp位于
// 参数环境表的末端。然后我们先把sp向下（低地址方向）移动，在栈中留出环境变量指针占用
// 的空间，并让环境变量指针envp指向该处。多留出的一个位置用于在最后存放一个NULL值。
// 再把sp向下移动，留出命令行参数指针占用的空间，并让argv指针指向该处。同样，多留出的
// 一个位置用于存放一个NULL值。此时sp指向参数指针块的起始处，我们将环境参数块指针envp
// 和命令行参数块指针以及命令行参数个数值分别压入栈中。注意，下面的指针加1操作，会使sp
// 递增指针宽度值（4字节）。
	sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
	sp -= envc+1;				// 即sp = sp - (envc+1);
	envp = sp;
	sp -= argc+1;
	argv = sp;
	put_fs_long((unsigned long) envp, --sp);
	put_fs_long((unsigned long) argv, --sp);
	put_fs_long((unsigned long) argc, --sp);
// 再将命令行各参数指针和环境变量和指针分别放入前面预留出来的相应地方，最后盆架放置一个
// NULL指针。
	while (argc-- > 0) {
		put_fs_long((unsigned long) p, argv++);
		while (get_fs_byte(p++)) /* nothing */ ; // p指针指向下一个参数串。
	}
	put_fs_long(0, argv);
	while (envc-- > 0) {
		put_fs_long((unsigned long) p, envp++);
		while (get_fs_byte(p++)) /* nothing */ ; // p指针指向下一个参数串。
	}
	put_fs_long(0, envp);
	return sp;				// 返回构造的当前新栈指针。
}

/*
 * count() counts the number of arguments/envelopes
 *
 * count()函数计算命令行参数/环境变量的个数。
 */
/// 计算参数个数。
// 参数：argv - 参数指针数组，明天后天一个指针项是NULL、
// 统计参数指针数组中指针的个数。关于函数参数传递指针的指针的作用，请参见程序
// kernel/sched.c中第171行前的注释。
// 返回：参数个数。
static int count(char ** argv)
{
	int i = 0;
	char ** tmp;
	if ((tmp = argv))
		while(get_fs_long((unsigned long *) (tmp++)))
			i++;

	return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 *
 * Modified by TYT, 11/24/91 to add the from_kmem argument, which specifies
 * whether the string and the string arrary are from user or kernel segments:
 *
 * from_kmem	argv *		argv **
 *	0	user space	user space
 *	1	kernel space	user space
 *	2	kernel space	kernel space
 *
 * We do this by playing games with the fs segment register. Since it
 * it is expensive to load a segment register, we try to avoid calling
 * set_fs() unless we absolutely have to.
 */
/*
 * ‘copy_string()’函数从用户内存空间拷贝参数/环境字符串到内核空闲页面中。
 * 这些已具有直接放到新用户内存中的格式。
 *
 * 由TYT（Tytso）于1991.11.24日修改，增加了from_kmem参数，该参数指明了
 * 字符串或字符串数组是来自用户段不是内核段。
 *
 * from_kmem	指针 argv *	字符串 argv **
 *	0	用户空间		用户空间
 *	1	内核空间		用户空间	
 *	2	内核空间		内核空间
 *
 * 我们量通过巧妙处理fs段寄存器来操作的。由于加载一个段寄存器代价太高，
 * 所以我们尽量避免调用set_fs()，除非实在必要。
 */
/// 复制参数字符串到进程的参数和环境空间中。
// 参数：argc - 欲添加的参数个数；argv - 参数指针数组；page - 参数和环境空间页面指针
// 数组。p - 参数表空间中偏移指针，始终指向已复制串的头部；from_kmem - 字符串来源标志。
// 在 do_execve()函数中，p初始化为指向参数表（128kB）空间的最后一个长字处，参数字符串
// 是以堆栈操作方式逆向往其中复制存放的。因此p指针会随着复制信息的增加而逐渐减小，
// 并始终指向参数字符串的头部。字符串来源标志from_kmem应该是TYT为了给execve()增添
// 执行脚本文件的功能而新加的参数。当没有运行脚本文件的功能时，所有参数字符串都在用
// 刻数据空间中。返回：参数和环境空间当前头部指针。若出错则返回0。
static unsigned long copy_strings(int argc, char ** argv, unsigned long * page,
				  unsigned long p, int from_kmem)
{
	char * tmp, * pag;
	int len, offset = 0;
	unsigned long old_fs, new_fs;

// 首先取当前段寄存器DS（指向内核数据段）和FS（用户数据段）值，盆架保存到变量new_fs
// 和old_fs中。若字符串及其数组（指针）来自内核空间，则设置FS段寄存器指向内核数据段。
	if (!p)
		return 0;			/* bullet-proofing */ /* 偏移指针验证 */
	new_fs = get_ds();
	old_fs = get_fs();
	if (from_kmem == 2)			// 若串及其指针在内核空间则设置fs指向内核空间。
		set_fs(new_fs);
// 然后循环处理各个参数，从最后一个参数逆向开始复制，复制到指定偏移地址处。在循环中，
// 首先取需要复制的当前字符串指针。如果字符串在用户空间而字符串数组（字符串指针）在
// 内核空间，则先设置FS段寄存器指向内核数据段（DS），并在内核数据空间中取了字符串指针
// tmp之后就立刻恢复FS段寄存器原值（FS再指回用户空间），否则不用修改fs值而直接从
// 用户空间取字符串指针到tmp。
	while (argc-- > 0) {
		if (from_kmem == 1)		// 若串指针在内核空间，则fs指向内核空间。
			set_fs(new_fs);
		if (!(tmp = (char *)get_fs_long(((unsigned long *)argv) + argc)))
			panic("argc is wrong");
		if (from_kmem == 1)		// 若串指针在内核空间，则fs指回用户空间。
			set_fs(old_fs);
// 然后从用户空间取该字符串，并计算该参数字符串长度len，此后tmp指向该字符串末端。
// 如果该字符串长度超过此时参数和环境空间中还剩余的空闲长度，则空间不够了。于是恢复
// fs段寄存器值（如果被改变的话）并返回0。不过因为参数和环境空间留有128KB，所以通
// 常不可能发生这种情况。
		len = 0;			/* remember zero-padding */
		do {				/* 我们我知道串是以NULL字节结尾的 */
			len++;
		} while (get_fs_byte(tmp++));
		if (p - len < 0) {		/* this shouldn't happen - 128kB */
			set_fs(old_fs);		/* 不会发生-因为有128kB的空间 */
			return 0;
		}
// 接着我们逆向把字符串逐个字符地复制到参数和环境空间末端处。在循环复制字符串的字符
// 过程中，我们首先要判断参数和环境空间中相应位置处是否已经有内存页面，如果还没有就
// 先为其申请1页内存页面。偏移量offset被用作为在一个页面中的当前指针偏移值。 因为
// 则开始执行本函数时，偏移变量offset被初始化为0，所以（offset-1 < 0）肯定成立而使得
// offset重新被设置为当前p指针在页面范围内的偏移值。
		while (len) {
			--p; --tmp; --len;
			if (--offset < 0) {
				offset = p % PAGE_SIZE;
				if (from_kmem == 2) // 若串在内核空间则fs指回用户空间。
					set_fs(old_fs);
// 如果当前偏移值p所在的串空间页面指针数组项page[p/PAGE_SIZE] == 0，表示此时p指针
// 所处的空间内存页面还不存在，则需申请一空闲内存页，并将该页面指针填入指针数组，同
// 时也将使页面指针pag指向该新页面。若申请不到空闲页面则返回0。
				if (!(pag = (char *) page[p/PAGE_SIZE]) &&
				    !(pag = (char *) (page[p/PAGE_SIZE] = get_free_page())))
					return 0;
				if (from_kmem == 2) // 若串在内核空间则fs指向内核空间。
					set_fs(new_fs);
			}
// 然后从fs段中复制字符串的1字节到参数和环境空间内存页面pag的offset处。
			*(pag + offset) = get_fs_byte(tmp);
		}
	}
// 最后，如果字符串和字符串数组在内核空间，则恢复fs段寄存器原值，然后返回参数和环境空
// 间中已复制参数的头部偏移值。
	if (from_kmem == 2)
		set_fs(old_fs);
	return p;
}

/// 修改任务的局部描述符表内容。
// 修改局部描述符表LDT中描述符的段基址和段限长，并将参数和环境空间页面放置在数据段
// 末端。
// 参数：text_size - 执行文件头部中a_text字段给出的代码段长度值；
// page - 参数和环境空间页面指针数组。
// 返回：数据段限长值（64MB）。
static unsigned long change_ldt(unsigned long text_size, unsigned long * page)
{
	unsigned long code_limit, data_limit, code_base, data_base;
	int i;

// 首先把代码和数据段长度均设置为64MB，然后取当前进程的LDT中代码段描述符中代码段基址。
// 代码段基址与数据段基址相同。再使用这些新值重新设置LDT中代码段和数据段描述符中的基址
// 和段限长。这里主注意，由于被加载的新程序的代码和数据段基址与原程序的相同，因此没有必
// 要再重复去设置它们，即第186和188行上的两条设置段基址的语句多余，可省略。
	code_limit = TASK_SIZE;
	data_limit = TASK_SIZE;
	code_base = get_base(current->ldt[1]);	// include/linux/sched.h，第277行。
	data_base = code_base;
	set_base(current->ldt[1], code_base);
	set_limit(current->ldt[1], code_limit);
	set_base(current->ldt[2], data_base);
	set_limit(current->ldt[2], data_limit);
/* make sure fs points to the NEW data segment */
/* 要确信FS段寄存器已指向新的数据段 */
// FS段寄存器中放入局部表数据段描述符的选择符（0x17），即默认情况下FS都指向任务数据段。
// 然后将参数和环境空间已存放数据的页面（最多有MAX_ARG_PAGES页，128kB）放到数据段末
// 端。方法是从进程空间库代码位置开始处逆向一页一页地放。库文件代码占用进程空间末端
// 部分，长度为4MB的整数倍。函数put_dirty_page()用于把物理页面映射到进程逻辑空间中。
// 参见mm/memory.c文件。
	__asm__("pushl $0x17\n\tpop %%fs"::);
	data_base += data_limit - LIBRARY_SIZE;
	for (i = MAX_ARG_PAGES-1; i >= 0; i--) {
		data_base -= PAGE_SIZE;
		if (page[i])			// 若该页面存在，就放置该页面。
			put_dirty_page(page[i], data_base);
	}
	return data_limit;			// 最后返回数据段限长（64MB）。
}

/*
 * 'do_execve()' executes a new program
 * NOTE! We leave 4MB free at the top of the data-area for a loadable
 * library.
 *
 * ‘do_execve()’函数执行一个新程序。
 * 注意：我们在数据区顶部保留4MB空闲空间给可加载库代码。
 */
/// execve()系统中断调用函数。加载并执行子进程（其他程序）。
// 该函数是系统中断调用（int 0x80）功能号__NR_execve调用的函数。函数的参数是进入系统调
// 用处理过程后直到调用本系统调用处理过程（system_call.s第214行）和调用本函数之前
// （system_call.s，第217行）逐步压入栈中的值。这些在system_call.s文件中的值包括：
// （1）第89--91行入栈的edx、ecx和ebx寄存器值，分别对应**envp、**argv和*filename；
// （2）第99行调用sys_call_table中的sys_execve函数时压入栈的函数返回地址（tmp）；
// （3）第216行在调用本函数do_execve前入栈的指向栈中调用系统中断的程序代码指针eip。
// 参数：eip - 调用系统中断的程序代码指针；tmp - 系统中断调用sys_execve时的返回地址；
// 	filename - 被执行程序文件名指针；argv - 命令行参数指针数组的指针；
// 	envp - 环境变量指针数组的指针。
// 返回：如果调用成功，则不返回；否则设置出错号，并返回-1。
int do_execve(unsigned long * eip, long tmp, char * filename,
	      char ** argv, char ** envp)
{
	struct m_inode * inode;
	struct buffer_head * bh;
	struct exec ex;
	unsigned long page[MAX_ARG_PAGES];	// 参数和环境串空间页面指针数组。
	int i, argc, envc;
	int e_uid, e_gid;
	int retval;
	int sh_bang = 0;			// 控制是否需要执行脚本程序。
	unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4; // p指向参数和环境空间的最后部。

	// char _tmp[100];
	// char * _p = filename, _c;
	// int _len = 0;
	// while ((_c = get_fs_byte(_p++))){
	// 	_tmp[_len++] = _c;
	// }
	// _tmp[_len] = '\0';
	// printk("=====> do_execve: %s\n", _tmp);

// 在正式设置执行文件的运行环境之前，让我们先做些准备工作。内核准备了128KB（32个页面）
// 空间来存放化执行文件的命令行参数和环境字符串。上行把p初始设置成位于128KB空间的最后
// 1个长字处。在初始参数和环境空间的操作过程中，p将用来指明在128KB空间中的当前位置。
// 另外，参数eip[1]是调用本次系统调用的原用户程序代码段寄存器CS值，其中的段选择符当然
// 必须是当前任务的代码段选择符（0x000f）。若不是该值，那么CS只能会是内核代码段的选择
// 符0x0008。但这是绝对不允许的，因为内核代码是常驻内存而不能被替换掉的。因此下面根据
// eip[1]的值确认是否符合正常情况。然后再初始化128KB的参数和环境串空间，把所有字节清零，
// 并取出执行文件的i节点。再根据函数参数分别计算出命令行参数和环境字符串的个数argc和
// envc。另外，执行文件必须是常规文件。
	if ((0xffff & eip[1]) != 0x000f)
		panic("execve called from supervisor mode");
	for (i = 0; i < MAX_ARG_PAGES; i++)	/* clear page-table */
		page[i] = 0;
	if (!(inode = namei(filename)))		/* get executables inode */
		return -ENOENT;
	argc = count(argv);			// 命令行参数个数。
	envc = count(envp);			// 环境字符串变量个数。

restart_interp:
	if (!S_ISREG(inode->i_mode)) {		/* must be regular file */
		retval = -EACCES;
		goto exec_error2;		// 若不是常规文件则置出错码，跳转到376行。
	}
// 下面检查当前进程是否有权运行指定的执行文件。即根据执行文件i节点中的属性，看看本进程
// 是否有权执行它。在把执行文件i节点的属性字段取到i中后，我们首先查看属性中是否设置
// 了“设置-用户-ID”（set-user_id）标志 和 “设置-组-ID”（set-group-id）标志。这两个
// 标志主要用于让一般用户能够执行特权用户（如超级用户root）的程序，例如改变密码的程序
// passwd等。如果set-user-id标志置位，则后面执行进程的有效用户ID（euid）就设置成执行
// 文件的用户ID，否则设置成当前进程的euid。如果执行文件set-group-id被置位的话，则执行
// 进程的有效组ID（egid）就设置为执行文件的组ID，否则设置成当前进程的egid。这里暂时把
// 这两个判断出来的值保存在变量e_uid和e_gid中。
	i = inode->i_mode;
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
	e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

// 现在根据进程的euid和egid与执行文件的访问属性进行比较。如果执行文件属于运行进程的用
// 户，则把文件属性值i右移6位，此时其最低3位是文件宿主的访问权限标志。否则的话如果执
// 行文件与当前进程的用户属于同组，则使属性值最低3位是执行文件组用户的访问权限标志。否
// 则此时属性字最低3位就是其他用户访问该执行文件的权限。
// 然后我们根据该最低3比特值来判断当前进程是否有权限运行这个执行文件。如果选出的相应用
// 户没有运行该文件的权力（位0量执行权限），并且其他用户也没有任何权限或者当前进程用户
// 不是超级用户，则表明当前进程没有权力运行这个执行文件。于是置不可执行出错码，并跳转到
// exec_error2处去作退出处理。
	if (current->euid == inode->i_uid)
		i >>= 6;
	else if (in_group_p(inode->i_gid))
		i >>= 3;
	if (!(i & 1) &&
	    !((inode->i_mode & 0111) && suser())) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
// 若程序能执行到这里，说明当前进程有运行指定执行文件的权限。因此从这里开始我们要取出
// 执行文件首部的数据，并根据其中的信息来分析设置运行环境，或者运行另一个shell程序来执
// 行脚本程序。首先读取执行文件的第一块数据到高速缓冲块中，并复制缓冲块数据到ex结构中。
// 如果执行文件开始的两个字节是字符‘#!’，则说明执行文件是一个脚本文本文件。若要运行脚本
// 文件，我们就需要执行脚本文件的解释程序（例如shell程序）。通常脚本文件的第一行文本均
// 为“#! /bin/bash”，它指明了运行脚本文件需要的解释程序。运行方法是从脚本文件第一行
// （带字符‘#!’）中取出其中的解释程序名及后面的参数（若有的话），然后将这些参数和脚本文
// 件名放进执行文件（此时是解释程序）的命令行参数空间中。在这之前我们当然需要先把函数指
// 定的原有命令行参数和环境字符串放到128KB空间中，而这里建立起来的命令行参数则放到它们
// 前面位置处（因为是逆向放置）。最后让内核执行脚本文件的解释程序。下面就是在设置好解释
// 程序的脚本文件名等参数后，取出解释程序的i节点并跳转到229行去执行解释程序。由于我们
// 需要跳转到执行过的代码229行去，因此在确认并处理了脚本文件之后需要设置一个禁止再次执
// 行下面的脚本处理代码标志sh_bang。在后面的代码中该标志也用来表示我们已经设置好执行文
// 件的命令行参数，不要重复设置。
	if (!(bh = bread(inode->i_dev, inode->i_zone[0]))) {
		retval = -EACCES;
		goto exec_error2;
	}
	ex = *((struct exec *) bh->b_data);	/* read exec-header */
	if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
		/*
		 * This section does the #! interpretation.
		 * Sorta complicated, but hopefully it will work.  -TYT
		 *
		 * 这部分处理对‘#!’的解释，有些复杂，但希望能工作。-TYT
		 */

		char buf[128], *cp, *interp, *i_name, *i_arg;
		unsigned long old_fs;

// 从这里开始，我们从脚本文件中提取解释程序名及其参数，并把解释程序名、解释程序的参数
// 和脚本文件名组合放入环境参数块中。首先复制脚本文件头1行字符‘#!’后面的字符串到buf
// 中，其中含有脚本解释程序名（例如/bin/sh），也可能还包含解释程序的几个参数。然后对
// buf中的内容进行处理。删除开始的空格、制表符。
		strncpy(buf, bh->b_data+2, 127);
		brelse(bh);			// 释放缓冲块并放回脚本文件i节点。
		iput(inode);
		buf[127] = '\0';
		if ((cp = strchr(buf, '\n'))) {
			*cp = '\0';		// 第1个换行符换成NULL并去掉空格制表符。
			for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp++) {};
		}
		if (!cp || *cp == '\0') {	// 若该行没有其他内容，则出错。
			retval = -ENOEXEC;	/* No interpreter name found */
			goto exec_error1;	/* 没有找到脚本解释程序名 */
		}
// 此时我们得到了开头是脚本解释程序名的一行内容（字符串）。下面分析该行。首先取第一个
// 字符串，它应该是解释程序名，此时i_name指向该名称。若解释程序名后还有字符，则它们应
// 该量解释程序的参数串，于是令i_arg指向该串。
		interp = i_name = cp;
		i_arg = 0;
		for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
			if (*cp == '/')
				i_name = cp+1;
		}
		if (*cp) {
			*cp++ = '\0';		// 解释程序名尾添加NULL字符。
			i_arg = cp;		// i_arg指向解释程序参数。
		}
		/*
		 * OK, we've parsed out the interpreter name and
		 * (optional) argument.
		 *
		 * OK，我们已经解析出解释程序的文件名以及（可选的）参数。
		 */
// 现在我们要把上面解析出来的解释程序名i_name及其参数i_arg和脚本文件名作为解释程序的
// 参数放进环境和参数块中。不过首先我们需要把函数提供的原来一些参数和环境字符串先放进去，
// 然后再放这里解析出来的。例如对于命令行是“example.sh -argq -arg2”来说，其执行文件是
// 一个脚本文件。脚本文件名是“example.sh”，参数是“-arg1 -arg2”。若脚本文件的首先内容是
// “#! /bin/bash -iarg1 -iarg2”，那么在放入这里解析的首先参数之后，新的命令行类似于这样：
// 	“bash -iarg1 -iarg2 example.sh -arg1 -arg2”
// 这里我们把sh_bang标志置上，然后把函数参数提供的原有参数和环境字符串放入到空间中。环
// 境字符串个数是envc个，参数个数是argc-1个。少复制的一个原有参数是原执行文件名，即这
// 里的脚本文件名，将在下面进行处理。[[?? 可以看出，实际上我们不需要去另行处理脚本文件名，
// 即这里完全可以复制argc个参数，包括原来执行文件名（即现在的脚本文件名），因为它位于同一
// 个位置上]]。
// 注意！这里指针p随着复制信息增加而逐渐向小地址方向移动，因此这两个复制串函数执行完后，
// 环境参数串信息块位于程序命令行参数串信息块的上方，并且p指向程序的第1个参数串。另外，
// copy_string()最后一个参数（0）指明参数字符串有用户空间。
		if (sh_bang++ == 0) {
			p = copy_strings(envc, envp, page, p, 0);
			p = copy_strings(--argc, argv+1, page, p, 0);
		}
		/*
		 * Splice in (1) the interpreter's name for argv[0]
		 *	     (2) (optional) argument to interpreter
		 *	     (3) filename of shell script
		 *
		 * This is done in reverse order, because of how the
		 * user enviroment and arguments are stored.
		 */
		/*
		 * 拼接（1）argv[0]中放解释程序的名称
		 *    （2）（可选的）解释程序的参数
		 *    （3）脚本程序的名称
		 * 这是以逆序进行处理的，是由于用户环境和参数的存放方式造成的。
		 */
// 接着我们逆向复制脚本文件名、解释程序的参数各解释程序文件名到参数和环境空间中。
// 若出错，则置出错码，跳转到exec_error1。另外，由于本函数参数提供的脚本文件名
// filename在用户空间，但这里赋予copy_string()的脚本文件名的指针在内核空间，因
// 此这个复制字符串函数的最后一个参数（字符串来源标志）需要被设置成1。若字符串在
// 内核空间，则copy_string()的最后一个参数设置成2，如下面的第301、304行。
		p = copy_strings(1, &filename, page, p, 1);
		argc++;
		if (i_arg) {			// 复制解释程序的多个参数。
			p = copy_strings(1, &i_arg, page, p, 2);
			argc++;
		}
		p = copy_strings(1, &i_name, page, p, 2);
		argc++;
		if (!p) {
			retval = -ENOMEM;
			goto exec_error1;
		}
		/*
		 * OK, now restart the process with the interpreter's inode.
		 *
		 * OK，现在使用解释程序的i节点重启进程。
		 */
// 最后我们取得解释程序的i节点指针，然后跳转到229行去执行解释程序。为了获得解释程序的
// i节点，我们需要使用namei()函数，但是该函数使用的参数（文件名）是人用户数据空间得到
// 的，即ww段寄存器FS所指空间中取得。因此在调用namei()函数之前我们需要先临时让FS指向
// 内核数据空间，以让函数能从内核空间得到解释程序名，并在namei()返回后恢复FS的默认设置。
// 之后再跳转到restart_interp（229行）处重新处理新的执行文件 -- 脚本文件的解释程序。
		old_fs = get_fs();
		set_fs(get_ds());
		if (!(inode = namei(interp))) {	/* get executables inode */
			set_fs(old_fs);		/* 取得解释程序的i节点 */
			retval = -ENOENT;
			goto exec_error1;
		}
		set_fs(old_fs);
		goto restart_interp;
	}

// 此时缓冲块中的执行文件头结构数据已经复制到了ex中。于是先释放该缓冲块，并开始对ex
// 中的执行头信息进行判断处理。对于这个内核来说，它仅支持ZMAGIC执行文件格式，并且执行
// 文件代码都从逻辑地址0开始执行，因此不支持含有代码和数据重定位信息的执行文件。当然，
// 如果执行文件实在太大或者执行文件残缺不全，那么我们也不能运行它。因此对于下列情况将不
// 执行程序：执行文件不是可执行文件（ZMAGIC）、或者代码和数据重定位部分长度不等于0、或者
// （代码段 + 数据段 + 堆）长度超过50MB、或者执行文件长度小于（代码段 + 数据段 + 符号表
// 长度 + 执行头部分）长度的总和。
	brelse(bh);
	if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
	    ex.a_text + ex.a_data + ex.a_bss > 0x3000000 ||
	    inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF(ex)) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
// 另外，如果执行文件中代码开始处没有位于1个页面（1024字节）边界处，则也不能执行。
// 因为需求页（Demand paging）技术要求加载执行文件内容时以页面为单位，因此要求执行
// 文件映像中代码和数据都从页面边界处开始。
	if (N_TXTOFF(ex) != BLOCK_SIZE) {
		printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h", filename);
		retval = -ENOEXEC;
		goto exec_error2;
	}
// 如果sh_bang标志没有设置，则复制指定个数的命令行参数和环境字符串到参数和环境空间
// 中。若sh_bang标志已经设置，则表明是将运行脚本解释程序，此时环境变量页面已经复制，
// 无须再复制。同样，若sh_bang没有置位而需要复制的话，那么此时指针p随着复制信息增
// 加而逐渐向小地址方向移动，因此这两个复制串函数执行完后，环境参数串信息块位于程序
// 参数串信息块的上方，并且p指向程序的第1个参数串。事实上，p是128KB参数和环境空
// 间中的偏移值。因此如果p=0，则表示环境变量与参数空间页面已经被点满，容纳不下了。
	if (!sh_bang) {
		p = copy_strings(envc, envp, page, p, 0);
		p = copy_strings(argc, argv, page, p, 0);
		if (!p) {
			retval = -ENOMEM;
			goto exec_error2;
		}
	}
/* OK, This is the pint of no return */
/* note that current->library stays unchagned by an exec */
/* OK，下面开始就没有返回的地方了 */
/* 注意，exec类函数不会改动current->library */
// 前面我们根据函数参数提供的信息，对运行执行文件的命令行参数和环境空间进行的设置，但还
// 没有为执行文件做过什么实质性的工作，即还没有做过为执行文件初始化进程任务结构信息、建
// 立页表等工作。现在我们就来做这些工作。由于执行文件直接使用当前进程的“躯壳”，即当前
// 进程将被改造成执行文件的进程，因此我们需要首先释放当前进程占用的某些系统资源，包括关
// 闭指定的已打开文件、占用的页表和内存页面等。然后根据执行文件头结构信息修改当前进程使
// 用的局部描述符表LDT中描述符的内容，重新设置代码段和数据段描述符的限长，再利用前面处
// 理得到的e_uid和e_gid等信息来设置进程任务结构中相关的字符。最后把执行本次系统调用程
// 序的返回地址eip[]指向执行文件中代码的起始位置处。这样当本系统调用退出返回后就会去运
// 行新执行文件的代码了。注意，虽然此时新执行文件的代码和数据还没有加载到内存中，但其参
// 数和环境块已经在copy_strings()中使用get_free_page()分配了物理内存页来保存数据，并在
// change_ldt()函数中使用put_page()存放到了进程逻辑空间的末端。另外，在create_tables()
// 中也会由于在用户栈上存放参数和环境指针表而引起缺页异常，从而内存管理程序也会就此为用
// 户栈空间映射物理内存页。
//
// 这里我们首先放回进程原执行程序的i节点，并且让进程executable字符指向新执行文件的i
// 节点。然后复位原进程的所有信号处理句柄，但对于SIG_IGN句柄无须复位。再根据设定的执行
// 时关闭文件句柄（close_on_exec）位图标志，关闭指定的打开文件并复位该标志。
	if (current->executable)
		iput(current->executable);
	current->executable = inode;
	current->signal = 0;
	for (i = 0; i < 32; i++) {
		current->sigaction[i].sa_mask = 0;
		current->sigaction[i].sa_flags = 0;
		if (current->sigaction[i].sa_handler != SIG_IGN)
			current->sigaction[i].sa_handler = NULL;
	}
	for (i = 0; i < NR_OPEN; i++)
		if ((current->close_on_exec>>i)&1)
			sys_close(i);
	current->close_on_exec = 0;
// 然后根据当前进程指定的基地址和限长，释放原来程序的代码段和数据段所对应的内存页表指定
// 的物理内存页面及页表本身。此时新执行文件并没有占用主内存区任何页面，因此在处理器真正
// 运行新执行文件代码时就会引起缺页异常中断。此时内存管理程序即会执行缺页处理而为新执行
// 文件申请内存页面和设置相关页表项，并且把相关执行文件页面读入内存中。如果“上次任务使
// 用了协处理器”指向的是当前进程，则将其置空，并复位使用也协处理器的标志。
	free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	current->used_math = 0;
// 然后我们根据新执行文件头结构中的代码长度字段a_text的值，来修改局部表中描述符基址和
// 段限长，并将128KB的参数和环境空间页面放置在数据段末端。执行下面语句之后，p此时更改
// 成以数据段起始处为原点的偏移值，但仍指向参数和环境空间数据开始处，即已转换成为栈指针
// 值。然后调用内部函数create_tables()，在栈空间中创建环境和参数变量指针表，供程序的
// main()作为参数使用，并返回该栈指针。
	p += change_ldt(ex.a_text, page);
	p -= LIBRARY_SIZE + MAX_ARG_PAGES*PAGE_SIZE;
	p = (unsigned long) create_tables((char *)p, argc, envc);

// 接着再修改进程各字段值为新执行文件的信息。即令进程任务结构代码尾字段end_code等于执
// 笔文件的代码长度a_text；数据尾字段end_data等于执行文件的代码段长度加数据段长度
// （a_data + a_text）；并令进程堆结尾字段brk = a_text + a_data + a_bss。brk用于指明
// 进程当前数据段（包括未初始化数据部分）末端位置，供内核为进程分配内存时指定分配开始位
// 置。然后设置进程栈开始字段为栈指针所在页面，并重新设置进程的有效用户id和有效组id。
	current->brk = ex.a_bss +
		       (current->end_data = ex.a_data + (current->end_code = ex.a_text));
	current->start_stack = p & 0xfffff000;
	current->suid = current->euid = e_uid;
	current->sgid = current->egid = e_gid;
// 最后将原调用系统中断的程序在堆栈上的代码指针替换为指向新执行程序的入口点，并将栈指针
// 替换为新执行文件的栈指针。此后返回指令指令将弹出这些栈数据并使得CPU去执行新执行文件，因
// 此不会返回到原调用系统中断的程序中去了。
	eip[0] = ex.a_entry;			/* eip, magic happens :-) eip，魔法起作用了 */
	eip[3] = p;				/* stack ponter esp，堆栈指针 */
	return 0;
exec_error2:
	iput(inode);				// 放回i节点。
exec_error1:
	for (i = 0; i < MAX_ARG_PAGES; i++)
		free_page(page[i]);		// 释放存放参数和环境串的内存页面。
	return (retval);			// 返回出错码。
}

