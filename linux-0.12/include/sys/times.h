#ifndef _TIMES_H
#define _TIMES_H

#include <sys/types.h>

struct tms {
        time_t tms_utime;	// 用户使用的CPU时间。
        time_t tms_stime;	// 系统（内核）CPU时间。
        time_t tms_cutime;	// 已终止的子进程使用的用户PU时间。
        time_t tms_cstime;	// 已终止的子进程使用的系统CPU时间。
};

extern time_t times(struct tms * tp);

#endif 

