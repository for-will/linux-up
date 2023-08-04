/* 测试内联汇编的编译 */

/// 编译命令:
/// x86_64-elf-gcc -m32 -march=i386 -Wall -O -fomit-frame-pointer -finline-functions -nostdinc -c -o test1.o test1.c
#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

unsigned char test()
{
        unsigned char s = inb_p(0x1f7);
        return s;
}

/// x86_64-elf-objdump -d test1.o
/*

test1.o:     file format elf32-i386


Disassembly of section .text:

00000000 <test>:
   0:   ba f7 01 00 00          mov    $0x1f7,%edx
   5:   ec                      in     (%dx),%al
   6:   eb 00                   jmp    8 <test+0x8>
   8:   eb 00                   jmp    a <test+0xa>
   a:   c3                      ret
*/

