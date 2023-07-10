#define move_to_user_mode() \
__asm__("movl %%esp,%%eax\n\t" \
	"pushl $0x17\n\t" \
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl $0x0f\n\t" \
	"pushl $1f\n\t" \
	"iret\n" \
	"1:\tmovl $0x17,%%eax\n\t" \
	"mov %%ax,%%ds\n\t" \
	"mov %%ax,%%es\n\t" \
	"mov %%ax,%%fs\n\t" \
	"mov %%ax,%%gs" \
	:::"ax")
	
#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

#define _set_gate(gate_addr, type, dpl, addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000 + (dpl<<13) + (type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4 + (char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

#define set_intr_gate(n, addr) \
	_set_gate(&_idt[n], 14, 0, addr)

#define set_trap_gate(n, addr) \
	_set_gate(&_idt[n], 15, 0, addr)

#define set_system_gate(n, addr) \
	_set_gate(&_idt[n], 15, 3, addr)