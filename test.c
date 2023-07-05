#define outb_p(value,port) \
__asm__ ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#define CMOS_READ(addr) ({\
outb_p(0x80|addr, 0x70);               // 向端口0x70输出要读取的CMOS内存位置（0x80|addr）。 \
})
// outb_p(0x80|addr, 0x70); \              // 向端口0x70输出要读取的CMOS内存位置（0x80|addr）。
// inb_p(0x71); \                          // 从端口0x71读取1字节，返回该字节。
// })

void test(){

	unsigned char a = ({
		outb_p(0x80|2, 0x70);
		inb_p(0x71);
	});
}