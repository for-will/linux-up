/*
 * Why isn't this a .c file? Enquiring minds....
 */
/*
 * 为什么不是一个.c文件？动动脑筋自己想想....
 */

extern int sys_setup();				// 0 - 系统启动初始化设置函数。
extern int sys_exit();				// 1 - 程序退出。
extern int sys_fork();				// 2 - 创建进程。
extern int sys_read();				// 3 - 读文件。
extern int sys_write();				// 4 - 写文件。
extern int sys_open();				// 5 - 打开文件。
extern int sys_close();				// 6 - 关闭文件。
extern int sys_waitpid();			// 7 - 等待进程终止。
extern int sys_creat();				// 8 - 创建文件。
extern int sys_link();				// 9 - 创建一个文件的硬连接。
extern int sys_unlink();			// 10 - 删除一文件名（或删除文件）。
extern int sys_execve();			// 11 - 执行程序。
extern int sys_chdir();				// 12 - 更改当前目录。
extern int sys_time();				// 13 - 取当前时间。
extern int sys_mknod();				// 14 - 建立块/字符特殊文件。
extern int sys_chmod();				// 15 - 修改文件属性。
extern int sys_chown();				// 16 - 修改文件宿主和所属组。
extern int sys_break();				// 17 -
extern int sys_stat();				// 18 - 使用路径名取文件状态信息。
extern int sys_lseek();				// 19 - 重新定位读/写文件偏移。
extern int sys_getpid();			// 20 - 取进程id。
extern int sys_mount();				// 21 - 安装文件系统。
extern int sys_umount();			// 22 - 卸载文件系统。
extern int sys_setuid();			// 23 - 设置进程用户id。
extern int sys_getuid();			// 24 - 取进程用户id。
extern int sys_stime();				// 25 - 设置系统时间日期。
extern int sys_ptrace();			// 26 - 程序调试。
extern int sys_alarm();				// 27 - 设置报警。
extern int sys_fstat();				// 28 - 用文件句柄取文件状态信息。
extern int sys_pause();				// 29 - 暂停进程运行。
extern int sys_utime();				// 30 - 设置文件的访问和修改时间。
extern int sys_stty();				// 31 - 修改终端行设置。
extern int sys_gtty();				// 32 - 取终端行设置信息。
extern int sys_access();			// 32 - 检查用户对文件的访问权限。
extern int sys_nice();				// 34 - 设置进程执行优先权。
extern int sys_ftime();				// 35 - 取日期和时间。
extern int sys_sync();				// 36 - 同步高速缓冲与设备中数据。
extern int sys_kill();				// 37 - 终止一个进程。
extern int sys_rename();			// 38 - 更改文件名。
extern int sys_mkdir();				// 39 - 创建目录。
extern int sys_rmdir();				// 40 - 删除目录。
extern int sys_dup();				// 41 - 复制文件句柄。
extern int sys_pipe();				// 42 - 创建管道。
extern int sys_times();				// 43 - 取运行时间。
extern int sys_prof();				// 44 - 程序执行时间区域。
extern int sys_brk();				// 45 - 修改数据段长度。
extern int sys_setgid();			// 46 - 设置进程组id。
extern int sys_getgid();			// 47 - 取进程组id。
extern int sys_signal();			// 48 - 信号处理。
extern int sys_geteuid();			// 49 - 取进程有效用户id。
extern int sys_getegid();			// 50 - 取进程有效组id。
extern int sys_acct();				// 51 - 进程记账。
extern int sys_phys();				// 52 - 映射物理内存到进程空间。
extern int sys_lock();				// 53 -
extern int sys_ioctl();				// 54 - 设备输入输出控制。
extern int sys_fcntl();				// 55 - 文件句柄控制操作。
extern int sys_mpx();				// 56 -
extern int sys_setpgid();			// 57 - 设置进程组id。
extern int sys_ulimit();			// 58 - 统计进程使用资源情况。
extern int sys_uname();				// 59 - 显示系统信息。
extern int sys_umask();				// 60 - 取默认文件创建属性码。
extern int sys_chroot();			// 61 - 改变根目录。
extern int sys_ustat();				// 62 - 取文件系统信息。
extern int sys_dup2();				// 63 - 复制文件句柄。
extern int sys_getppid();			// 64 - 取父进程id。
extern int sys_getpgrp();			// 65 - 取组id，等于getpgid(0)。
extern int sys_setsid();			// 66 - 在新会话中运行程序。
extern int sys_sigaction();			// 67 - 改变信号处理过程。
extern int sys_sgetmask();			// 68 - 取信号屏蔽码。
extern int sys_ssetmask();			// 69 - 设置信号屏蔽码。
extern int sys_setreuid();			// 70 - 设置真实与/或有效用户id。
extern int sys_setregid();			// 71 - 设置真实与/或有效组id。
extern int sys_sigsuspend();			// 72 - 使用新屏蔽码挂起进程。
extern int sys_sigpending();			// 73 - 检查暂未处理的信号。
extern int sys_sethostname();			// 74 - 设置主机名。
extern int sys_setrlimit();			// 75 - 设置资源使用限制。
extern int sys_getrlimit();			// 76 - 取得进程使用资源的限制。
extern int sys_getrusage();			// 77 - 取得资源利用信息。
extern int sys_gettimeofday();			// 78 - 获取当日时间
extern int sys_settimeofday();			// 79 - 设置当日时间。
extern int sys_getgroups();			// 80 - 取得进程所有组标识号。
extern int sys_setgroups();			// 81 - 设置进程组标识号数组。
extern int sys_select();			// 82 - 等待文件描述符状态改变。
extern int sys_symlink();			// 83 - 建立符号链接。
extern int sys_lstat();				// 84 - 取符号链接文件状态。
extern int sys_readlink();			// 85 - 读取符号链接文件信息。
extern int sys_uselib();			// 86 - 选择共享库。


typedef int (*fn_ptr)();			// 本来定义在sched.h中

// 系统调用函数指针表。用于秕调用中断处理程序（int 0x80），作为跳转表。
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
sys_nice, sys_ftime, sys_sync,sys_kill, sys_rename, sys_mkdir,
sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid, sys_setregid, sys_sigsuspend, sys_sigpending, sys_sethostname,
sys_setrlimit, sys_getrlimit, sys_getrusage, sys_gettimeofday,
sys_settimeofday, sys_getgroups, sys_setgroups, sys_select, sys_symlink,
sys_lstat, sys_readlink, sys_uselib };

/* So we don't have to do any more manual updating.... */
/* 下面这样定义后，我们就无需手工更新系统调用数目了 */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);

