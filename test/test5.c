// 什么是调用栈对齐？这段代码演示默认的调用栈对齐（2^4 = 16字节）的情况下，使用栈上的局部
// 变量调用变参函数 printf() 


/// gcc的编译选项： 
// -mpreferred-stack-boundary=num
//            Attempt to keep the stack boundary aligned to a 2 raised to num byte boundary.  If
//            -mpreferred-stack-boundary is not specified, the default is 4 (16 bytes or
//            128-bits).

//            Warning: If you use this switch, then you must build all modules with the same
//            value, including any libraries.  This includes the system libraries and startup
//            modules.

#include <stdio.h>

int main (int argc, char *argv[])
{
	int a = 100;
	int b = 200;
	int c = a + b;
	int d = 0;
	printf("%x|%x|%x| %d + %d = %d;\n", 1, 2, 3);
	return 0;
}

/// 编译&运行
/// gcc -m32 -o test4.out test5.c && ./test5.out
/// 输出：
/// 1|2|3| 100 + 200 = 300;


/// 编译后的汇编代码：
/*
	.file	"test5.c"
	.text
	.section	.rodata
.LC0:
	.string	"%x|%x|%x| %d + %d = %d;\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	leal	4(%esp), %ecx
	.cfi_def_cfa 1, 0
	andl	$-16, %esp
	pushl	-4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	.cfi_escape 0x10,0x5,0x2,0x75,0
	pushl	%ebx
	pushl	%ecx
	.cfi_escape 0xf,0x3,0x75,0x78,0x6
	.cfi_escape 0x10,0x3,0x2,0x75,0x7c
	subl	$16, %esp
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movl	$100, -24(%ebp)
	movl	$200, -20(%ebp)
	movl	-24(%ebp), %ecx
	movl	-20(%ebp), %edx
	addl	%ecx, %edx
	movl	%edx, -16(%ebp)
	movl	$0, -12(%ebp)
	pushl	$3
	pushl	$2
	pushl	$1
	leal	.LC0@GOTOFF(%eax), %edx
	pushl	%edx
	movl	%eax, %ebx
	call	printf@PLT
	addl	$16, %esp
	movl	$0, %eax
	leal	-8(%ebp), %esp
	popl	%ecx
	.cfi_restore 1
	.cfi_def_cfa 1, 0
	popl	%ebx
	.cfi_restore 3
	popl	%ebp
	.cfi_restore 5
	leal	-4(%ecx), %esp
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.section	.text.__x86.get_pc_thunk.ax,"axG",@progbits,__x86.get_pc_thunk.ax,comdat
	.globl	__x86.get_pc_thunk.ax
	.hidden	__x86.get_pc_thunk.ax
	.type	__x86.get_pc_thunk.ax, @function
__x86.get_pc_thunk.ax:
.LFB1:
	.cfi_startproc
	movl	(%esp), %eax
	ret
	.cfi_endproc
.LFE1:
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
*/


// 调用printf之前的堆栈如下：
// 	d = 0
// 	c = 300
// 	b = 200
// 	a = 100
// 	3
// 	2
// 	1
// 	"%x|%x|%x| %d + %d = %d;\n"
// 	
// 如果没有字义'int d = 0;'，则会因为栈的16字节对齐，而在栈中出现未初始化的4个字节，堆栈便是下面这样：
// 	c = 300
// 	b = 200
// 	a = 100
// 	UNINIT-VALUE
// 	3
// 	2
// 	1
// 	"%x|%x|%x| %d + %d = %d;\n"
// 	
// 同理，如果printf没有传参数 1,2,3 ，则也会因为16字节对齐，在栈上出现未初始化的值。
// 比如 printf("%x|%x|%x| %d + %d = %d;\n")对应的栈就是：
// 	d = 0
// 	c = 300
// 	b = 200
// 	a = 100
// 	UNINIT_VALUE
// 	UNINIT_VALUE
// 	UNINIT_VALUE
// 	"%x|%x|%x| %d + %d = %d;\n"
 	
