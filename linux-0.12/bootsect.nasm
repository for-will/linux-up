; SYS_SIZE is the number of clicks (16 bytes) to be loaded. 
; 0x3000 is 0x30000 bytes = 196kB, more than enough for current 
; versions of linux

; #include <linux/config.h> 
DEF_SYSSIZE     equ 0x3000      ; 默认系统模块长度。单位是节，每节16字节；
DEF_INITSEG     equ 0x9000      ; 默认本程序代码移动目的段位置；
DEF_SETUPSEG    equ 0x9020      ; 默认setup程序代码段位置；
DEF_SYSSEG      equ 0x1000      ; 默认从磁盘加载系统模块到内存的段位置。
SYSSIZE         equ DEF_SYSSIZE ; 编译连接后system模块的大小。

;
;   bootsect.s      (C) 1991 Linus Torvalds
;   modified by Drew Eckhardt
;
; bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
; iself out of the way to address 0x90000, and jumps there.
;
; It then loads 'setup' directly after itself (0x90200), and the system
; at 0x10000, using BIOS interrupts. 
;
; NOTE! currently system is at most 8*65536 bytes long. This should be no
; problem, even in the future. I want to keep it simple. This 512 kB
; kernel size should be enough, especially as this doesn't contain the
; buffer cache as in minix
;
; The loader has been made as simple as possible, and continuos
; read errors will result in a unbreakable loop. Reboot by hand. It
; loads pretty fast by getting whole sectors at a time whenever possible.

SETUPLEN    equ 4                   ; nr of setup-sectors
BOOTSEG     equ 0x07c0              ; original address of boot-sector
INITSEG     equ DEF_INITSEG         ; we move boot here - out of the way
SYSSEG      equ DEF_SYSSEG          ; system loaded at 0x10000 (65536).
ENDSEG      equ SYSSEG + SYSSIZE    ; where to stop loading

; ROOT_DEV & SWAP_DEV are now written by "build".
ROOT_DEV    equ 0                   ; 根文件系统设备使用与系统引导时同样的设备；
SWAP_DEV    equ 0                   ; 交换设备使用与系统引导时同样的设备；

start:
    mov ax, BOOTSEG                 ; 将ds段寄存器置为0x7c0
    mov ds, ax
    mov ax, INITSEG                 ; 将es段寄存器置为0x9000
    mov es, ax
    mov cx, 256
    sub si, si
    sub di, di
    rep movsw
    jmp INITSEG:go

go:
    mov ax, cs
    mov dx, 0xfef4                  ; arbitary value >> 512 - disk parm size
                                    ; 在这个位置上会放一个自建的驱动器参数表
                                    ; 0xff00 -12（参数表长度），即sp = 0xfef4
    mov ds, ax
    mov es, ax
    push ax

    mov ss, ax
    mov sp, dx

; Many BIOS's default disk parameter tables will not
; recognize multi-sector reads beyoud the maximum sector number
; specified in the default diskette parameter tables - this may
; mean 7 sectors in some cases.
;
; Since single sector reads are slow and out of the question,
; we must take care of this by creating new parameter tables
; (for the first disk) in RAM. We will set the maximum sector
; cout to 18 - the most we will encounter on an HD 1.44.
;
; Hight doesn't hurt. Low does.
; 
; Segments are as follows: ds=es=ss=cs - INITSEG,
;         fs = 0, gs = parameter table segment

    push 0
    pop fs
    mov bx, 0x78                    ; fs:bx is parameter table address

    lgs si, [fs:bx]                 ; gs:si is source

    mov di, dx                      ; es:di is destination ; dx=0xfef4
    mov cx, 6                       ; copy 12 bytes
    cld

    rep gs movsw                    ; rep movsw word ptr es:[di], word ptr gs:[si]

    mov di, dx
    mov byte [di+4], 18             ; patch sector count

    mov [fs:bx], di
    mov [fs:bx+2], es

    pop ax
    mov fs, ax
    mov gs, ax

    times 510-($-$$) db 0
    db 0x55, 0xaa