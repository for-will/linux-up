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

SETUPLEN        equ 4                   ; nr of setup-sectors
BOOTSEG         equ 0x07c0              ; original address of boot-sector
INITSEG         equ DEF_INITSEG         ; we move boot here - out of the way
SETUPSEG        equ DEF_SETUPSEG        ; setup starts here 0x90200
SYSSEG          equ DEF_SYSSEG          ; system loaded at 0x10000 (65536).
ENDSEG          equ SYSSEG + SYSSIZE    ; where to stop loading

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

        xor ah, ah                      ; reset FDC ; 复位软盘控制器，让其采用新参数。
        xor dl, dl                      ; dl = 0, 第1个软驱
        int 0x13

; load the setup-sectors directly after the bootblock.
; Note that 'es' is already set up.

; INT 0x13读扇区使用调用参数设置如下：
; ah = 0x02 - 读磁盘扇区到内存； al = 需要读出的扇区数量；
; ch = 磁道(柱面)号的低8位；     cl = 开始扇区(位0-5)，磁道号高2位(位6-7)；
; dh = 磁头号；；               dl = 驱动器号(如果是硬盘则位7要置位)；
; es:bx -> 指向数据缓冲区；如果出错则CF标志置位，ah是出错码。   
; wiki: https://en.wikipedia.org/wiki/INT_13H
load_setup:
        xor dx, dx                      ; drive 0, head 0
        mov cx, 0x0002                  ; sector 2, track 0
        mov bx, 0x0200                  ; adress = 512, in INITSEG
        mov ax, 0x0200+SETUPLEN         ; service 2, nr of sectors
        int 0x13                        ; read it
        jnc ok_load_setup               ; ok - continue

        push ax                         ; dump error code 
        call print_nl
        mov bp, sp                      ; ss:bp指向欲显示的字（world）
        call print_hex
        pop ax

        xor dl, dl                      ; reset FDC ; 复位磁盘控制器，复试
        xor ah, ah
        int 0x13
        jmp load_setup

ok_load_setup:

; Get disk drive parameters, specifically nr of sectors/track
; 这段代码利用BIOS INT 0x13功能号8来取磁盘驱动器的参数。实际是取每磁道扇区数，并保存在
; 位置sectors处。取磁盘驱动器参数INT 0x13调用格式和返回信息如下：
; ah = 0x08     dl = 驱动器号（如果是硬盘则要置位7为1）。
; 返回信息：
; 如果出错则CF置位，并且 ah = 状态码。
; ah = 0 al = 0                 bl = 驱动器类型(AT/PS2)
; ch = 最大磁道号的低8位           cl = 每磁道最大扇区数(位 0-5)，最大磁道号高2位(w位 6-7)
; dh = 最大磁头数                 dl = 驱动器数量 
; es:di --> 软驱磁盘参数表。
        xor dl, dl
        mov ah, 0x08                    ; ah =8 is get drive parameters
        int 0x13
        xor ch, ch
        mov [cs:sectors], cx
        mov ax, INITSEG
        mov es, ax                      ; 因为上面取磁盘参数中断改了es值，这里重新改回

; Print some inane message
; 下面利用BIOS INT 0x10 功能0x03和0x13来显示信息：“'Loading'+回车+换行”，显示包括
; 回车和换行控制字符在内共9个字符。
; BIOS中断0x10功能号 ah = 0x03, 记取光标位置。
; 输入：bh = 页号
; 返回：ch = 扫描开始线； cl = 扫描结束线； dh = 行号(px00顶端)； dl = 列号(0x00最左边)
;
; BIOS中断0x10功能号 ah = 0x13，显示字符串。
; 输入：al = 放置光标的方式及规定属性。0x01-表示使用bl中的属性值，光标停在字符串结尾处。
; bh = 显示页面号； bl =字符属性； dh = 行号； dl = 列号。 cx = 显示的字符串字符数。
; es:bp 此寄存器对指向要显示的字符串起始位置处。
    
        mov ah, 0x03
        xor bh, bh
        int 0x10

        mov cx, 9
        mov bx, 0x0007                  ; page 0, attribute 7 (normal)
        mov bp, msg1
        mov ax, 0x1301                  ; write string, move cursor
        int 0x10

; ok, we've written the message, now
; we want to load the system (at 0x10000)

        mov ax, SYSSEG
        mov es, ax                      ; segment of 0x10000
        call read_it
        call kill_motor
        call print_nl

; After that we check which root-device to use. If the device is
; defined (!= 0), nothong is done and the given device is used.
; Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
; on the number of sectors that the BIOS reports currently.
; 上面一行中两个设备文件的含义说明如下：
; 在Linux中软驱的主设备号是2，次设备号 = type*4 + nr，其中
; nr 为0-3分别对应软驱A、B、C或D；type是软驱的类型（2->12MB或7->1.44MB等）。
; 因为 7*4 + 0 = 28，所以/dev/PS0(2,28)指的是1.44MB A驱动器，其设备号是0x021c
; 同理 /dev/at0(2,8)指的是1.2MB A驱动器，其设备号是 0x0208。
        mov ax, [cs:root_dev]
        or ax, ax
        jne root_defined
        mov bx, [cs:sectors]
        mov ax, 0x0208                  ; /dev/at0 - 1.2MB
        cmp bx, 15
        je root_defined
        mov ax, 0x021c                  ; /dev/ps0 - 1.44MB
        cmp bx, 18
        je root_defined
undef_root:
        jmp undef_root
root_defined:
        mov [cs:root_dev], ax

; after that (everything loaded), we jump to
; the setnup-routine loaded directly after
; the bootblock:
        ; push dx
        ; push cx
        ; push bx
        ; push ax
        ; push 0x55aa
        ; call print_all
        ; hlt
        jmp SETUPSEG:0

; This routine loads the system at address 0x10000, making sure
; no 64 KB boundaries are crossed. We try to load it as fast as
; possible, loading whole tracks whenever we can.
;
; in:   es - staring address segment (normally 0x1000)
;
sread:  dw 1+SETUPLEN                   ; sectors read of current track
head:   dw 0                            ; current head
track:  dw 0                            ; current track

read_it:
        mov ax, es
        test ax, 0x0fff
die:    jne die                         ; es must be at 64KB boundary
        xor bx, bx                      ; bx is starting address within segment
rp_read:
        mov ax, es
        cmp ax, ENDSEG                  ; have we loaded allyet?
        jb ok1_read
        ret
ok1_read:
        mov ax, [cs:sectors]
        sub ax, [sread]
        mov cx, ax
        shl cx, 9
        add cx, bx
        jnc ok2_read
        je ok2_read
        xor ax, ax
        sub ax, bx
        shr ax, 9
ok2_read:
        call read_track
        mov cx, ax
        add ax, [sread]
        cmp ax, [cs:sectors]            ; 若当前磁道上还有扇区未读，则跳转到ok3_read处
        jne ok3_read
        mov ax, 1
        sub ax, [head]
        jne ok4_read                    ; 如果是0磁头，则再去读1磁头面上的扇区数据
        inc word [track]                ; 否则去读下一磁道
ok4_read:
        mov [head], ax                  ; 保存当前磁头号
        xor ax, ax                      ; 清零当前磁道已读扇区数
ok3_read:
        mov [sread], ax
        shl cx, 9
        add bx, cx
        jnc rp_read
        mov ax, es
        add ah, 0x10                    ; 将段基地址调整为指向下一个64KB内存开始处。
        mov es, ax
        xor bx, bx                      ; 清零段内数据开始偏移值
        jmp rp_read

; read_track 子程序。读当前磁道上指定开始扇区和需读扇区数的数据到 es:bx 开始处。
; al - 需读扇区数            es:bx - 缓冲区开始位置
read_track:
; 首先调用BIOS中断0x10，功能0x0e（以电传方式写字符），光标前移一位置。显示一个‘.’
        pusha
        pusha
        mov ax, 0x0e2e                  ; loading... message 2e = .
        mov bx, 7                       ; 字符前景色属性
        int 0x10
        popa

; 然后正式进行磁道扇区读操作
        mov dx, [track]
        mov cx, [sread]
        inc cx                          ; cl = 开始读扇区
        mov ch, dl                      ; ch = 当前磁道号
        mov dx, [head]
        mov dh, dl                      ; dh = 磁头号， dl = 驱动器号（为0表示当前A驱动器）
        and dx, 0x0100                  ; 磁头号不大于1
        mov ah, 2                       ; ah = 2, 读磁盘扇区功能号

        push dx                         ; save for error dump
        push cx
        push bx
        push ax

        int 0x13
        jc bad_rt
        add sp, 8
        popa
        ret

bad_rt: push ax                         ; save error code
        call print_all                  ; ah = error, al = read


        xor ah, ah
        xor dl, dl
        int 0x13                        ; 执行驱动器复位操作（磁盘中断功能号0）


        add sp, 10                      ; 丢弃为出错情况保存的信息
        pop ax
        jmp read_track

;
;       print_all is for debugging purpose.
;       It will print out all of the registers. The assumption is that this is
;       called from a routine, with a stack frame like
;       dx
;       cx
;       bx
;       ax
;       error
;       ret <- sp
;
print_all:
        mov cx, 5                       ; error code + 4 registers
        mov bp, sp

print_loop:
        push cx                         ; save count left
        call print_nl                   ; nl for readability
        jae no_reg                      ; see if register name is needed
        mov ax, 0x0e05 + 0x41 - 1       ; ah = 功能号（0x0e）；al = 字符（0x05 + 0x41 - 1）
        sub al, cl
        int 0x10

        mov al, 0x58                    ; X
        int 0x10

        mov al, 0x3a                    ; :
        int 0x10

no_reg:
        add bp, 2                       ; next register
        call print_hex                  ; print it
        pop cx
        loop print_loop
        ret

print_nl:
        mov ax, 0xe0d                   ; CR
        int 0x10
        mov al, 0xa                     ; LF
        int 0x10
        ret

;       print hex is for debugging purpose, and prints the word
;       pointed to by ss:bp in hexadecmial
print_hex:
        mov cx, 4                       ; 4 hex digits
        mov dx, [bp]                    ; load word into dx
print_digit:
        rol dx, 4                       ; rotate so that lowest 4 bits are used
        mov ah, 0x0e
        mov al, dl                      ; mask off so we have only next nibble
        and al, 0xf
        add al, 0x30                    ; convert to 0 based digit, '0'
        cmp al, 0x39                    ; check for overflow
        jbe good_digit
        add al, 0x41 - 0x30 - 0xa       ; 'A' - '0' - 0xa

good_digit:
        int 0x10
        loop print_digit                ; cx--
        ret

; This procedure turns off the floppy drive motor, so
; that we enter the kernel in a known state, and
; dont't have to worry about it later.
kill_motor:
        push dx
        mov dx, 0x3f2
        xor al, al
        out dx, al
        pop dx
        ret

sectors:
        dw 0

msg1:
        db 13, 10
        db 'Loading'



;;;;;;;;;;;;;;;;;;;;;;;;
times 506-($-$$) db 0
swap_dev:
        dw SWAP_DEV
root_dev:
        dw ROOT_DEV
boot_flag:
        db 0x55, 0xaa
