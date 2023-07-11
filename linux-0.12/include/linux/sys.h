/*
 * linux/include/sys.h
 */

extern int sys_fork();


typedef int (*fn_ptr)();

fn_ptr sys_call_table[] = {
        sys_fork,
};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);