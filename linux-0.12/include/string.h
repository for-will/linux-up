#ifndef _STRING_H_
#define _STRING_H_


/* extern inline char * strcpy(char * dest, const char * src) */
inline char * strcpy(unsigned char * dest, const unsigned char * src)
{
__asm__("cld\n"
 	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	::"S" (src),"D" (dest):/* "si","di","ax" */);	
return dest;
}

/* extern inline int strcmp(const char * cs, const char * ct) */
inline int strcmp(const unsigned char * cs, const unsigned char * ct)
{
register int __res __asm__("ax");	
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"scasb\n\t"
	"jne 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 3f\n"
	"2:\tmovl $1,%%eax\n\t"
	"jl 3f\n\t"
	"negl %%eax\n"
	"3:"
	:"=a" (__res):"D" (cs),"S" (ct):/* "si","di" */);
return __res;
}

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
