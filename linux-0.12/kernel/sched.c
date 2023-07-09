

#include <linux/sched.h>
#include <sys/types.h>	// defined NULL

unsigned long startup_time=0;

struct task_struct *current = NULL;
struct task_struct *last_task_used_math = NULL;

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };



void sched_init(void)
{
	//todo:
}