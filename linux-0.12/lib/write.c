/*
 *  linux/lib/write.c
 *
 */

#define __LIBRARY__
#include <unistd.h>
#include <sys/types.h>

_syscall3(int,write,int,fd,const char *,buf,off_t,count)