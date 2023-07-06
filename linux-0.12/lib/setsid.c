/*
 *  linux/lib/setsid.c
 *
 */

#define __LIBRARY__
#include <unistd.h>
#include <sys/types.h>

_syscall0(pid_t,setsid)