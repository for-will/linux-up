#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;			// 用于对象的大小（长度）。
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;				// 用于时间（以秒计）。
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

#ifndef NULL
#define NULL ((void *) 0)
#endif

typedef int pid_t;			// 用于进程号和进程组号。
typedef unsigned short uid_t;		// 用于用户号（用户标识号）。
typedef unsigned short gid_t;		// 用于组号。
typedef unsigned short dev_t;		// 用于设置号。
typedef unsigned short ino_t;		// 用于文件序列号。
typedef unsigned short mode_t;		// 用于某些文件属性。
typedef unsigned short umode_t;		// 
typedef unsigned char nlink_t;		// 用于链接计数。
typedef int daddr_t;
typedef long off_t;			// 用于文件长度（大小）。
typedef unsigned char u_char;		// 无符号字符类型。
typedef unsigned short ushort;		// 无符号短整数类型。

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned long tcflag_t;

typedef unsigned long fd_set;		// 文件描述符集。每比特代表1个描述符。

typedef struct { int quot, rem; } div_t;		// 用于DIV操作。
typedef struct { long quot, rem; } ldiv_t;		// 用于长DIV操作。

// 文件系统参数结构，用于ustat()函数。最后两个字段未使用，总是返回NULL指针。
struct ustat {
	daddr_t f_tfree;		// 系统总空闲块数。
	ino_t f_tinode;			// 总空闲i节点数。
	char f_fname[6];		// 文件系统名称。
	char f_fpack[6];		// 文件系统压缩名称。
};

#endif

