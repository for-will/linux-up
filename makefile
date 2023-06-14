start: linux-bootsect

ubuntu:
	nasm boot.S -o boot
	dd if=boot of=Image bs=512 count=1 conv=notrunc
	# gcc-12 -c -o head.o -m32 head.s
	as --32 -o head.o head.s
	ld -m elf_i386 -Ttext 0 -e startup_32 -x -s -o system head.o
	dd bs=512 if=system of=Image skip=8 seek=1 conv=notrunc
	bochs -q -unlock

macos:
	nasm boot.S -o boot
	dd if=boot of=Image bs=512 count=1 conv=notrunc
	as -arch i386 -o head.o head.gas 
	dd bs=280 if=head.o of=head.bin skip=1
	dd bs=512 if=head.bin of=Image seek=1 conv=notrunc
	bochs -q -unlock
	# -m elf_i386 -Ttext 0 -e startup_32 -s -x -M 
	
zero:
	nasm boot.S -o boot
	dd if=boot of=Image bs=512 count=1 conv=notrunc
	nasm -f bin -o head.bin head.nasm
	dd bs=512 if=head.bin of=Image seek=1 conv=notrunc
	bochs -q -unlock


hello:
	nasm bootsect.nasm -o bootsect.bin
	dd if=bootsect.bin of=Image bs=512 count=1 conv=notrunc
	bochs -q -unlock

linux-bootsect:
	nasm linux-0.12/bootsect.nasm -o bootsect.bin
	dd if=bootsect.bin of=Image bs=512 count=1 conv=notrunc
	bochs -q -unlock