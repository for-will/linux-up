#ifndef _SYS_TIME_H
#define _SYS_TIME_H

/* gettimofday returns this */
/* gettimeofday()函数返回该时间结构 */
struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* microseconds */
};

// 时间区结构。tz为时区（Time Zone）的缩写，DST（Daylight Saving Time）是夏令时的缩写。
struct timezone {
        int     tz_minuteswest; /* minutes west of Greenwich / 格林威治西部分钟时间 */
        int     tz_dsttime;     /* type of dst correction    / 夏令时区高速时间 */
};

#define DST_NONE	0	/* not on dst / 非夏令时 */
#define DST_USA		1	/* USA style dst / USA形式的夏令时 */
#define DST_AUST	2	/* Australian style dst / 澳洲形式的夏令时 */
#define DST_WET		3	/* Western European dst */
#define DST_MET		4	/* Middle European dst */
#define DST_EET		5	/* Eastern European dst */
#define DST_CAN		6	/* Canada */
#define DST_GB		7	/* Great Britain and Eire */
#define DST_RUM		8	/* Rumania */
#define DST_TUR		9	/* Turkey */
#define DST_AUSTAIL	10	/* Australian style with shift in 1986 */

/*
 * Operations on timevals.
 *
 * NB: timercmp does not work for >= or <=.
 */
/*
 * 用于timevals上的操作。
 * 注意：这里的timercmp不能用于 >= 或 <=。
 */
// timeval时间结构的操作函数。
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#define timercmp(tvp, uvp, cmp) \
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	(tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
/* 内部定时器名称和结构，用于定义定时器设置。 */
#define ITIMER_REAL	0	/* 以实际时间递减 */
#define ITIMER_VIRTUAL	1	/* 以进程虚拟时间递减 */
#define ITIMER_PROF	2	/* 以进程虚拟时间或者当系统运行时以进程时间递减 */

#include <time.h>
#include <sys/types.h>

int gettimeofday(struct timeval * tp, struct timezone * tz);
int select(int width, fd_set * readfds, fd_set * writefds,
	   fd_set * exceptfds, struct timeval * timeout);

#endif /* _SYS_TIME_H */

