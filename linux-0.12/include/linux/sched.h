#ifndef _SCHED_H
#define _SCHED_H

#include <linux/head.h>
#include <linux/mm.h>

extern unsigned long startup_time;

struct task_struct {
	long pid;
	struct desc_struct ldt[3];//todo:dummy
};

extern struct task_struct *last_task_used_math;
extern struct task_struct *current;

/*
 * Entry into gdt where to find first TSS. 0-null, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4

#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))

// HIGH:24~31	-- 基地址24~31 #addr[7]
// HIGH:00~07	-- 基地址16~23 #addr[4]
// LOW :16~31	-- 基地址00~15 #addr[2..3]
#define _get_base(addr) ({ \
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
	"movb %2,%%dl\n\t" \
	"shll $16,%%edx\n\t" \
	"movw %1,%%dx" \
	:"=d" (__base) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7))); \
__base;})

#define get_base(ldt) _get_base( ((char *) &(ldt)))


// LSL:Load Segment Limit
// r - 使用任意动态分配的寄存器
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif