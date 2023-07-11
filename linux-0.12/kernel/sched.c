

#include <linux/sched.h>
#include <linux/sys.h>
#include <sys/types.h>	// defined NULL

unsigned long volatile jiffies = 0; 
unsigned long startup_time=0;


struct task_struct *current = NULL;
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {};//dummy

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

// dummy
void schedule()
{
	// TODO:
}

void math_state_restore()// dummy
{
	// todo:
}

void sched_init(void)
{
	//todo:
}

void do_timer(void)//dummy
{
	// todo
}