#ifndef _SIGNAL_H
#define _SIGNAL_H

#define SIGKILL         9
#define SIGALRM         14
#define SIGSTOP         19

struct sigaction {
        // dummy
        void * dummy;
};

#endif /* _SIGNAL_H */