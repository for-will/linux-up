
// 与<string.h>中同名的strlen
inline int strlen(const char * s)
{
register int __res __asm__("cx");
__asm__("cld\n\t"
        "repne\n\t"
        "scasb\n\t"
        "notl %0\n\t"
        "decl %0"
        :"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):/* "di" */);
return __res;
};

// 另一个与strlen实现相同但函数名不再
inline int _strlen(const char * s)
{
register int __res __asm__("cx");
__asm__("cld\n\t"
        "repne\n\t"
        "scasb\n\t"
        "notl %0\n\t"
        "decl %0"
        :"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):/* "di" */);
return __res;
};

int test()
{
	const char * s;
	s = "aaabb";
	int n = 100;
	if (n != 100)
		s = "aa";
        int a = strlen(s);
	int b = _strlen(s);

        return a + b;
}
