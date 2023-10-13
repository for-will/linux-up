#ifndef _UNISTD_H
#define _UNISTD_H

/* ok, this may be a oke, but I'm working on it */
// 下面符号常数指出符合IEEE标准1003.1实现的版本号，是琴整数值。
#include "sys/types.h"
#include <stddef.h>
#define _POSIX_VERSION 198808L

// chown()和fchown()的使用受限于进程的权限。
#define _POSIX_CHOWN_RESTRICTED			/* only root can do a chown (I think..) */
// 长于（NAME_MAX）的路径名将产生错误，而不会自动截断。
#define _POSIX_NO_TRUNC				/* no pathname truncation (but see in kernel) */
// 下面这个符号字义成字符值，该值将禁止终端对其的处理
// _POSIX_VDISABLE用于控制终端某些特殊字符的功能。当一个终端termios结构中c_cc[]
// 数组某项字符代码值等于_POSIX_VDISABLE的值，表示禁止使用相应的特殊字符。
#define _POSIX_VDISABLE '\0'	/* character to disable things like ^C */
// 指明系统实现支持作业控制。
#define _POSIX_JOB_CONTROL
// 每个进程都有一保存的set-user-ID和一保存的set-group-ID。
#define _POSIX_SAVED_IDS	/* Implemented, for whatever good it is */

#define STDIN_FILENO	0			/* 标准输入文件句柄（描述符）号 */
#define STDOUT_FILENO	1			/* 标准输出文件句柄号 */
#define STDERR_FILENO	2			/* 标准出错文件句柄号 */

#ifndef NULL
#define NULL ((void *) 0)
#endif

/* access */
// 以下定义的符号常数用于access()函数。
#define F_OK	0				/* 检测文件是否存在 */
#define X_OK	1				/* 检测是否可执行（搜索） */
#define W_OK	2				/* 检测是否可写 */
#define R_OK	4				/* 检测是否可读 */

/* lseek */
// 以下符号常数用于lseek()和fcntl()函数。
#define SEEK_SET	0			/* 将文件读写指针设置为偏移值 */
#define SEEK_CUR	1			/* 将文件读写指针设置为当前值加上偏移值 */
#define SEEK_END	2			/* 将文件读写指针设置为文件长度加上偏移值 */

/* _SC stand for System Configuration. We don't use them much */
// 下面的符号常数用于sysconf()函数。
#define _SC_ARG_MAX		1		/* 最大参数数量 */
#define _SC_CHILD_MAX		2		/* 子进程最大数 */
#define _SC_CLOCKS_PER_SEC	3		/* 每秒滴答数 */
#define _SC_NGROUPS_MAX		4		/* 最大组数 */
#define _SC_OPEN_MAX		5		/* 最大打开文件数 */
#define _SC_JOB_CONTROL		6		/* 作业控制 */
#define _SC_SAVED_IDS		7		/* 保存的标识符 */
#define _SC_VERSION		8		/* 版本 */

/* more (possibly) configurable things - now pathnames */
// 下面的符号常数用于pathconf()函数。
#define _PC_LINK_MAX		1		/* 连接最大数 */
#define _PC_MAX_CANON		2		/* 最大常规文件数 */
#define _PC_MAX_INPUT		3		/* 最大输入长度 */
#define _PC_NAME_MAX		4		/* 名称最大长度 */
#define _PC_PATH_MAX		5		/* 路径最大长度 */
#define _PC_PIPE_BUF		6		/* 管道缓冲大小 */
#define _PC_NO_TRUNC		7		/* 文件名不截断 */
#define _PC_VDISABLE		8		/*  */
#define _PC_CHOWN_RESTRICTED	9		/* 改变宿主受限 */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <utime.h>

#ifdef __LIBRARY__

// 以下是内核实现的系统调用符号常数，用作系统调用函数表中索引值（参见include/linux/sys.h）。
#define __NR_setup	0	/* used only by init, to get system going */
#define __NR_exit	1
#define __NR_fork	2
#define __NR_read	3
#define __NR_write	4
#define __NR_open	5
#define __NR_close	6
#define __NR_waitpid	7
#define __NR_creat	8
#define __NR_link	9
#define __NR_unlink	10
#define __NR_execve	11
#define __NR_chdir	12
#define __NR_time	13
#define __NR_mknod	14
#define __NR_chmod	15
#define __NR_chown	16
#define __NR_break	17
#define __NR_stat	18
#define __NR_lseek	19
#define __NR_getpid	20
#define __NR_mount	21
#define __NR_umount	22
#define __NR_setuid	23
#define __NR_getuid	24
#define __NR_stime	25
#define __NR_ptrace	26
#define __NR_alarm	27
#define __NR_fstat	28
#define __NR_pause	29
#define __NR_utime	30
#define __NR_stty	31
#define __NR_gtty	32
#define __NR_access	33
#define __NR_nice	34
#define __NR_ftime	35
#define __NR_sync	36
#define __NR_kill	37
#define __NR_rename	38
#define __NR_mkdir	39
#define __NR_rmdir	40
#define __NR_dup	41
#define __NR_pipe	42
#define __NR_times	43
#define __NR_prof	44
#define __NR_brk	45
#define __NR_setgid	46
#define __NR_getgid	47
#define __NR_signal	48
#define __NR_geteuid	49
#define __NR_getegid	50
#define __NR_acct	51
#define __NR_phys	52
#define __NR_lock	53
#define __NR_ioctl	54
#define __NR_fcntl	55
#define __NR_mpx	56
#define __NR_setpgid	57
#define __NR_ulimit	58
#define __NR_uname	59
#define __NR_umask	60
#define __NR_chroot	61
#define __NR_ustat	62
#define __NR_dup2	63
#define __NR_getppid	64
#define __NR_getpgrp	65
#define __NR_setsid	66
#define __NR_sigaction	67
#define __NR_sgetmask	68
#define __NR_ssetmask	69
#define __NR_setreuid	70
#define __NR_setregid	71
#define __NR_sigsuspend	72
#define __NR_sigpending	73
#define __NR_sethostname	74
#define __NR_setrlimit	75
#define __NR_getrlimit	76
#define __NR_getrusage	77
#define __NR_gettimeofday	78
#define __NR_settimeofday	79
#define __NR_getgroups	80
#define __NR_setgroups	81
#define __NR_select	82
#define __NR_symlink	83
#define __NR_lstat	84
#define __NR_readlink	85
#define __NR_uselib	86

// 以下字义系统调用嵌入式汇编宏函数。
// 不带参数的系统调用宏函数。type name(void)。
// %0 - eax（__res）,%1 - eax(__NR_##name)。其中name是系统调用的名称，与__NR_组合形成上面
// 的系统调用符号常数，从而用来对系统调用表中函数指针寻址。
// 返回：如果返回值大于0，则返回该值，否则置出错号errno，并返回-1。
// 在宏字义中，若在两个标记符号之间有两个连续的井号‘##’，则表示在宏替换时会把这两个标记
// 符号连接在一起。例如下面第156行上的__NR##name，在替换了参数name（例如是fork）之后，
// 最后在程序中出现的将会是符号__NR_fork。参见《The C Programming Language》附录A.12.3。
#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name)); \
if (__res >=0 ) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

// 有1个参数的系统调用宏函数。type name(atype a)
// %0 - eax（__res），%1 - eax（__NR_name），%2 - ebx（a）。
#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

// 有2个参数的系统调用宏函数。type name(atype a, btype b)
// %0 - eax（__res），%1 - eax（__NR_name），%2 - ebx（a），%3 - ecx（b）。
#define _syscall2(type,name,atype,a,btype,b) \
type name(atype a, btype b) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

// 有3个参数的系统调用宏函数。type name(atype a, btype b, ctype c)
// %0 - eax（__res），%1 - eax（__NR_name），%2 - ebx（a），%3 - ecx（b），%4 - edx（c）。
// 本内核的系统调用最多带有三个参数。如果数据比较多，则可以把数据放在一个缓冲区中，并把
// 缓冲区指针作为参数传到内核中。
#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a,btype b,ctype c) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)),"d"((long)(c))); \
if (__res >= 0) \
	return (type) __res; \
errno=-__res; \
return -1; \
}

#endif /* __LIBRARY__ */

extern int errno;				// 出错号，全局变量。

// 下面是对应各系统调用的函数原型字义（详细说明参见include/linux/sys.h）。
int access(const char * filename, mode_t mode);
int acct(const char * filename);
int alarm(int sec);
int brk(void * end_data_segment);
void * sbrk(ptrdiff_t increment);
int chdir(const char * filename);
int chmod(const char * filename, mode_t mode);
int chown(const char * filename, uid_t owner, gid_t group);
int chroot(const char * filename);
int close(int fildes);
int creat(const char * filename, mode_t mode);
int dup(int fildes);
int execve(const char * filename, char ** argv, char ** envp);
int execv(const char * pathname, char ** argv);
int execvp(const char * file, char ** argv);
int execl(const char * pathname, char * arg0, ...);
int execlp(const char * file, char * arg0, ...);
int execle(const char * pathname, char * arg0, ...);
void exit(int status) __attribute__((noreturn));
void _exit(int status) __attribute__((noreturn));
int fcntl(int fildes, int cmd, ...);
int fork(void);
int getpid(void);
int getuid(void);
int geteuid(void);
int getgid(void);
int getegid();
int ioctl(int fildes, int cmd, ...);
int kill(pid_t pid, int signal);
int link(const char * filename1, const char * filename2);
int lseek(int fildes, off_t offset, int origin);
int mknod(const char * filename, mode_t mode, dev_t dev);
int mount(const char * specialfile, const char * dir, dev_t dev);
int nice(int val);
int open(const char * filename, int flag, ...);
int pause(void);
int pipe(int * fildes);
int read(int fildes, char * buf, off_t count);
int setpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
int setuid(uid_t uid);
int setgid(gid_t gid);
void (*signal(int sig, void (*fn)(int)))(int);
int stat(const char * filename, struct stat * stat_buf);
int fstat(int fildes, struct stat * stat_buf);
int stime(time_t * tptr);
int sync(void);
time_t time(time_t * tloc);
time_t times(struct tms * tbuf);
int ulimit(int cmd, long limit);
mode_t umask(mode_t mask);
int umount(const char * specialfile);
int unlink(const char * filename);
int ustat(dev_t dev, struct ustat * ubuf);
int utime(const char * filename, struct utimbuf * times);
pid_t waitpid(pid_t pid, int * wait_stat, int options);
pid_t wait(int * wait_stat);
int write(int fildes, const char * buf, off_t count);
int dup2(int oldfd, int newfd);
int getppid(void);
pid_t getpgrp(void);
pid_t setsid(void);
int sethostname(char * name, int len);
int setrlimit(int resource, struct rlimit *rlp);
int getrlimit(int resource, struct rlimit *rlp);
int getrusage(int who, struct rusage * rusage);
int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(struct timeval *tv, struct timezone *tz);
int getgroups(int gidsetlen, gid_t * gidset);
int setgroups(int gidsetlen, gid_t * gidset);
int select(int width, fd_set * readfds, fd_set * writefds,
	   fd_set * exceptfds, struct timeval * timeout);

#endif

