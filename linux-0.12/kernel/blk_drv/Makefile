AS	=nasm
AR	=x86_64-elf-ar
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -s -x
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -fomit-frame-pointer -finline-functions -nostdinc -I../../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../../include

.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS	= floppy.o hd.o ll_rw_blk.o

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
floppy.s floppy.o: floppy.c
hd.s hd.o: hd.c
ll_rw_blk.s ll_rw_blk.o: ll_rw_blk.c
