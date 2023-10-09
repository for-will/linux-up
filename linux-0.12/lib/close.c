/*
 *  linux/lib/close.c
 *
 * (C) 1991 Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 关闭文件函数。
// 下面该宏对应函数原型：int close(int fd)。它直接调用系统中断 init 0x80，参数是__NR_close。
// 其中fd是文件描述符。
_syscall1(int,close,int,fd)

