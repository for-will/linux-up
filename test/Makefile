AS	=x86_64-elf-as --32
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	= /usr/local/bin/x86_64-elf-gcc
CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -I../include -m32 -march=i386
CPP	=x86_64-elf-gcc -E -nostdinc -I../include


test:
	gcc -c test1.c
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	rm -f *.o *.out

.PHONY: clean
