#ifndef _SCHED_H
#define _SCHED_H

#define HZ 100

#define NR_TASKS	64

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <signal.h>


#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

extern void panic(const char * str);

struct i387_struct {
	long 	cwd;
	long	swd;
	long 	twd;
	long 	fip;
	long 	fcs;
	long 	foo;
	long 	fos;
	long 	st_pace[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

struct tss_struct {
	long 	back_link;	/* 16 high bits zero */
	long 	esp0;
	long 	ss0;		/* 16 high bits zero */
	long 	esp1;
	long 	ss1;		/* 16 high bits zero */
	long	esp2;
	long 	ss2;		/* 16 high bits zero */
	long 	cr3;
	long 	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long 	esp;
	long 	ebp;
	long	esi;
	long 	edi;
	long 	es;		/* 16 high bits zero */
	long 	cs;		/* 16 high bits zero */
	long 	ss;		/* 16 high bits zero */
	long 	ds;		/* 16 high bits zero */
	long 	fs;		/* 16 high bits zero */
	long 	gs;		/* 16 high bits zero */
	long 	ldt;		/* 16 high bits zero */
	long 	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32];
	long blocked;	/* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,pgrp,session,leader;
	int	groups[NGROUPS];
	/*
	 * pointers to parent process, youngest child, younger sibling,
	 * older sibling, respectively. (p->father can be replaced with
	 * p->p_pptr->pid)
	 */
	struct task_struct	*p_pptr, *p_cptr, *p_ysptr, *p_osptr;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	unsigned long timeout,alarm;
	long utime,stime,cutime,cstime,start_time;
	struct rlimit rlim[RLIM_NLIMITS];
	unsigned int flags;	/* per process flags, defined below */
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	struct m_inode * library;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};

/* 
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640KB)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid tec.. */	0,0,0,0, \
/* suppl grps*/	{NOGROUP,}, \
/* proc links*/ &init_task.task,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* timeout */	0,0,0,0,0,0,0, \
/* rlimits */	{ {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}, \
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}, \
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}}, \
/* flags */	0, \
/* math */	0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/* tss */{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	  0,0,0,0,0,0,0,0, \
	  0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	  _LDT(0),0x80000000, \
	  	{0} \
	  }, \
}

extern struct  task_struct *task[NR_TASKS];
extern unsigned long startup_time;
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;

/*
 * Entry into gdt where to find first TSS. 0-null, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4) + (FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4) + (FIRST_LDT_ENTRY<<3))

#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))
#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))


#define switch_to(n) {\
struct {long a, b} __tmp; \
__asm__("cmpl %%eax,current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,current\n\t" \
	"ljmp %0\n\t" \
	"cmpl %%ecx,last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

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