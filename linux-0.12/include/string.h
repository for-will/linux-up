#ifndef _STRING_H_
#define _STRING_H_

/* extern */ inline int strlen(const unsigned char * s)
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

#endif