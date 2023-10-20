AR	=x86_64-elf-ar
AS	=x86_64-elf-as
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -static
CC	=x86_64-elf-gcc -m32 -march=i386
# CFLAGS	=-Wall -O -fomit-frame-pointer -finline-functions -nostdinc -fno-builtin -I../include
CFLAGS	=-Wall -O0 -nostdinc -fno-builtin -I../include
CPP	=x86_64-elf-gcc -E -nostdinc -I../include
DBGFLAGS=-g

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS	=kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH	=kernel/math/math.a
LIBS	=lib/lib.a

%.o: %.s
	$(AS) --32 -c -o \
	$@ $<

%.o: 
	$(CC) $(DBGFLAGS) $(CFLAGS) -c -o \
	$@ $<

## kernel/math/math.a
MATHOBJS = kernel/math/error.o 			\
	kernel/math/math_emulate.o

## kernel/blk_drv/blk_drv.a
BLKDRVOBJS = kernel/blk_drv/ll_rw_blk.o		\
	kernel/blk_drv/floppy.o			\
	kernel/blk_drv/hd.o			\
	kernel/blk_drv/ramdisk.o		\

## kernel/chr_drv/chr_drv.a
CHRDRVOBJS = kernel/chr_drv/console.o		\
	kernel/chr_drv/keyboard.o		\
	kernel/chr_drv/pty.o			\
	kernel/chr_drv/rs_io.o			\
	kernel/chr_drv/serial.o			\
	kernel/chr_drv/tty_io.o			\
	kernel/chr_drv/tty_ioctl.o

## kernel/kernel.o
KERNELOBJS = kernel/asm.o			\
	kernel/exit.o				\
	kernel/fork.o				\
	kernel/mktime.o				\
	kernel/panic.o				\
	kernel/printk.o				\
	kernel/sched.o				\
	kernel/signal.o				\
	kernel/sys.o				\
	kernel/sys_call.o			\
	kernel/traps.o				\
	kernel/vsprintf.o

## mm/mm.o
MMOBJS = mm/memory.o mm/page.o mm/swap.o

## fs/fs.o
FSOBJS = fs/bitmap.o 				\
	fs/buffer.o 				\
	fs/exec.o				\
	fs/file_table.o				\
	fs/inode.o				\
	fs/namei.o				\
	fs/super.o				\
	fs/truncate.o 				\
	fs/block_dev.o				\
	fs/file_dev.o				\
	fs/pipe.o				\
	fs/char_dev.o				\
	fs/read_write.o				\
	fs/open.o				\
	fs/stat.o				\
	fs/fcntl.o				\
	fs/ioctl.o 				\
	fs/select.o

## lib/lib.a
LIBOBJS = lib/_exit.o				\
	lib/close.o				\
	lib/ctype.o				\
	lib/dup.o				\
	lib/errno.o				\
	lib/execve.o				\
	lib/open.o				\
	lib/setsid.o				\
	lib/wait.o				\
	lib/write.o				\
	lib/malloc.o				\
	lib/string.o

.PYONY: build

build: bin/system
	@ls -l bin/system	

## run
qemu: img/Image
	qemu-system-i386 \
	-boot order=a \
	-drive file=img/Image,if=floppy,format=raw \
	-drive file=img/rootimage-0.12-hd,if=ide,format=raw \
	-hdb img/rootimage.qcow2 \
	-k en-us 

## debug
gqemu: img/Image
	qemu-system-i386 \
	-boot order=a \
	-drive file=img/Image,if=floppy,format=raw \
	-drive file=img/rootimage-0.12-hd,if=ide,format=raw \
	-hdb img/rootimage.qcow2 \
	-s -S -k en-us

## build
img/Image: bin/bootsect.bin bin/setup.bin bin/kernel
	dd if=bin/bootsect.bin of=img/Image bs=512 count=1 conv=notrunc
	dd if=bin/setup.bin    of=img/Image bs=512 seek=1 conv=notrunc
	dd if=bin/kernel       of=img/Image bs=512 seek=5 conv=notrunc

bin/kernel: bin/system
	x86_64-elf-objcopy -O binary -R .note -R .comment bin/system bin/kernel

bin/bootsect.bin: ../boot/bootsect.nasm
	nasm -o $@ $<

bin/setup.bin: ../boot/setup.nasm
	nasm -o $@ $<

bin/system: boot/head.o init/main.o $(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) -Ttext 0 -e startup_32	\
	boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o bin/system

boot/head.o: ../boot/head.nasm
	nasm $(DBGFLAGS) -f elf32 -o $@ $<

kernel/math/math.a: $(MATHOBJS)
	$(AR) rcs $@ $^

kernel/blk_drv/blk_drv.a: $(BLKDRVOBJS)
	$(AR) rcs $@ $^

kernel/chr_drv/rs_io.o: ../kernel/chr_drv/rs_io.s
	$(AS) --32 -c -o $@ $<

kernel/chr_drv/keyboard.o: kernel/chr_drv/keyboard.s
	$(AS) --32 -c -o $@ $<

kernel/chr_drv/keyboard.s: ../kernel/chr_drv/keyboard.S
	$(CPP) -traditional $< -o $@

kernel/chr_drv/chr_drv.a: $(CHRDRVOBJS)
	$(AR) rcs $@ $^
	sync

kernel/asm.o: ../kernel/asm.s
kernel/sys_call.o: ../kernel/sys_call.s
kernel/kernel.o: $(KERNELOBJS)
	$(LD) $(LDFLAGS) -r -o \
		$@ $^
	sync

mm/page.o: ../mm/page.s
mm/mm.o: $(MMOBJS)
	$(LD) $(LDFLAGS) -r -o \
		$@ $^
	sync

fs/fs.o: $(FSOBJS)
	$(LD) $(LDFLAGS) -r -o \
		$@ $^
	sync

lib/lib.a: $(LIBOBJS)
	$(AR) rcs $@ $^
	sync


## clean
clean:
	rm -f *.o *.a tmp_make
	(for i in *.c;do rm -f `basename $$i .c`.s;done)
	rm -rf boot/ init/ kernel/ mm/ fs/ lib/ bin/
	mkdir -p boot/ init/ kernel/math kernel/blk_drv/ kernel/chr_drv/ mm/ fs/ lib/ bin/

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in ../init/*.c; do \
		/bin/echo -n "init/"; \
		$(CPP) -M $$i; done) >> tmp_make
	(for i in ../kernel/math/*.c;do \
		/bin/echo -n "kernel/math/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../kernel/blk_drv/*.c;do \
		/bin/echo -n "kernel/blk_drv/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../kernel/chr_drv/*.c;do \
		/bin/echo -n "kernel/chr_drv/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../kernel/*.c;do \
		/bin/echo -n "kernel/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../mm/*.c;do \
		/bin/echo -n "mm/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../fs/*.c;do \
		/bin/echo -n "fs/"; \
		$(CPP) -M $$i;done) >> tmp_make
	(for i in ../lib/*.c;do \
		/bin/echo -n "lib/"; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

## 使用for循环生成所有的deps
deps:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for d in init kernel/math kernel/blk_drv kernel/chr_drv kernel mm fs lib; do \
		for i in ../$$d/*.c; do \
			/bin/echo -n "$$d/"; \
			$(CPP) -M $$i; \
		done; \
	done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
init/main.o: ../init/main.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h \
 ../include/time.h ../include/linux/tty.h ../include/termios.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/asm/system.h ../include/asm/io.h ../include/stdarg.h \
 ../include/fcntl.h ../include/string.h
init/main0.o: ../init/main0.c
kernel/math/error.o: ../kernel/math/error.c
kernel/math/math_emulate.o: ../kernel/math/math_emulate.c
kernel/blk_drv/floppy.o: ../kernel/blk_drv/floppy.c ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/fdreg.h ../include/asm/system.h ../include/asm/io.h \
 ../include/asm/segment.h ../kernel/blk_drv/blk.h
kernel/blk_drv/hd.o: ../kernel/blk_drv/hd.c ../include/linux/config.h \
 ../include/linux/sched.h ../include/sys/types.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/linux/fs.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/linux/hdreg.h \
 ../include/asm/system.h ../include/asm/io.h ../include/asm/segment.h \
 ../kernel/blk_drv/blk.h
kernel/blk_drv/ll_rw_blk.o: ../kernel/blk_drv/ll_rw_blk.c ../include/errno.h \
 ../include/linux/sched.h ../include/sys/types.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/linux/fs.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h \
 ../kernel/blk_drv/blk.h
kernel/blk_drv/ramdisk.o: ../kernel/blk_drv/ramdisk.c ../include/string.h \
 ../include/stddef.h ../include/linux/config.h ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/system.h ../include/asm/segment.h ../include/asm/memory.h \
 ../kernel/blk_drv/blk.h
kernel/chr_drv/console.o: ../kernel/chr_drv/console.c ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/tty.h ../include/termios.h ../include/linux/config.h \
 ../include/asm/io.h ../include/asm/system.h ../include/asm/segment.h \
 ../include/string.h ../include/stddef.h ../include/errno.h
kernel/chr_drv/pty.o: ../kernel/chr_drv/pty.c ../include/linux/kernel.h \
 ../include/linux/tty.h ../include/termios.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/signal.h ../include/linux/fs.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/system.h \
 ../include/asm/io.h
kernel/chr_drv/serial.o: ../kernel/chr_drv/serial.c ../include/linux/tty.h \
 ../include/termios.h ../include/sys/types.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/linux/fs.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/system.h \
 ../include/asm/io.h
kernel/chr_drv/tty_io.o: ../kernel/chr_drv/tty_io.c ../include/sys/types.h \
 ../include/termios.h ../include/ctype.h ../include/errno.h \
 ../include/signal.h ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/time.h \
 ../include/sys/times.h ../include/sys/utsname.h ../include/sys/param.h \
 ../include/sys/resource.h ../include/utime.h ../include/linux/kernel.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/fs.h ../include/linux/tty.h ../include/asm/segment.h \
 ../include/asm/system.h
kernel/chr_drv/tty_ioctl.o: ../kernel/chr_drv/tty_ioctl.c ../include/linux/fs.h \
 ../include/sys/types.h ../include/signal.h ../include/errno.h \
 ../include/termios.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/linux/tty.h \
 ../include/asm/io.h ../include/asm/segment.h ../include/asm/system.h
kernel/exit.o: ../kernel/exit.c ../include/errno.h ../include/signal.h \
 ../include/sys/types.h ../include/sys/wait.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/linux/fs.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/linux/tty.h ../include/termios.h \
 ../include/asm/segment.h
kernel/fork.o: ../kernel/fork.c ../include/errno.h ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/asm/system.h
kernel/mktime.o: ../kernel/mktime.c ../include/time.h
kernel/panic.o: ../kernel/panic.c ../include/linux/kernel.h \
 ../include/linux/sched.h ../include/sys/types.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
kernel/printk.o: ../kernel/printk.c ../include/stdarg.h ../include/stddef.h
kernel/sched.o: ../kernel/sched.c ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/sys.h ../include/a.out.h ../include/unistd.h \
 ../include/sys/types.h ../include/stddef.h ../include/sys/stat.h \
 ../include/sys/times.h ../include/sys/utsname.h ../include/utime.h \
 ../include/linux/fdreg.h ../include/asm/system.h ../include/asm/io.h \
 ../include/asm/segment.h
kernel/signal.o: ../kernel/signal.c ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/errno.h
kernel/sys.o: ../kernel/sys.c ../include/errno.h ../include/linux/sched.h \
 ../include/sys/types.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/fs.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/tty.h ../include/termios.h ../include/linux/config.h \
 ../include/asm/segment.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/string.h ../include/stddef.h
kernel/traps.o: ../kernel/traps.c ../include/string.h ../include/stddef.h \
 ../include/linux/head.h ../include/linux/sched.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/linux/fs.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h \
 ../include/asm/segment.h ../include/asm/io.h
kernel/vsprintf.o: ../kernel/vsprintf.c ../include/stdarg.h ../include/string.h \
 ../include/stddef.h
mm/memory.o: ../mm/memory.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/asm/system.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
mm/swap.o: ../mm/swap.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/stddef.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h
fs/bitmap.o: ../fs/bitmap.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/stddef.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h
fs/block_dev.o: ../fs/block_dev.c ../include/linux/fs.h \
 ../include/sys/types.h ../include/errno.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h \
 ../include/asm/system.h
fs/buffer.o: ../fs/buffer.c ../include/sys/types.h ../include/linux/fs.h \
 ../include/stdarg.h ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/asm/io.h
fs/char_dev.o: ../fs/char_dev.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/errno.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/asm/io.h
fs/exec.o: ../fs/exec.c ../include/sys/types.h ../include/signal.h \
 ../include/errno.h ../include/string.h ../include/stddef.h \
 ../include/sys/stat.h ../include/a.out.h ../include/linux/fs.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
fs/fcntl.o: ../fs/fcntl.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/stddef.h ../include/errno.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/segment.h \
 ../include/fcntl.h ../include/sys/stat.h
fs/file_dev.o: ../fs/file_dev.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/errno.h ../include/fcntl.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
fs/file_table.o: ../fs/file_table.c ../include/linux/fs.h \
 ../include/sys/types.h
fs/inode.o: ../fs/inode.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/stddef.h ../include/sys/stat.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/system.h
fs/ioctl.o: ../fs/ioctl.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/string.h ../include/stddef.h ../include/errno.h \
 ../include/sys/stat.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
fs/namei.o: ../fs/namei.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/asm/segment.h \
 ../include/string.h ../include/stddef.h ../include/fcntl.h \
 ../include/errno.h ../include/const.h ../include/sys/stat.h
fs/open.o: ../fs/open.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/termios.h ../include/string.h ../include/stddef.h \
 ../include/errno.h ../include/fcntl.h ../include/utime.h \
 ../include/sys/stat.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/tty.h ../include/asm/segment.h
fs/pipe.o: ../fs/pipe.c ../include/const.h ../include/fcntl.h \
 ../include/sys/types.h ../include/linux/fs.h ../include/signal.h \
 ../include/errno.h ../include/termios.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h
fs/read_write.o: ../fs/read_write.c ../include/linux/fs.h \
 ../include/sys/types.h ../include/sys/stat.h ../include/errno.h \
 ../include/linux/kernel.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h
fs/select.o: ../fs/select.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/kernel.h ../include/linux/tty.h ../include/termios.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h \
 ../include/asm/system.h ../include/sys/stat.h ../include/string.h \
 ../include/stddef.h ../include/const.h ../include/errno.h
fs/stat.o: ../fs/stat.c ../include/sys/types.h ../include/errno.h \
 ../include/sys/stat.h ../include/linux/fs.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/segment.h
fs/super.o: ../fs/super.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h ../include/errno.h \
 ../include/sys/stat.h
fs/truncate.o: ../fs/truncate.c ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/signal.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/sys/stat.h
lib/_exit.o: ../lib/_exit.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
lib/close.o: ../lib/close.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
lib/ctype.o: ../lib/ctype.c ../include/ctype.h
lib/dup.o: ../lib/dup.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
lib/errno.o: ../lib/errno.c
lib/execve.o: ../lib/execve.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
lib/malloc.o: ../lib/malloc.c ../include/linux/kernel.h ../include/linux/mm.h \
 ../include/signal.h ../include/sys/types.h ../include/asm/system.h
lib/open.o: ../lib/open.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h \
 ../include/stdarg.h
lib/setsid.o: ../lib/setsid.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
lib/string.o: ../lib/string.c ../include/string.h ../include/stddef.h
lib/wait.o: ../lib/wait.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h \
 ../include/sys/wait.h
lib/write.o: ../lib/write.c ../include/unistd.h ../include/sys/types.h \
 ../include/stddef.h ../include/sys/stat.h ../include/sys/types.h \
 ../include/sys/time.h ../include/sys/times.h ../include/sys/utsname.h \
 ../include/sys/param.h ../include/sys/resource.h ../include/utime.h
