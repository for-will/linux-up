#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat {
	dev_t   st_dev;		// 含有文件的设备号。
	ino_t   st_ino;		// 文件i节点号。
	umode_t st_mode;	// 谁的类型和属性（见下面）。
	nlink_t st_nlink;	// 指定文件的链接数。
	uid_t   st_uid;		// 文件的用户（标识）号。
	gid_t   st_gid;		// 文件的组号。
	dev_t   st_rdev;	// 设备号（如果文件是特殊的字符文件或块文件）。
	off_t   st_size;	// 文件大小（字节数）（如果文件是常规文件）。
	time_t  st_atime;	// 上次（最后）访问时间。
	time_t  st_mtime;	// 最后修改时间。
	time_t  st_ctime;	// 最后节点修改时间。
};

#define S_IFMT	00170000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK	 0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000	/* 对于目录，受限删除标志 */

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)


#endif
