AR	=x86_64-elf-ar
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -fomit-frame-pointer -finline-functions -nostdinc -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $@ $<

OBJS	= _exit.o close.o ctype.o dup.o errno.o execve.o open.o setsid.o wait.o write.o \
	malloc.o

lib.a: $(OBJS)
	$(AR) rcs lib.a $(OBJS)
	sync

clean:
	rm -f *.o *.a tmp_make
	(for i in *.c;do rm -f `basename $$i .c`.s;done)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do /bin/echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
_exit.s _exit.o: _exit.c ../include/unistd.h ../include/sys/types.h
close.s close.o: close.c ../include/unistd.h ../include/sys/types.h
ctype.s ctype.o: ctype.c ../include/ctype.h
dup.s dup.o: dup.c ../include/unistd.h ../include/sys/types.h
errno.s errno.o: errno.c
execve.s execve.o: execve.c ../include/unistd.h ../include/sys/types.h
malloc.s malloc.o: malloc.c ../include/linux/kernel.h ../include/linux/mm.h \
 ../include/signal.h ../include/sys/types.h ../include/asm/system.h
open.s open.o: open.c
setsid.s setsid.o: setsid.c ../include/unistd.h ../include/sys/types.h \
 ../include/sys/types.h
wait.s wait.o: wait.c ../include/sys/wait.h ../include/sys/types.h
write.s write.o: write.c ../include/unistd.h ../include/sys/types.h \
 ../include/sys/types.h
