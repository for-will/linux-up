

#include <linux/sched.h>

unsigned long startup_time=0;

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

void sched_init(void)
{
	//todo:
}