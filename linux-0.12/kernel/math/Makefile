AR	=x86_64-elf-ar
AS	=x86_64-elf-as --32
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -fomit-frame-pointer -finline-functions -nostdinc -I../../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../../include

.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= error.o math_emulate.o

math.a: $(OBJS)
	$(AR) rcs math.a $(OBJS)
	sync

clean:
	rm -f core *.o *.a tmp_make
	for i in *.c; do rm -f `basename $$i .c`.s;done

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do /bin/echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
error.s error.o: error.c
math_emulate.s math_emulate.o: math_emulate.c
