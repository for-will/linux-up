#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

/* 
 * Definition of struct rusage taken from BSD 4.3 Reno
 * 
 * We don't support all of these yet, but we might as well have them....
 * Otherwise, each time we add new items, programs which depend on this
 * structure will lose.  This reduces the chances of that happening.
 */
#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN -1

struct rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;              /* maximum resident set size */
        long    ru_ixrss;               /* intergral shared memory size */
        long    ru_idrss;               /* intergral unshared data size */
        long    ru_isrss;               /* intergral unshared stack size */
        long    ru_minflt;              /* page reclaims */
        long    ru_majflt;              /* page faults */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcws;              /* involuntary " */
};

#define RLIM_NLIMITS    6

struct rlimit {
        int     rlim_cur;
        int     rlim_max;
};

#endif /* _SYS_RESOURCE_H */