#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

// 文件状态数据结构。其中所有字段均可以从文件的i节点结构中获取。
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

// 两面是为st_mode字段所用的值字义的符号名称。这些值均用八进制表示。为便于记住，这些符
// 号名称均为一些英文单词的首字母或缩写组合而成。例如名称S_IFMT的他喜欢我好字母分别代表单词
// State、Inode、File、Mask和Type；而名称S_IFREG则是State、Inode、File和REGular几
// 个大写字母的组合；名称S_IRWXU是State、Inode、Read、Write、eXcute和User的组合。其
// 它名称可以此类推。
// 下面是文件类型：
#define S_IFMT	00170000	/* 文件类型比特位屏蔽码（8进制表示） */
#define S_IFLNK  0120000	/* 符号链接 */
#define S_IFREG  0100000	/* 常规文件 */
#define S_IFBLK	 0060000	/* 块特殊（设备）文件，如磁盘dev/fd0 */
#define S_IFDIR  0040000	/* 目录 */
#define S_IFCHR  0020000	/* 字符设备文件 */
#define S_IFIFO  0010000	/* FIFO特殊文件 */

// 文件属性位：
// S_ISUID用于测试文件的set-user-ID标志是否置位。若该标志置位，则当执行该文件时，进程的
// 有效用户ID将被设置为该文件宿主的用户ID。S_ISGID则是针对组ID进行相同处理。
#define S_ISUID  0004000	/* 执行时设置用户ID（set-user-ID） */
#define S_ISGID  0002000	/* 执行时设置组ID（set-group-ID） */
#define S_ISVTX  0001000	/* 对于目录，受限删除标志 */

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK) /* 测试是否符号链接文件 */
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG) /* 测试是否常规文件 */
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR) /* 是否目录文件 */
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR) /* 是否字符设备文件 */
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK) /* 是否块设备文件 */
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO) /* 是否FIFO特殊文件 */

// 文件访问权限：
#define S_IRWXU 00700	/* 宿主可以读、写、执行/搜索（名称最后字母代表User） */
#define S_IRUSR 00400	/* 宿主读许可 */
#define S_IWUSR 00200	/* 宿主写许可 */
#define S_IXUSR 00100	/* 宿主执行/搜索许可 */

#define S_IRWXG 00070	/* 组成员可以读、写、执行/搜索（名称最后字母代表Group） */
#define S_IRGRP 00040	/* 组成员读许可 */
#define S_IWGRP 00020	/* 组成员写许可 */
#define S_IXGRP 00010	/* 组成员执行/搜索许可 */

#define S_IRWXO 00007	/* 其他人读、写、执行/搜索许可（名称最后字母O代表Other） */
#define S_IROTH 00004	/* 其他人读许可（最后3个字母代表Other） */
#define S_IWOTH 00002	/* 其他从写许可 */
#define S_IXOTH 00001	/* 其他从执行/搜索许可 */

extern int chmod(const char *_path, mode_t mode); 	// 修改文件属性。
extern int fstat(int fildes, struct stat * stat_buf); 	// 取指定文件句柄的文件状态信息。
extern int mkdir(const char * _path, mode_t mode);	// 创建目录。
extern int mkfifo(const char * _path, mode_t mode);	// 创建管道文件。
extern int stat(const char * filename, struct stat * stat_buf); // 取指定文件名的文件状态信息。
extern mode_t umask(mode_t mask);			// 设置属性屏蔽码。

#endif

