/*
 * linux/kernel/mktime.c
 *
 *  (C) 1991  Linux Torvalds
 */

#include <time.h>	// 时间头文件，定义了标准时间数据结构tm和一些处理时间函数原型。

/*
 * This isn't the library routine, is is only used in the kernel.
 * as much, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for ther library
 * routines (although I think minix times is pulic).
 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'a grumpy.
 */
#define MINUTE	60			// 1分钟的秒数
#define HOUR	(60*MINUTE)		// 1小时的秒数
#define DAY	(24*HOUR)		// 1天的秒数
#define YEAR	(365*DAY)		// 1年的秒数

/* interestingly we assume leap-years */
// 下面以年为界限，定义了每个月开始时的秒数时间。
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

// 该函数计算从1970年1月1日0时起到开机当日经过的秒数，作为开机时间。
// 参数tm中各字段已经在init/main.c中被赋值，信息取自CMOS。
long kernel_mktime(struct tm * tm)
{
	long res;
	int year;

// 首先计算1970年到现在经过的年数。因为是2位表示方式，所以会有2000年问题。我们可以
// 简单地在最前面添加一条语句来解决这个问题：if (tm->tm_year<70) tm->tm_year += 100;
// 由于UNIX计算年份y是从1970年算起。到1972年就是一个闰年，因此过3年（71，72，73）
// 就是第1个闰年，这样从1970年开始的闰年数计算方法就应该是 1 + (y-3)/4，即为
// (y + 1)/4。res = 这些年经过的秒数时间 + 每个闰年时多1天的秒数时间 + 当年到当月时
// 的秒数。另外，month[]数组中已经在2月份的天数中包含进了闰年的天数，即2月份天数
// 多算了1天。因此，若当年不是闰年并且当前月份大于2月份的话，我们就要减去这天。因
// 为从70年开始算起，所以当年是闰年的判断方法是（y + 2）能被4除尽。若不能除尽（有余
// 数）就不是闰年。
	if (tm->tm_year < 70)
		tm->tm_year += 100;
	year = tm->tm_year - 70;
/* magic offsets (y+1) needed to get leapyears right. */
	res = YEAR*year + DAY*((year+1)/4);
	res += month[tm->tm_mon];
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
	if (tm->tm_mon>1 && ((year+2)%4))
		res -= DAY;
	res += DAY*(tm->tm_mday-1);		// 再加上本月过去的天数的秒数时间。
	res += HOUR*tm->tm_hour;		// 再加上当天过去的小时数的秒数时间。
	res += MINUTE*tm->tm_min;		// 再加上1小时内过去的分钟数的秒数时间。
	res += tm->tm_sec;			// 再加上1分钟内已过的秒数。
	return res;				// 即等于从1970年以来经过的秒数时间。
}