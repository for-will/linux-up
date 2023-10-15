#ifndef _HEAD_H
#define _HEAD_H

// 定义了段描述符数据结构。其中仅说明每个描述符是由8个字节构成，每个描述符表共有256项。
typedef struct desc_struct {
	unsigned long a, b;
} desc_table[256];

// 系统分页机制使用的内存页面数组，每项4字节。本内核将其放在物理地址0开始处。
extern unsigned long pg_dir[1024];	// 第项4字节，共1024项。
extern desc_table idt, gdt;		// 中断描述符表，全局描述符表。

#define GDT_NUL		0	/* 全局描述符表的第0项，不用 */
#define GDT_CODE	1	/* 第1项，是内核代码段描述符项 */
#define GDT_DATA	2	/* 第2项，是内核数据段描述符项 */
#define GDT_TMP		3	/* 第3项，系统段描述符，Linux没有使用 */

#define LDT_NUL		0	/* 每个局部描述符表的第0项，不用 */
#define LDT_CODE	1	/* 第1项，是用户程序代码段描述符项 */
#define LDT_DATA	2	/* 第2项，是用户程序数据段描述符项 */

#endif

