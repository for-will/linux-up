// gcc -O 会把变参部分给省略掉，....
static int vsum(char * str, const char *fmt, ...)
{
	int a = *(int *)((char *)&fmt+4);
	int b = *(int *)((char *)&fmt+8);
	int c = a + b;
        return c;
}


int foo(){
	int sum = vsum("dd","%d,%d", 100, 200);
	return sum;
}
