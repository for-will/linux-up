AS	=nasm
AR	=x86_64-elf-ar
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= floppy.o hd.o ll_rw_blk.o ramdisk.o

blk_drv.a: $(OBJS)
	$(AR) rcs blk_drv.a $(OBJS)
	sync

clean:
	rm -f *.o *.a tmp_make
	(for i in *.c;do rm -f `basename $$i .c`.s;done)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do /bin/echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
floppy.s floppy.o: floppy.c ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/types.h \
 ../../include/linux/fs.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/linux/fdreg.h ../../include/asm/system.h \
 ../../include/asm/io.h ../../include/asm/segment.h blk.h
hd.s hd.o: hd.c ../../include/linux/config.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/sys/types.h ../../include/linux/fs.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/linux/hdreg.h \
 ../../include/asm/system.h ../../include/asm/io.h \
 ../../include/asm/segment.h blk.h
ll_rw_blk.s ll_rw_blk.o: ll_rw_blk.c ../../include/errno.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/types.h \
 ../../include/linux/fs.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/asm/system.h blk.h
ramdisk.s ramdisk.o: ramdisk.c ../../include/string.h ../../include/linux/config.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/types.h \
 ../../include/linux/fs.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/asm/system.h ../../include/asm/segment.h \
 ../../include/asm/memory.h blk.h
