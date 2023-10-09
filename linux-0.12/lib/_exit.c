/*
 * linux/lib/_exit.c
 *
 * (C) 1991 Linus Torvalds
 */

#define __LIBRARY__			// 定义一个符号常量，见下行说明。
#include <unistd.h>			// Linux标准头文件。字义了各种符号常数和类型，并声明了各种函数
					//  若字义了__LIBRARY__，则还含系统调用号内嵌汇编syscall0()等。
/// 内核使用的程序（退出）终止函数。
// 直接调用系统中断int 0x80，功能号__NR_exit。
// 参数：exit_code - 退出码。
// 函数名前的关键字volatile用于告诉编译器gcc，该函数不会返回。这样可让gcc产生更好一
// 些的代码，更重要的是使用这个关键字可以避免产生某些假警告信息。
// 等同于gcc的函数属性说明：void do_exit(int error_code) __attribute__ ((noreturn));
// volatile void _exit(int exit_code)
void _exit(int exit_code)
{
	// %0 - eax（系统调用号__NR_exit）；%1 - ebx（退出码exit_code）。
	__asm__("int $0x80"::"a" (__NR_exit),"b" (exit_code));
}
