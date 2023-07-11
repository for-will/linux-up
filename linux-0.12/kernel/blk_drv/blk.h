#ifndef _BLK_H
#define _BLK_H

#include <sys/types.h>

#if (MAJOR_NR == 2)
/* floppy */
#define DEVICE_INTR do_floppy

#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_TIMEOUT hd_timeout
#define DEVICE_INTR do_hd

#else
/* unknown blk device */
#error "unknown blk device"

#endif

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
#ifdef DEVICE_TIMEOUT
int DEVICE_TIMEOUT = 0;
#endif

#endif