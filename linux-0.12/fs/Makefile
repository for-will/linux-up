LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= bitmap.o buffer.o exec.o inode.o super.o truncate.o

fs.o: $(OBJS)
	$(LD)  $(LDFLAGS) -r -o fs.o $(OBJS)

clean:
	rm -f *.o *.a tmp_make
	(for i in *.c;do rm -f `basename $$i .c`.s;done)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do $(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
bitmap.o: bitmap.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
buffer.o: buffer.c ../include/sys/types.h ../include/linux/fs.h \
 ../include/stdarg.h ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/asm/io.h
exec.o: exec.c
inode.o: inode.c ../include/linux/fs.h ../include/sys/types.h
super.o: super.c ../include/linux/fs.h
truncate.o: truncate.c ../include/linux/fs.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/types.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/sys/stat.h
