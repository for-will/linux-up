/* 
 *  linux/kernel/printk.c
 *  (C) 1991  Linus Torvalds
 */

/* 
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' thins. Make a printf with fs-saving, and
 * all is well.
 * 
 * 当处于内核模式时，我们不能使用printf，因为寄存器fs指向其他不感兴趣
 * 的地方。自己编制一个printf并在使用前保存fs，一切就解决了。
 */
// 标准参数头文件。以宏的形式定义变量参数列表。主要说明了一个类型（va_list）事三个宏
// va_start、va_arg和va_end，用于vsprintf、sprintf、sfprintf函数。
#include <stdarg.h>
#include <stddef.h>		// 标准定义头文件。定义了NULL，offsetof(TYPE, MEMEBER)。

static char buf[1024];		// 显示用临时缓冲区。

// 函数vsprintf()定义在linux/kernel/vsprintf.c中92行开始处。
extern int vsprintf(char * buf, const char * fmt, va_list args);

// 内核使用的显示函数。
int printk(const char *fmt, ...)
{
	va_list args;			// va_list实际上是一个字符指针类型。
	int i;

// 运行参数处理开始函数。然后使用格式串fmt将参数列表args输出到buf中。返回值i
// 等于输出字符串的长度。再运行参数处理结束函数。最后调用控制台显示函数并返回显示
// 字符数。
	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);
	console_print(buf);		// chr_drv/console.c，第995行开始。
	return i;
}

int ksprintf(char * str, const char *fmt, ...)
{
        va_list args;
        int i;

        va_start(args, fmt);
        i = vsprintf(str, fmt, args);
        va_end(args);
        return i;
}
