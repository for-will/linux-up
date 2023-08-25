AS	=x86_64-elf-as --32
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	= /usr/local/bin/x86_64-elf-gcc
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include -m32 -march=i386
CPP	=x86_64-elf-gcc -E -nostdinc -I../include
ASFLAGS	=-m32 -Qn -fno-pic -fomit-frame-pointer -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables


test:
	gcc -c test1.c
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.c.s:
	$(CC) $(ASFLAGS) -S -o $@ $<

clean:
	rm -f *.o *.out *.s

.PHONY: clean
