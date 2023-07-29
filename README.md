


### GCC如果编译没有指定优化，内联函数不会内联
[gcc文档](https://gcc.gnu.org/onlinedocs/gcc/Inline.html): GCC does not inline any functions when not optimizing unless you specify the ‘always_inline’ attribute for the function, like this:


### `char *`类型的参数会让内联失败
test2.c:
```c
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


int test()
{
        int a;
        a = strlen("aaabb");
        return a;
}
```

使用`x86_64-elf-gcc -m32 -march=i386 -Wall -O -fomit-frame-pointer -finline-functions -c -o test2.o test2.c`编译，然后`x86_64-elf-objdump -d test2.o`查看编译后的结果:

```
test2.o:     file format elf32-i386


Disassembly of section .text:

00000000 <strlen>:
   0:	57                   	push   %edi
   1:	b9 ff ff ff ff       	mov    $0xffffffff,%ecx
   6:	b8 00 00 00 00       	mov    $0x0,%eax
   b:	8b 7c 24 08          	mov    0x8(%esp),%edi
   f:	fc                   	cld
  10:	f2 ae                	repnz scas %es:(%edi),%al
  12:	f7 d1                	not    %ecx
  14:	49                   	dec    %ecx
  15:	89 c8                	mov    %ecx,%eax
  17:	5f                   	pop    %edi
  18:	c3                   	ret

00000019 <test>:
  19:	b8 05 00 00 00       	mov    $0x5,%eax
  1e:	c3                   	ret
```

将strlen声明为`inline int strlen(const unsigned char * s)`则能成功内联：
```

test2.o:     file format elf32-i386


Disassembly of section .text:

00000000 <test>:
   0:	57                   	push   %edi
   1:	b9 ff ff ff ff       	mov    $0xffffffff,%ecx
   6:	bf 00 00 00 00       	mov    $0x0,%edi
   b:	b8 00 00 00 00       	mov    $0x0,%eax
  10:	fc                   	cld
  11:	f2 ae                	repnz scas %es:(%edi),%al
  13:	f7 d1                	not    %ecx
  15:	49                   	dec    %ecx
  16:	89 c8                	mov    %ecx,%eax
  18:	5f                   	pop    %edi
  19:	c3                   	ret
```

