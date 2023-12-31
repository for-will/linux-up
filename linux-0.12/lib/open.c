/*
 *  linux/lib/open.c
 *
 * (C) 1991 Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

/// 打开文件函数。
// 打开一个文件或在文件不存在时创建一个文件。
// 参数：filename - 文件名；flag - 文件打开标志；...
// 返回：文件描述符，若出错则置出错码，并返回-1。
// 第13行定义了一个寄存器变量res，该变量将被保存在一个寄存器中，以便于高效访问和操作。
// 若想指定存放的寄存器（例如eax），那么可以把该句写成“register int res asm("ax");”。
int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

// 利用va_start()宏函数，取得flag后面参数的指针，然后调用系统中int 0x80，功能open进行
// 文件打开操作。
// %0 - eax（返回的描述符或出错码）；%1 - eax（系统蹼调用功能号__NR_open）；
// %2 - ebx（文件名filename）；%3 - ecx（打开文件标志flag）；%4 - edx（后随参数文件属性mode）。
	va_start(arg, flag);
	__asm__("int $0x80"
	 :"=a" (res)
	 :"0" (__NR_open),"b" (filename),"c" (flag),
	 "d" (va_arg(arg, int)));
// 若系统中断调用返回值大于或等于0，表示是一个文件描述符，则直接返回之。
// 否则说明返回值小于0，则代表一个出错码。设置该出错码并返回-1。
	if (res >= 0)
		return res;
	errno = -res;
	return -1;
}

