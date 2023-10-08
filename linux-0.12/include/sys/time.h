#ifndef _SYS_TIME_H
#define _SYS_TIME_H

/* gettimofday returns this */
struct timeval {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* microseconds */
};

struct timezone {
        int     tz_minuteswest; /* minutes west of Greenwich */
        int     tz_dsttime;     /* type of dst correction */
};

#endif /* _SYS_TIME_H */
