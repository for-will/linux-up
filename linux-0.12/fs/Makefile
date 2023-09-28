LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= bitmap.o buffer.o exec.o file_table.o inode.o namei.o super.o truncate.o \
	block_dev.o

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
block_dev.o: block_dev.c ../include/linux/fs.h ../include/errno.h \
 ../include/linux/sched.h ../include/sys/types.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/asm/system.h
buffer.o: buffer.c ../include/sys/types.h ../include/linux/fs.h \
 ../include/stdarg.h ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/asm/io.h
exec.o: exec.c
file_table.o: file_table.c ../include/linux/fs.h
inode.o: inode.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/sys/stat.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h
namei.o: namei.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/segment.h \
 ../include/string.h ../include/fcntl.h ../include/errno.h \
 ../include/const.h ../include/sys/stat.h
super.o: super.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/errno.h \
 ../include/sys/stat.h
truncate.o: truncate.c ../include/linux/fs.h ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/sys/stat.h
