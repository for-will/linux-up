
void put_ch()
{
	unsigned long pos = 0xb8000;
	char attr = 0x07;
	__asm__("movb %2,%%ah\n\t" // 写字符。
		"movw %%ax,%1\n\t"
		::"a" (20),
		 "m" (*(short *)pos),
		 "m" (attr)
		:/* "ax" */);
}

// x86_64-elf-gcc -m32 -Qn -fno-pic -fomit-frame-pointer -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables -S -o test6.s test6.c
/*
    .file   "test6.c"
    .text
    .globl  put_ch
    .type   put_ch, @function
put_ch:
    subl    $8, %esp
    movl    $753664, 4(%esp)
    movb    $7, 3(%esp)
    movl    4(%esp), %edx
    movl    $20, %eax
/APP
# 6 "test6.c" 1
    movb 3(%esp),%ah
    movw %ax,(%edx)

# 0 "" 2
/NO_APP
    nop
    addl    $8, %esp
    ret
    .size   put_ch, .-put_ch
 */
