LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= bitmap.o buffer.o exec.o file_table.o inode.o namei.o super.o truncate.o \
	block_dev.o file_dev.o pipe.o char_dev.o read_write.o open.o stat.o

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
block_dev.o: block_dev.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/errno.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/asm/system.h
buffer.o: buffer.c ../include/sys/types.h ../include/linux/fs.h \
 ../include/stdarg.h ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/asm/io.h
char_dev.o: char_dev.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/errno.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/asm/io.h
exec.o: exec.c ../include/sys/types.h ../include/signal.h \
 ../include/errno.h ../include/string.h ../include/sys/stat.h \
 ../include/a.out.h ../include/linux/fs.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h
file_dev.o: file_dev.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/errno.h ../include/fcntl.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
file_table.o: file_table.c ../include/linux/fs.h ../include/sys/types.h
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
open.o: open.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/termios.h ../include/string.h ../include/errno.h \
 ../include/fcntl.h ../include/utime.h ../include/sys/stat.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/linux/tty.h \
 ../include/asm/segment.h
pipe.o: pipe.c ../include/const.h ../include/fcntl.h \
 ../include/linux/fs.h ../include/sys/types.h ../include/signal.h \
 ../include/errno.h ../include/termios.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h
read_write.o: read_write.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/sys/stat.h ../include/errno.h ../include/linux/kernel.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
stat.o: stat.c ../include/sys/types.h ../include/errno.h \
 ../include/sys/stat.h ../include/linux/fs.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
super.o: super.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/errno.h \
 ../include/sys/stat.h
truncate.o: truncate.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/sys/stat.h
