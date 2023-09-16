AS	=x86_64-elf-as --32
AR	=x86_64-elf-ar
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $@ $<

OBJS	= memory.o page.o swap.o

all: mm.o

mm.o: $(OBJS)
	$(LD) $(LDFLAGS) -r -o mm.o $(OBJS)


clean:
	rm -f *.o *.a tmp_make
	(for i in *.c; do rm -f `basename $$i .c`.s;done)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
memory.o: memory.c ../include/linux/fs.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/types.h \
 ../include/asm/system.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
swap.o: swap.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
