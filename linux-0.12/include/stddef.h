#ifndef _STDDEF_H
#define _STDDEF_H

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;				/* 两个指针相减结果的类型。 */
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;			/* sizeof返回的类型 */
#endif

#undef NULL
#define NULL ((void *)0)			/* 空指针 */

// 下面定义了一个计算某成员在类型中偏移位置的宏。使用该宏可以确定一个成员（字段）在包含
// 它的结构类型中从结构开始处算起的字节偏移量。宏的结果是类型为size_t的整数常数表达式。
// 这里有一个技巧用法，((TYPE *)0)是表示将琴整数类型0投身（type cast）成数据对象指针
// 类型，然后在该结果上进行运算。
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif

