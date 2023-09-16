	.file	"test0.c"
	.text
	.globl	total
	.section	.bss
	.align 4
	.type	total, @object
	.size	total, 4
total:
	.zero	4
	.data
	.align 4
	.type	count, @object
	.size	count, 4
count:
	.long	13
	.text
	.type	add, @function
add:
	movl	total, %edx
	movl	4(%esp), %eax
	addl	%edx, %eax
	movl	%eax, total
	movl	total, %edx
	movl	8(%esp), %eax
	addl	%edx, %eax
	movl	%eax, total
	movl	count, %eax
	addl	$1, %eax
	movl	%eax, count
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	addl	%edx, %eax
	ret
	.size	add, .-add
	.globl	call_add
	.type	call_add, @function
call_add:
	subl	$12, %esp
	movl	$100, 8(%esp)
	movl	$200, 4(%esp)
	pushl	4(%esp)
	pushl	12(%esp)
	call	add
	addl	$8, %esp
	movl	%eax, (%esp)
	movl	8(%esp), %edx
	movl	4(%esp), %eax
	addl	%edx, %eax
	cmpl	%eax, (%esp)
	jne	.L5
	movl	$0, (%esp)
.L5:
	nop
	addl	$12, %esp
	ret
	.size	call_add, .-call_add
