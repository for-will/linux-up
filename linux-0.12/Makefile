AS	=nasm
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -Ttext 0 -e startup_32
CC	=x86_64-elf-gcc -m32 -march=i386 -g
CFLAGS	=-Wall -O -fomit-frame-pointer
CPP	=x86_64-elf-cpp -nostdinc -Iinclude

# start: linux-bootsect

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS	=kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH	=kernel/math/math.a
LIBS	=lib/lib.a

%.o: %.nasm
	nasm -f elf32 -o $*.o $<

.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $@ $<

all: tools/system


build-system:
	nasm -f elf32 -o head.o boot/head.nasm
	$(CC) $(CFLAGS) -nostdinc -Iinclude -Wno-main -c -o main.o init/main.c
	$(LD) $(LDFLAGS) head.o main.o  -o system
	x86_64-elf-objcopy -O binary -R .note -R .comment system kernel

main:
	$(CC) $(CFLAGS) -nostdinc -Iinclude -Wno-main -c -o main.o init/main.c

tools/system:   boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system

# boot/head.o: boot/head.nasm

kernel/math/math.a:
	(cd kernel/math; make)

kernel/blk_drv/blk_drv.a:
	(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv.a:
	(cd kernel/chr_drv; make)

kernel/kernel.o:
	(cd kernel; make)

mm/mm.o:
	(cd mm; make)

fs/fs.o:
	(cd fs; make)

lib/lib.a:
	(cd lib; make)

clean:
	rm -f tmp_make
	rm -f init/*.o tools/system tools/build boot/*.o
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c; do /bin/echo -n "init/";$(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile
	(cd kernel; make dep)
	(cd mm; make dep)
# 	(cd fs; make dep)


### Dependencies:
init/main.o: init/main.c include/unistd.h include/time.h include/linux/tty.h \
 include/termios.h include/sys/types.h include/linux/sched.h \
 include/linux/head.h include/linux/mm.h include/linux/kernel.h \
 include/signal.h include/linux/fs.h include/sys/param.h \
 include/sys/time.h include/sys/resource.h include/asm/system.h \
 include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h \
 include/string.h
init/main0.o: init/main0.c
