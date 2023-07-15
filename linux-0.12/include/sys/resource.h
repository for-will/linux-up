#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#define RLIM_NLIMITS    6

struct rlimit {
        int     rlim_cur;
        int     rlim_max;
};

#endif /* _SYS_RESOURCE_H */