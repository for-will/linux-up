
void printk()
{

}

void _start(void)
{
	int a = 0x55aa;
	int b = 0xaa55;

	while(1)
	{
		a = a^b;
		b = a^b;
		a = a^b;
	}
}


#define PAGE_SIZE 4096

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
