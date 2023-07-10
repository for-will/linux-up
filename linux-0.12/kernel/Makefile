AS	=x86_64-elf-as --32
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= asm.o exit.o mktime.o printk.o sched.o sys_call.o traps.o vsprintf.o

kernel.o: $(OBJS)
	$(LD) $(LDFLAGS) -r -o kernel.o $(OBJS)
	sync

clean:
	rm -f core *.o *.a tmp_make keyboard.s
	for i in *.c; do rm -f `basename $$i .c`.s;done
	(cd chr_drv; make clean)
	(cd blk_drv; make clean)
# 	(cd math; make clean)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do /bin/echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd chr_drv; make dep)
	(cd blk_drv; make dep)

### Dependencies:
exit.s exit.o: exit.c
mktime.s mktime.o: mktime.c ../include/time.h
printk.s printk.o: printk.c
sched.s sched.o: sched.c ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/sys/types.h
traps.s traps.o: traps.c ../include/string.h ../include/linux/head.h \
 ../include/linux/sched.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/asm/system.h ../include/asm/segment.h ../include/asm/io.h
vsprintf.s vsprintf.o: vsprintf.c ../include/stdarg.h