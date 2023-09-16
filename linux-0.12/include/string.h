#ifndef _STRING_H_
#define _STRING_H_

/* extern */ 
static inline int strncmp(const char * cs, const char * ct, int count)
{
register int __res __asm__("ax");	
__asm__("cld\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"scasb\n\t"
	"jne 3f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %%eax,%%eax\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%%eax\n\t"
	"jl 4f\n\t"
	"negl %%eax\n"
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count):/* "si","di","cx" */);
return __res;
}

/* extern */
inline int strlen(const unsigned char * s)
{
register int __res __asm__("cx");
__asm__("cld\n\t"
        "repne\n\t"
        "scasb\n\t"
        "notl %0\n\t"
        "decl %0"
        :"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):/* "di" */);
return __res;
}

/* extern */
inline void * memset(void * s, char c, int count)
{
	__asm__("cld\n\t"
		"rep\n\t"
		"stosb"
		::"a" (c),"D" (s),"c" (count)
		:/* "cx","di" */);	
	return s;
}

#endif
