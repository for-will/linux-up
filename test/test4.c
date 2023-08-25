// 局部变量和函数参数都分配在栈上，所以可以使用局部变量作为函数调用的参数。
// 以下代码用于演示局部变量被作为pritnf()的参数。

#include <stdio.h>

int main()
{
	int a = 100;
	int b = 200;
	int c = a + b;

	printf("%d + %d = %d\n");

	return 0;
}

/// 使用gcc编译得到汇编代码：
// gcc -m32 -Qn -fno-pic -fomit-frame-pointer -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables -S -o test4.s test.c

/*
    .file   "test4.c"
    .text
    .section    .rodata
.LC0:
    .string "%d + %d = %d\n"
    .text
    .globl  main
    .type   main, @function
main:
    subl    $12, %esp
    movl    $100, (%esp)
    movl    $200, 4(%esp)
    movl    (%esp), %edx
    movl    4(%esp), %eax
    addl    %edx, %eax
    movl    %eax, 8(%esp)
    pushl   $.LC0
    call    printf
    addl    $4, %esp
    movl    $0, %eax
    addl    $12, %esp
    ret
    .size   main, .-main
    .section    .note.GNU-stack,"",@progbits
 */

/// 编译运行： 
// gcc -m32 -Qn -fno-pic -fomit-frame-pointer -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables -o test4.out test4.c
// ./test4.out
/*
100 + 200 = 300
 */

/// 编译选项：
// -mpreferred-stack-boundary=2  
// 	栈使用4字节对齐（2^2）
/*
-mpreferred-stack-boundary=num
           Attempt to keep the stack boundary aligned to a 2 raised to num byte boundary.  If
           -mpreferred-stack-boundary is not specified, the default is 4 (16 bytes or
           128-bits).

           Warning: If you use this switch, then you must build all modules with the same
           value, including any libraries.  This includes the system libraries and startup
           modules.

 */
