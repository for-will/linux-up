AS	=x86_64-elf-as
AR	=x86_64-elf-ar
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<
.s.o:
	$(AS) --32 -c -o $@ $<

OBJS	= console.o keyboard.o pty.o rs_io.o serial.o tty_io.o tty_ioctl.o

chr_drv.a: $(OBJS)
	$(AR) rcs chr_drv.a $(OBJS)
	sync

keyboard_cpp.s: keyboard.S
	$(CPP) -traditional keyboard.S -o keyboard_cpp.s

keyboard.o: keyboard_cpp.s
	$(AS) --32 -c -o $@ $<

clean:
	rm -f *.o *.a tmp_make keyboard_cpp.s
	(for i in *.c;do rm -f `basename $$i .c`.s;done)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do /bin/echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
console.s console.o: console.c ../../include/linux/sched.h \
 ../../include/sys/types.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/linux/fs.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/linux/tty.h \
 ../../include/termios.h ../../include/linux/config.h \
 ../../include/asm/io.h ../../include/asm/system.h \
 ../../include/asm/segment.h ../../include/string.h \
 ../../include/stddef.h ../../include/errno.h
pty.s pty.o: pty.c ../../include/linux/kernel.h ../../include/linux/tty.h \
 ../../include/termios.h ../../include/sys/types.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/signal.h ../../include/linux/fs.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/asm/system.h \
 ../../include/asm/io.h
serial.s serial.o: serial.c ../../include/linux/tty.h ../../include/termios.h \
 ../../include/sys/types.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/linux/fs.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/asm/system.h ../../include/asm/io.h
tty_io.s tty_io.o: tty_io.c ../../include/sys/types.h ../../include/termios.h \
 ../../include/ctype.h ../../include/errno.h ../../include/signal.h \
 ../../include/unistd.h ../../include/sys/types.h ../../include/stddef.h \
 ../../include/sys/stat.h ../../include/sys/time.h \
 ../../include/sys/times.h ../../include/sys/utsname.h \
 ../../include/sys/param.h ../../include/sys/resource.h \
 ../../include/utime.h ../../include/linux/kernel.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/mm.h ../../include/linux/fs.h \
 ../../include/linux/tty.h ../../include/asm/segment.h \
 ../../include/asm/system.h
tty_ioctl.s tty_ioctl.o: tty_ioctl.c ../../include/linux/fs.h \
 ../../include/sys/types.h ../../include/signal.h ../../include/errno.h \
 ../../include/termios.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/linux/tty.h ../../include/asm/io.h \
 ../../include/asm/segment.h ../../include/asm/system.h
