#ifndef _ERRNO_H
#define _ERRNO_H

#define ERROR           99
#define EPERM            1
#define ENOENT		 2
#define ESRCH            3
#define EINTR            4
#define EIO		 5
#define EBADF		 9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES		13
#define ENOTBLK		15
#define EBUSY		16
#define EEXIST		17
#define EXDEV		18
#define ENODEV		19
#define ENOTDIR		20
#define EISDIR		21
#define EINVAL          22
#define ENOTTY		25
#define ENOSPC		28
#define ESPIPE		29
#define ENOSYS          38
#define ENOTEMPTY	39


/* Should never be seen by user programs */
#define ERESTARTSYS     512
#define ERESTARTNOINTR  513

#endif
