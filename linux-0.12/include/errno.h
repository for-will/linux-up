#ifndef _ERRNO_H
#define _ERRNO_H

#define ERROR           99
#define EPERM            1
#define ESRCH            3
#define EINTR            4
#define EIO		 5
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EINVAL          22
#define ENOTTY		25
#define ENOSYS          38


/* Should never be seen by user programs */
#define ERESTARTSYS     512
#define ERESTARTNOINTR  513

#endif
