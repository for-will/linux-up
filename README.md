


### GCC如果编译没有指定优化，内联函数不会内联
[gcc文档](https://gcc.gnu.org/onlinedocs/gcc/Inline.html): GCC does not inline any functions when not optimizing unless you specify the ‘always_inline’ attribute for the function, like this:


### `strlen`、`strcpy`等string.h中的函数是`builtin-declaration`
test2.c:
```c
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

// 另一个与strlen实现相同但函数名不同
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
  19:	57                   	push   %edi
  1a:	53                   	push   %ebx
  1b:	bf 00 00 00 00       	mov    $0x0,%edi
  20:	b8 00 00 00 00       	mov    $0x0,%eax
  25:	bb ff ff ff ff       	mov    $0xffffffff,%ebx
  2a:	89 d9                	mov    %ebx,%ecx
  2c:	fc                   	cld
  2d:	f2 ae                	repnz scas %es:(%edi),%al
  2f:	f7 d1                	not    %ecx
  31:	49                   	dec    %ecx
  32:	89 ca                	mov    %ecx,%edx
  34:	89 d9                	mov    %ebx,%ecx
  36:	fc                   	cld
  37:	f2 ae                	repnz scas %es:(%edi),%al
  39:	f7 d1                	not    %ecx
  3b:	49                   	dec    %ecx
  3c:	8d 04 11             	lea    (%ecx,%edx,1),%eax
  3f:	5b                   	pop    %ebx
  40:	5f                   	pop    %edi
  41:	c3                   	ret
```
因为strlen与c标准函数声明相同，所以会多一个非内联的strlen函数定义（实现）。string.h会被多个c文件包含，所以同一个函数会在每个包含该头文件的c文件中有一个相应的定义（实现），这在链接的时候会报错`multiple definition`。因此我把include/string.h中的所有函数的前面都加上了一个下划线。


### Ubuntu 64上的GCC编译32位程序

添加`-m32`选项,并安装:

```sh 
sudo apt-get install build-essential module-assistant  
sudo apt-get install gcc-multilib g++-multilib  
```

### 返回值类型为`volatile void`的函数

在内核代码中有的函数返回值类型为`volatile void`表示该函数不会返回，当函数中有死循环或者函数中直接退出的进程的时候，函数就不会再返回了。比如`volatile void do_exit(long error_code)`，现在的gcc貌似不支持这样写了，而是在函数声明中添加标注。相应的函数声明为：`void do_exit(long error_code) __attribute__((noreturn));`。


### 对于目录文件，可执行表示可以进入目录
>对于目录文件，可执行表示可以进入目录


### 为什么会出现m_inode->i_count=0但是m_inode->i_nlinks>0的m_inode??
i_count是inode运行时的引用计数,其值等于0时，表示这个m_inode没有进程或程序在使用，如果其中的数据已经回写到了硬盘中（i_dirt=0）则该m_inode可以用来加载其他的inode节点。m_inode只是硬盘中inode数据的在内存中的一个缓存，i_count只在内存中有效，而不会保存到硬盘中。而i_nlinks表示了当前节点在文件系统中被引用的次数，如果i_nlinks=0时，表示该节点已经完全被删除，相应的数据块和inode节点都会被回收。


### `void read_inode(struct m_inode * inode)`读取指定i节点信息
这里会锁定i节点。结尾会解锁i节点。


### `void write_inode(struct m_inode * inode)`将i节点信息写入缓冲区中
开始时锁定inode，结尾处解锁inode。将inode复制到buffer中后，inode清除脏标记，而对应的buffer被标脏。


### super_block中的内存分配和回收
在`read_super`中为`s_imap`和`s_zmap`分配的buffer，将会在`put_super`中进行释放。
