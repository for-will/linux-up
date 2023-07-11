AS	=nasm
LD	=x86_64-elf-ld
LDFLAGS	=-m elf_i386 -Ttext 0 -e startup_32
CC	=x86_64-elf-gcc -m32 -march=i386
CFLAGS	=-Wall -fomit-frame-pointer

# start: linux-bootsect

bochs:
	x86_64-elf-objcopy -O binary -R .note -R .comment linux-0.12/tools/system kernel
	nasm linux-0.12/boot/bootsect.nasm -o bootsect.bin
	nasm linux-0.12/boot/setup.nasm -o setup.bin
	dd if=bootsect.bin of=Image bs=512 count=1 conv=notrunc
	dd if=setup.bin of=Image bs=512 seek=1 conv=notrunc
	dd if=kernel of=Image bs=512 seek=5 conv=notrunc
	bochs -q -unlock

linux-0.12/tools/system:
	(cd linux-0.12/tools/system;make)

# ubuntu:
# 	nasm boot.S -o boot
# 	dd if=boot of=Image bs=512 count=1 conv=notrunc
# 	# gcc-12 -c -o head.o -m32 head.s
# 	as --32 -o head.o head.s
# 	ld -m elf_i386 -Ttext 0 -e startup_32 -x -s -o system head.o
# 	dd bs=512 if=system of=Image skip=8 seek=1 conv=notrunc
# 	bochs -q -unlock

# macos:
# 	nasm boot.S -o boot
# 	dd if=boot of=Image bs=512 count=1 conv=notrunc
# 	as -arch i386 -o head.o head.gas 
# 	dd bs=280 if=head.o of=head.bin skip=1
# 	dd bs=512 if=head.bin of=Image seek=1 conv=notrunc
# 	bochs -q -unlock
# 	# -m elf_i386 -Ttext 0 -e startup_32 -s -x -M 
	
# zero:
# 	nasm boot.S -o boot
# 	dd if=boot of=Image bs=512 count=1 conv=notrunc
# 	nasm -f bin -o head.bin head.nasm
# 	dd bs=512 if=head.bin of=Image seek=1 conv=notrunc
# 	bochs -q -unlock


hello:
	nasm bootsect.nasm -o bootsect.bin
	dd if=bootsect.bin of=Image bs=512 count=1 conv=notrunc
	bochs -q -unlock

linux-bootsect: build-system
	nasm linux-0.12/bootsect.nasm -o bootsect.bin
	nasm linux-0.12/setup.nasm -o setup.bin
	dd if=bootsect.bin of=Image bs=512 count=1 conv=notrunc
	dd if=setup.bin of=Image bs=512 seek=1 conv=notrunc
	dd if=kernel of=Image bs=512 seek=5 conv=notrunc
	bochs -q -unlock

head.o: linux-0.12/head.nasm 
	nasm -f elf32 -o head.o linux-0.12/head.nasm 

main.o: linux-0.12/init/main.c 
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $@ $<
# 	x86_64-elf-gcc -O -Wall -fstrength-reduce -fomit-frame-pointer -c -o $@ $<
# 	x86_64-elf-gcc -m32 -O -Wall -fstrength-reduce -fomit-frame-pointer -nostdinc -c -o main.o linux-0.12/init/main.c -Wno-main


test:
	x86_64-elf-gcc -m32 -O -Wall -fstrength-reduce -fomit-frame-pointer -nostdinc -c -o main.o linux-0.12/init/main.c

ld-system:
	x86_64-elf-ld -m elf_i386 -s -x head.o main.o -o system 
# 	x86_64-elf-gcc -s -x -M head.o main.o -o system
# 	x86_64-elf-ld -A elf32_i386 -m elf_i386 -e startup_32  -s -x head.o main.o -o system
# 	x86_64-elf-ld -A elf32_i386 -m elf_i386 -s -x head.o main.o -o system

qemu:
	qemu-system-i386 \
	-boot order=a \
	-drive file=Image,if=floppy,format=raw \
	-k en-us 

qemudb:
	qemu-system-i386 -boot order=a -drive file=Image,if=floppy,format=raw -s -S -k en-us