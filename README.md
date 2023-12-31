

# 《Linux 内核0.12 完全注释》学习笔记

代码中的注释几乎都来源于[《Linux 内核0.12 完全注释》](http://www.oldlinux.org/download/CLK-5.0-WithCover.pdf)这本书中。为了能在现代的操作系统下使用GCC进行编译，对部分代码进行了修改（主要是一些语法的改变）。

---
### 使用的GCC的版本
* 安装：`brew install x86_64-elf-binutils x86_64-elf-gcc`
* 版本信息：
```sh
# x86_64-elf-gcc --version
x86_64-elf-gcc (GCC) 13.1.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# x86_64-elf-ld --version
GNU ld (GNU Binutils) 2.40
Copyright (C) 2023 Free Software Foundation, Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License version 3 or (at your option) a later version.
This program has absolutely no warranty.
```


### GCC如果编译没有指定优化，内联函数不会内联
[gcc文档](https://gcc.gnu.org/onlinedocs/gcc/Inline.html): GCC does not inline any functions when not optimizing unless you specify the ‘always_inline’ attribute for the function, .....


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


### do_execve中内核为何可以直接访问为get_free_page分配的内存空间
`copy_string()`复制字符串到进程的参数和环境空间中，其实是复制到page指向的32个内存页中。这32个内存页事先也并没有分配，而是在`copy_string()`中通过调用`get_free_page()`进行分配，并保存返回的地址到page数组中。而`get_free_page()`返回的实际是物理地址，但是在`copy_string()`函数中却能直接访问该内存地址，这是因为内存在初始化时将16MB的物理地址与16MB的逻辑地址一一映射，所以在内核代码中逻辑地址和物理地址相同。这部分的代码在head.s的`setup_paging`小节中。而保存在page中的页物理地址会在`change_ldt()`中映射到数据段的最项端，真正成为用户进程的内存。并且位于栈空间的上面。

### gcc生成的段码在`rep movsl`之前没有`cld`
在console.c中有一行代码`vc_cons[currcons] = vc_cons[0];	// 复制0号结构的参数。`在执行之后，vc_cons[0]的数据就被修改。调试了很久之后发现，这一行对应的汇编代码主要是`rep movsl %ds:(%esi),%es:(%edi)`但是在这一行之前没有`cld`设置数据复制的方向。而更巧的是，在head.s中使用了`std`命令。这就导致了数据复制的方向是递减的。

暂的办法是在，在head.s的`setup_paging`函数的的`ret`命令前面添加一条`cld`命令。console中的问题暂解决了，但是内核中还有一些地方使用了`std`指令，不知道这会不会有问题，还需要进进一步测试。


### 嵌入汇编的寄存器被编译器使用
下面段代码珍edx作为输出寄存器，但gcc非常大胆，它把edx作为一个基地址表示current，简直是灭顶之灾。
```cpp
#define _get_base(addr) ({ 	\
unsigned long __base; 		\
__asm__("movb %3,%%dh\n\t" 	/* 取[addr+7]处基址高16位的高8位（位31-24）-> DH */\
	"movb %2,%%dl\n\t"	/* 取[addr+4]处基址高16位的低8位（位23-16）-> DL */\
	"shll $16,%%edx\n\t"	/* 基地址高16位到EDX中高16位处 		        */\
	"movw %1,%%dx"		/* 取[addr+2]处基址低16位（位15-0）-> DX 	*/\
	:"=d" (__base)		/* 从而EDX中含有32位的段基地址 		        */\
	:"m" (*((addr)+2)), 	\
	 "m" (*((addr)+4)), 	\
	 "m" (*((addr)+7))); 	\
__base;})

// 取局部描述符表中ldt所指段描述符中的基地址。
#define get_base(ldt) _get_base(((char *) &(ldt)))

// do_execve()中使用这个宏
free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
```
编译之后get_base对应的汇编如下
```c++
// c336:   8b 15 20 a2 01 00       mov    0x1a220,%edx
// c33c:   8a b2 af 03 00 00       mov    0x3af(%edx),%dh
// c342:   8a 92 ac 03 00 00       mov    0x3ac(%edx),%dl
// c348:   c1 e2 10                shl    $0x10,%edx
// c34b:   66 8b 92 aa 03 00 00    mov    0x3aa(%edx),%dx
```
两个解决办法:
* 一个是添加将`"=d"`改成`"=&d"`，`&`指明别的参数不要使用这个寄存器。
* 另一个方法是在输入列表中为edx赋一个初始值0，表明edx在输入中也被使用了，gcc在其他地方就不要使用,但这会增加一条无用的赋值语句。


### gdb调试命令
* `p/x $pc` -- 十六进制显示pc寄存器值
* `info reg eax` -- 显示eax寄存器内容
* `set disassemble-next-line on` -- 显示汇编代码
* `disassemble` -- 反汇编
* `si` -- 单步执行汇编代码
* `target remote localhost:1234` -- 连接远程调试
* `layout regs` -- 显示寄存器窗口
* 文档：[Output formats](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_54.html)

emacs有gdbmode，有时间再看吧：[GDB 从裸奔到穿戴整齐](http://www.skywind.me/blog/archives/2036)


### qemu镜像格式转换
```sh
# 从source.vhd转换成destination.qcow2
qemu-img convert source.vhd -O qcow2 destination.qcow2
```

### `losetup`命令
* `sudo losetup -f` - 自动查找空闲的设备