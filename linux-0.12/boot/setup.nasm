; setup.s is responsible for getting the system data from the BIOS,
; and putting them into the appropriate places in system memory.
; both setup.s and system has been loaded by the bootblock.
; 
; This code asks the bios for memory/disk/other parameters, and
; puts them in a "safe" place: 0x90000-0x901FF, ie where the
; boot-block used to be. It is then up to the protected mode
; system to read them from there before the area is overwritten
; for buffer-blocks.

; NOTE! These had better be the same as in bootsect.s!
;#include <linux/config.h>
DEF_SYSSIZE     equ 0x3000      ; 默认系统模块长度。单位是节，每节16字节；
DEF_INITSEG     equ 0x9000      ; 默认本程序代码移动目的段位置；
DEF_SETUPSEG    equ 0x9020      ; 默认setup程序代码段位置；
DEF_SYSSEG      equ 0x1000      ; 默认从磁盘加载系统模块到内存的段位置。
SYSSIZE         equ DEF_SYSSIZE ; 编译连接后system模块的大小。

INITSEG         equ DEF_INITSEG ; we move boot here - out of the way
SYSSEG          equ DEF_SYSSEG  ; system loaded at 0x10000 (65536)
SETUPSEG        equ DEF_SETUPSEG; this is the current segment

start:

; ok, the read went well so we get current cursor position and save it for
; posterity
        mov     ax, INITSEG
        mov     ds, ax

; Get memory size (extended men, KB)
; 取扩展内存的大小（KB）
; 利用BIOS中断0x15 功能号 ah = 0x88 取系统所含扩展内存大小并保存在内存0x90002处。
; 返回：ax = 从0x100000(1M) 处开始的扩展内存大小(KB)。若出错则CF置位，ax = 出错码。
        mov     ah, 0x88
        int     0x15
        mov     [2], ax

; check for EGA/VGA and some config parameters
; 检查显示方式（EGA/VGA）并取参数。
; 调用BIOS中断0x10功能号0x12（视频子系统配置）取EBA配置信息。
; ah = 0x12, bl = 0x10 - 取EGA配置信息。
; 返回：
; bh = 显示状态（0x00 -彩色模式，I/O端口=0x3dX； 0x01 -单色模式，I/O端口=0x3bX）。
; bl = 安装的显示内存(0x00 - 64K; 0x01 - 128K; 0x02 - 192K; 0x03 = 256K。)
; cx = 显示卡特性参数(参见程序后对BIOS视频中断0x10的说明)。

        mov     ah, 0x12
        mov     bl, 0x10
        int     0x10
        mov     [8], ax         ; 0x90008 = ??
        mov     [10], bx        ; 0x9000A = 安装的显示内存； 0x9000B = 显示状态(彩/单色)
        mov     [12], cx        ; 0x9000C = 显示卡特性参数
; 检测屏幕当前行列值。若显示卡是VGA卡时则请求用户选择显示行列值，并保存到0x9000E处。
        mov     ax, 0x5019      ; 在ax中预置屏幕默认行列值（ah = 80列； al = 25行）。
        cmp     bl, 0x10        ; 若中断返回bl值为0x10，则表示不是VGA显示卡，跳转。
        je      novga
        call    chsvga          ; 检测显示卡厂家和类型，修改显示行列值
novga:  mov     [14], ax        ; 保存屏幕当前行列值（0x9000E, 0x9000F）

; 使用BIOS中断0x10功能0x03取屏幕当前光标位置，并保存在内存0x90000处（2字节）。
; 控制台初始化程序console.c会到此处读取该值。
; BIOS 中断0x10功能号 ah = 0x03, 读光标位置。
; 输入：bh = 页号
; 返回：ch = 扫描开始线； cl = 扫描结束线； dh = 行号（0x00顶端）； dl = 列号（0x00最左边）。
        mov     ah, 0x03        ; read cursor pos
        xor     bh, bh
        int     0x10            ; save it in known place, con_init fetches
        mov     [0], dx         ; it from 0x90000

; Get video-card data:
; 下面这段用于取显示卡当前显示模式。
; 调用BIOS中断0x10，功能号 ah = 0x0f。
; 返回：ah = 字符列数； al = 显示模式； bh = 当前显示页。
; 0x90004(1字)存放当前页；0x90006存放显示模式； 0x90007存放字符列数。

        mov     ah, 0x0f
        int     0x10
        mov     [4], bx         ; bh = display page
        mov     [6], ax         ; al = video mode, ah = window width

; Get hd0 data
; 取第一个硬盘的信息（复制硬盘参数表）。
; 第1个硬盘参数表的首地址竟然是中断0x41的中断向量值！而第2个硬盘参数表紧接在第1个
; 表的后面，中断0x46的向量值也指向第2个硬盘参数表首地址。表的长度是16个字节。
; 下面两段程序分别复制ROM BIOS中有关两个硬盘参数表到：0x90080处存放第1个硬盘的表，
; 0x90090处存放第2个硬盘的表。有关硬盘参数表内容说明。
        mov     ax, 0x0000
        mov     ds, ax
        lds     si, [4*0x41]    ; 取中断向量0x41的值，即hd0参数表的地址 -> ds:si
        mov     ax, INITSEG
        mov     es, ax
        mov     di, 0x0080
        mov     cx, 0x10
        rep movsb

; Get hd1 data

        mov     ax, 0x0000
        mov     ds, ax
        lds     si, [4*0x46]
        mov     ax, INITSEG
        mov     es, ax
        mov     di, 0x0090
        mov     cx, 0x10
        rep movsb

; Check that there IS a hd1 :-)
; 检查系统是否有第2个硬盘。如果没有则把第2个表清零。
; 利用BIOS中断调用0x13的取盘类型功能，功能号 ah = 0x15;
; 输入：dl = 驱动器号（0x8X是硬盘：0x80指第1个硬盘，0x81第2个硬盘）
; 输出：ah = 类型码； 00 - 没有这个盘，CFl置位； 01 - 是软驱，没有change-line支持；
;                   02 - 是软驱（或其他可移动设备），有change-line支持；
;                   03 - 是硬盘。

        mov     ax, 0x1500
        mov     dl, 0x81
        int     0x13
        jc      no_disk1
        cmp     ah, 3           ; 是硬盘吗?(类型 = 3)
        je      is_disk1
no_disk1:
        mov     ax, INITSEG     ; 第2个硬盘不存在，则对第2个硬盘表清零。
        mov     es, ax
        mov     di, 0x0090
        mov     cx, 0x10
        mov     ax, 0x00
        rep stosb
is_disk1:

; now we want to move to protected mode ...

        cli                     ; no interrupts allowed

; first we move the system to it's rightful place
        mov     ax, 0x0000
        cld                     ; 'direction'=0, movs moves forward
do_move:
        mov     es, ax          ; destination segment
        add     ax, 0x1000
        cmp     ax, 0x9000 
        jz      end_move
        mov     ds, ax
        sub     di, di
        sub     si, si
        mov     cx, 0x8000       ; 移动0x8000字（64KB字节）
        rep movsw
        jmp     do_move

; then we load the segment descriptors

end_move:
        mov     ax, SETUPSEG    ; right, forgot this at first.didn't work :-)
        mov     ds, ax
        lidt    [idt_48]        ; load idt with 0, 0
        lgdt    [gdt_48]        ; load gdt with whatever appropriate

; that was painless, now we enable A20
        call    empty_8042      ; 测试8042状态寄存器，等待输入缓冲器空。
                                ; 只有当输入缓冲器为空时才可以对其执行写命令。
        mov     al, 0xd1        ; command write ; 0xD1命令码 - 表示要写数据到
        out     0x64, al        ; 8042的P2端口。P2端口位1用于A20线的选通。
        call    empty_8042      ; 等待输入缓冲器空，看命令是否被接受。
        mov     al, 0xDF        ; A20 on                ! 选通A20地址线的参数。
        out     0x60, al        ; 数据要写到0x60口。
        call    empty_8042      ; 若此时输入缓冲器为空，则表示A20线已经选通。

; well, that went ok, I hope. Now we have to reprogram the interrupts :-(
; we put them right after the intel-reserved hardware interrupts, at
; int 0x20-0x2F. There they won't mess up anthing. Sadly IBM really
; messed this up with the original PC, and they haven't been able to
; rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
; which is used for the internal hardware interrupts as well. We just 
; have to reprogram the 8259's, and it isn't fun.
        mov     al, 0x11        ; initialization sequence
        out     0x20, al        ; sent it to 8259A-1    ; 发送到8259A主芯片。
        dw      0x00eb, 0x00eb  ; jmp $+2, jmp $+2      ; $表示当前指令的地址
        out     0xA0, al        ; and to 8259A-2
        dw      0x00eb, 0x00eb
; Linux系统硬件中断号被设置成从0x20开始
        mov     al, 0x20        ; start of hardware int's (0x20)
        out     0x21, al        ; 送主芯片ICW2命令字，设置起始中断号，要送奇端口。
        dw      0x00eb, 0x00eb
        mov     al, 0x28        ; start of hardware int's 2 (0x28)
        out     0xA1, al        ; 送从芯片ICW2命令字，从芯片的起始中断号。
        dw      0x00eb, 0x00eb
        mov     al, 0x04        ; 8259-1 is master
        out     0x21, al        ; 送主芯片ICW3命令字，主芯片的IR2连从芯片 INT。
        dw      0x00eb, 0x00eb
        mov     al, 0x02        ; 8259-2 is slave
        out     0xA1, al        ; 送从芯片ICW3命令字，表示从芯片的INT连到主芯片的IR2引脚上。
        dw      0x00eb, 0x00eb
        mov     al, 0x01        ; 8086 mode for both
        out     0x21, al        ; 送主芯片ICW4命令字。8086模式：普通EOI、非缓冲方式，需发送指令复位
                                ; 初始化结束，芯片就绪。
        dw      0x00eb, 0x00eb
        out     0xA1, al        ; 送从芯片ICW4，内容同上。
        dw      0x00eb, 0x00eb
        mov     al, 0xFF        ; mask off all interrupts for now
        out     0x21, al        ; 屏蔽主芯片所有中断请求。
        dw      0x00eb, 0x00eb
        out     0xA1, al        ; 屏蔽从芯片所有中断请求。

; well, that certainly wasn't fun :-(. Hopefully it works, and we don't
; need no steenking BIOS anyway (except for the initial loading :-).
; The BIOS-routine wants lots of unnecessary data, and it's less 
; "interesting" anyway. This is how REAL programmers do it.
;
; Wellm now's the time to actually move into protected mode. To make
; things as simple as possible, we do no register set-up or anything,
; we let the gnu-compiled 32-bit programs do that. We just jump to 
; absolute address 0x00000, in 32-bit protected mode.
        mov     ax, 0x0001      ; protected mode (PE) bit
        lmsw    ax              ; This is it!
        jmp     8:0             ; jmp offset 0 of segment 8 (cs)

; This routine checks that the keyboard command queue is empty
; No timeout is used - if this hangs there is something wrong with
; the machine, and we probabl couldn't proceed anyway.
; 只有当输入缓冲器为空时（键盘控制器状态寄存器位1 = 0）才可以对其执行写命令。
empty_8042:
        dw      0x00eb, 0x00eb
        in      al, 0x64        ; 8042 status port      ; 读AT键盘控制状态寄存器。
        test    al, 2           ; is input buffer full? ; 测试位1，输入缓冲器满？
        jnz     empty_8042      ; yes - loop
        ret

; Routine trying to recognize type of SVGA-board present (if any)
; and if it recognize one gives the choices of resolution it offers.
; If one is found the resolution chosen is given by al,ah (rows,cols).

chsvga: cld
        push    ds
        push    cs              ; 把默认数据段设置成和代码段同一个段。
        pop     ds
        mov     ax, 0xc000
        mov     es, ax          ; es 指向0xc000段。此处是VGA卡上的 ROM BIOS区。
        lea     si, [msg1]      ; ds:si指向msg1字符串。
        call    prtstr          ; 显示以NULL结尾的msg1字符串。
nokey:  in      al, 0x60        ; 读取键盘控制缓冲中的扫描码
        cmp     al, 0x82        ; 与最小断开码0x82比较。
        jb      nokey           ; 若小于0x82，表示还没有松按键松开。
        cmp     al, 0xe0
        ja      nokey           ; 若大于 0xe0，表示收到的是扩展扫描码前缀。
        cmp     al, 0x9c        ; 若断开码是0x9c表示用户按下/松开了回车键，
        je      svga            ; 于是程序跳转去检查系统是否具有SVGA模式。
        mov     ax, 0x019       ; 否则设置默认行列值 AL=25行、AH=80列。
        pop     ds
        ret

; 下面根据VGA显示卡上的 ROM BIOS 指定位置的特征数据串或者支持的特别功能来判断机器上
; 安装的是什么牌子的显示卡。本程序支持10种显示卡的扩展功能。
; 
; 首先判断是不是ATI显示卡。我们把ds:si指向ATI显示卡特征数据串，并把es:si指向 VGA BIOS
; 中指定位置（偏移0x31）处。该特征串共有9个字符（“761295520”），我们来循环比较这个特征
; 串。如果相同则表示机器中的VGA卡是ATI牌子的，于是让ds:si指向该显示卡可以设置的行列值dscati,
; 让di指向ATI卡可设置的行列个数和模式，并跳转到标号 selmod 处进一步进行设置。
svga:   lea     si, [idati]     ; Check ATI 'clues'
        mov     di, 0x31        ; 特征串从0xc000:0x0031 开始。
        mov     cx, 0x09        ; 特征串有9个字节。
        repe    cmpsb           ; 如果9个字节都相同，表示系统中有一块ATI牌显示卡。
        jne     noati           ; 若特征串不同则表示不是ATI显示卡。跳转继续检测卡。
; Ok, 我们现在确定了显示卡的牌子是ATI。于是si指向ATI显示卡可选行列值表dscati
; di指向扩展模式个数和扩展模式号列表moati，然后跳转到selmod处继续处理。
        lea     si, [dscati]    ; 把dscati的有效地址放入si。
        lea     di, [moati]
        lea     cx, [selmod]
        jmp     cx

; 现在来判断是不是Ahead牌子的显示卡。首先向 EGA/VGA 图形索引寄存器 0x3ec写入想访问的
; 主允许寄存器索引号 0x0f，同时向0x3cf端口（此时对应主允许寄存器）写入开启扩展寄存器
; 标志值 0x20。然后通过0x3cf端口读取主允许寄存器值，以检查是否可以设置开启扩展寄存器
; 标志。如果可以则说明是Ahead牌子的显示卡。注意word输出时 al->端口n，ah->端口n+1。
noati:  mov     ax, 0x200f      ; Check Ahead 'clues'
        mov     dx, 0x3ce       ; 数据端口指向主允许寄存器（0x0f->0x3ce端口）
        out     dx, ax          ; 并设置开启扩展寄存器标志（0x20->0x3cf端口）
        inc     dx              ; 然后再读取寄存器，检查该标志是否被设置上。
        in      al, dx
        cmp     al, 0x20        ; 如果读取值是0x20，则表示是Ahead A显示卡。
        je      isahed
        cmp     al, 0x21        ; 如果赢取值是0x21，则表示是Ahead B显示卡。
        jne     noahed          ; 否则说明不是 Ahead显示卡，于是跳转继续检测其余卡。
isahed: lea     si, [dscahead]
        lea     di, [moahead]
        lea     cx, selmod
        jmp     cx

; 现在来检查是不是 Chips & Tech 生产的显示卡。通过端口0x3c3（0x04或0x46e8）设置VGA允许
; 寄存器的进入设置模式标志（位4），然后从端口 0x104 读取显示卡芯片集标识值。如果该标识值
; 是0xA5，则说明是 Chips & Tech 生产的显示卡。
noahed: mov     dx, 0x3c3       ; Check Chips & Tech. 'clues'
        in      al, dx          ; 从 0x3c3 端口读取VGA允许寄存器值，
        or      al, 0x10        ; 添加上进入设置模式标志（位4）后再写回。
        out     dx, al
        mov     dx, 0x104       ; 在设置模式时从全局标识端口0x104读取显示卡芯片标识值，
        in      al, dx          ; 并暂时存放在bl寄存器中。
        mov     bl, al
        mov     dx, 0x3c3       ; 然后把0x303端口中的进入设置模式标志复位。
        in      al, dx
        and     al, 0xef
        out     dx, al
        cmp     bl, [idcandt]   ; 再把bl中标识值与位于idcandt处的Chips &
        jne     nocant          ; Tech的标识值 0xA5 作比较。如果不同则跳转比较下一种显卡。
        lea     si, [dsccandt]
        lea     di, [mocandt]
        lea     cx, [selmod]
        jmp     cx

; 现在检查是不是 Cirrus 显示卡。方法是使用CRT控制器索引号0x1f寄存器内容来尝试禁止扩展
; 功能。该寄存器被称为鹰标（Eagle ID）寄存器，将其值高低半字节交换一下后写入端口0x3c4索
; 引的6号（定序/扩展）寄存器应该会禁止Cirrus显示卡的扩展功能。如果不会则说明不是Cirrus
; 显示卡。因为从端口0x3d4索引的0x1f鹰标寄存器中读取内容是鹰标值与0x0c索引号对应的显
; 存起始地址高字节寄存器内容异或操作之后的值，因此在读0x1f中内容之前我们需要先把显存起始
; 高字节寄存器内容保存后清零，并在检查后恢复之。另外，将没有交换过的Eagle ID值写到0x3c4
; 端口索引的6号定序/扩展寄存器会重新开启扩展功能。
nocant: mov     dx, 0x3d4       ; Check Cirrus ‘clues’
        mov     al, 0x0c        ; 首先向CRT控制寄存器的索引寄存器端口0x3d4写入要访问
        out     dx, al          ; 寄存器索引号0x0c（对应显存起始地址高字节寄存器），
        inc     dx              ; 然后从0x3d5端口读入显存起始地址高字节并暂存在bl中，
        in      al, dx          ; 再把显存起始地址高字节寄存器清零。
        mov     bl, al
        xor     al, al
        out     dx,al
        dec     dx              ; 接着向0x3d4端口输出索引0x1f，指出我们要在0x3d5端口
        mov     al, 0x1f        ; 访问读取“Eagle ID”寄存器内容。
        out     dx, al
        inc     dx
        in      al, dx          ; 从0x3d5端口读取“Eagle ID”寄存器值，并暂存在bh中。
        mov     bh, al          ; 然后把该值高低4比特互换位置存放到cl中。再左移8位
        xor     ah, ah          ; 后放入ch中，而cl中放入数值6。
        shl     al, 4
        mov     cx, ax
        mov     al, bh
        shr     al, 4
        add     cx, ax
        shl     cx, 8
        add     cx, 6           ; 最后把cx值存放入ax中。此时ah中是换位后的“Eagle
        mov     ax, cx          ; ID”值，al中是索引号6，对应定序/扩展寄存器。把ah
        mov     dx, 0x3c4       ; 写到0x3c4端口索引的定序/扩展寄存器应该会导致Cirrus
        out     dx, ax          ; 显示卡禁止扩展功能。
        inc     dx
        in      al, dx          ; 如果扩展功能真的被禁止，那么此时读入的值应该为0。
        and     al, al          ; 如果不为0则表示不是Cirrus显示卡，跳转继续检查其他卡。
        jnz     nocirr
        call    rst3d4          ; 恢复CRT控制器的显示起始地址高字节寄存器内容。
        lea     si, [dsccirrus]
        lea     di, [mocirrus]
        lea     cx, [selmod]
        jmp     cx

; 该子程序利用保存在bl中的值恢复CRT控制器的显示起始地址高字节寄存器内容。
rst3d4: mov     dx, 0x3d4
        mov     al, bl
        xor     ah, ah
        shl     ax, 8
        add     ax, 0x0c
        out     dx, ax          ; 注意，这是word输出！！al->0x3d4, ah->0x3d5。
        ret

; 现在检查系统中是不是Everex显示卡。方法是利用中断 int 0x10 功能 0x70（ax = 0x7000，
; bx = 0x0000）调用Everex的扩展视频BIOS功能。对于Everes类型显示卡，该中断调用应该
; 会返回模拟状态，即有以下返回信息：
; al = 0x70, 若是基于Tredent的Everex显示卡；
; cl = 显示器类型： 00-单色； 01-CGA； 02-EGA； 03-数字多频； 04-PS/2； 05-IBM 8514； 06-SVGA。
; ch = 属性： 位7-6：00-256K， 01-512K， 10-1MB， 11-2MB； 位4-开启VGA保护； 位0-6845模拟。
; dx = 板卡型号：位15-4：板类型标识号； 位3-0：板修正标识号。
;      0x2360-Ultragraphics II; 0x6200-Vision VGA; 0x6730-EVGA; 0x6780-Viewpoint。
; di = 用BCD码表示的视频BIOS版本号。
nocirr: call    rst3d4          ; Check Everex 'clues'
        mov     ax, 0x7000      ; 设置ax = 0x7000， bx = 0x0000，调用 int 0x10。
        xor     bx, bx
        int     0x10
        cmp     al, 0x70        ; 对于Everes显示卡，al中应该返回值0x70。
        jne     noevrx
        shr     dx, 4           ; 忽略板修正号（位3-0）。
        cmp     dx, 0x678       ; 板类型号是0x678表示是一块Trident显示卡，则跳转。
        je      istrid
        cmp     dx, 0x236       ; 板类型号是0x236表示是一块Trident显示卡，则跳转。
        je      istrid
        lea     si, [dsceverex]
        lea     di, [moeverex]
        lea     cx, selmod
        jmp     cx
istrid: lea     cx, [ev2tri]    ; 是Trident类型的Everex显示卡，则跳转到ev2tri处理。
        jmp     cx

; 现在检查是不是Genoa显示卡。法式是检查其视频BIOS中的特征数字串（0x77、0x00、0x66、
; 0x99）。注意，此时es已经被设置成指向VGA卡上ROM BIOS所在的段0xc000。
noevrx: lea     si, [idgenoa]   ; Check Genoa 'clues'
                                ; 让ds:si指向特征数字串。
        xor     ax, ax          
        mov     al, [es:0x37]   ; 取VGA卡上BIOS中0x37处的指针（它指向特征串）。
        mov     di, ax          ; 因此此时es:di指向特征数字串开始处。
        mov     cx, 0x04
        dec     si
        dec     di
ll:     inc     si              ; 然后循环比较这4个字节的特征数字串。
        inc     di
        mov     al, [si]
        and     al, [es:di]
        cmp     al, [si]
        loope   ll
        cmp     cx, 0x00        ; 如果特征数字串完全相同，则表示 是Genoa显示卡，
        jne     nogen           ; 否则跳转去检查其他类型的显示卡。
        lea     si, [dscgenoa]
        lea     di, [mogenoa]
        lea     cx, [selmod]
        jmp     cx

; 现在检查是不是Paradise显示卡。同样采用比较显示卡上BIOS中特征串（“VGA=”）的方式。
nogen:  lea     si, [idparadise]; Check Paradise ‘clues’        
        mov     di, 0x7d        ; es:di指向 VGA ROM BIOS的0xc000:0x007d处，该处应该有
        mov     cx, 0x04        ; 4个字符 “VGA=”。
        repe    cmpsb
        jne     nopara
        lea     si, [dscparadise]
        lea     di, [moparadise]
        lea     cx, [selmod]
        jmp     cx

; 现在检查是不是Trident（TVGA）显示卡。TVGA显示卡扩充的模式控制寄存器1（0x3c4端口索引
; 的0x0e）的位3--0是64K内存页面个数值。这个字段值有一个特性：当写入时，我们需要首先把
; 值与0x02进行异或操作后再写入；当读取该值时则不需要执行异或操作，即异或前的值应该与
; 写入后再读取的值相同。下面代码就利用这个特性来检查是不是Trident显示卡。
nopara: mov     dx, 0x3c4       ; Check Trident 'clues'
        mov     al, 0x0e        ; 首先在端口0x3c4输出索引号0x0e，索引模式控制寄存器1。
        out     dx, al          ; 然后从0x3c5数据端口读入该寄存器原值，并暂存在ah中。
        inc     dx
        in      al, dx
        xchg    ah, al
        mov     al, 0x00        ; 然后我们向该寄存器写入0x00，再读取其值->al。
        out     dx, al          ; 写入0x00就相当于“原值”0x02异或0x02后的写入值，
        in      al, dx          ; 因此若是Trident显示卡，则此后读入的值应该是0x02。
        xchg    al, ah          ; 交换后， al=原模式控制寄存器1的值，ah=最后读取的值。
; 如果bl中原模式控制寄存器1的位1在置位状态的话就将其复位，否则就将位1置位。
; 实际上这几条语句就是对原模式控制寄存器1的值执行异或0x02的操作，然后用结果值去设置
; （恢复）原寄存器值。
        mov     bl, al
        and     bl, 0x02
        jz      setb2
        and     al, 0xfd
        jmp     clrb2
setb2:  or      al, 0x02
clrb2:  out     dx, al
        and     ah, 0x0f        ; 取最后读入值的页面个数字段（位3--0），如果
        cmp     ah, 0x02        ; 该字段值等于0x02，则表示是Trident显示卡。
        jne     notrid
ev2tri: lea     si, [dsctrident]
        lea     di, [motrident]
        lea     cx, [selmod]
        jmp     cx

; 现在检查是不是Tseng显示卡（ET4000AX或ET4000/W32类）。方法是对0x3cd端口对应的段
; 选择（Segment Select）寄存器执行读写操作。该寄存器高4位（位7--4）是要进行读操作的
; 64KB段号（Bank number）,低4位（位3--0）是指定要写的段号。如果指定段选择寄存器的
; 值是0x55（表示读、写第6个64KB段），那么对于Tseng显示卡来说，把该值写入寄存器后
; 再读出应该不是0x55。
notrid: mov     dx, 0x3cd       ; Check Tseng 'clues'
        in      al, dx          ; Could things be this simple ! :-)
        mov     bl, al          ; 先从0x3cd端口读取段选择寄存器原值，并保存在bl中。
        mov     al, 0x55        ; 然后我们向该寄存器写入0x55。再读入并放在ah中。
        out     dx, al
        in      al, dx
        mov     ah, al
        mov     al, bl          ; 接着恢复该寄存器的原值
        out     dx, al
        cmp     ah, 0x55        ; 如果读取的就是我们写入的值，则表明是Tseng显示卡。
        jne     notsen
        lea     si, [dsctseng]
        lea     di, [motseng]
        lea     cx, [selmod]
        jmp     cx

; 下面检查是不是Video7显示卡。端口0x3c2是混合输出寄存器写端口，而0x3cc是混合输出寄存
; 器读端口。该寄存器的位0是单色/彩色标志。如果为0则表示是单色，否则是彩色。判断是不是
; Video7显示卡的方式是利用这种显示卡的CRT控制扩展标识寄存器（索引号是0x1f）。该寄存器
; 的值实际上就是显存起始地址高字节寄存器（索引号0x0c）的内容和0xea进行进行异或操作后的值。
; 因此我们只要向显存起始地址高字节寄存器中写入一个特定值，然后从标识寄存器中读取标识值
; 进行判断即可。
; 通过以上显示卡和这里Video7显示卡的检查分析，我们可知检查过程通常分为三个基本步骤。
; 首先读取并保存测试需要用到的寄存器原值，然后使用特定测试值进行写入和读出操作，最后恢复
; 寄存器值并对检查结果作出判断。
notsen: mov     dx, 0x3cc       ; Check Video7 'clues'
        in      al, dx
        mov     dx, 0x3b4       ; 先设置dx为单色显示CRT控制索引寄存器端口号0x3b4。
        and     al, 0x01        ; 如果混合混合输出寄存器的位0等于0（单色）则直接跳转，
        jz      even7           ; 否则dx设置为彩色显示CRT控制索引寄存器端口号0x3d4。
        mov     dx, 0x3d4
even7:  mov     al, 0x0c        ; 设置寄存器索引号为0x0c，对应显存地址高字节寄存器。
        out     dx, al
        inc     dx
        in      al, dx          ; 读取显示内存起始地址高字节寄存器内容，并保存在bl中。
        mov     bl, al
        mov     al, 0x55        ; 然后在显存起始地址高字节寄存器中写入值0x55，再读取出来。
        out     dx, al
        in      al, dx
        dec     dx              ; 然后通过CRTC索引寄存器端口0x3b4或0x3d4选择索引号是
        mov     al, 0x1f        ; 0x1f的Video7显示卡标识寄存器。该寄存器内容实际上就是
        out     dx, al          ; 显存起始地址高字节和0xea进行异或操作后的结果值。
        inc     dx
        in      al, dx          ; 读取Video7显示卡寄存器值，并保存在bh中。
        mov     bh, al
        dec     dx              ; 然后再选择显存起始地址高字节寄存器，恢复其原值。
        mov     al, 0x0c        
        out     dx, al
        inc     dx
        mov     al, bl
        out     dx, al
        mov     al, 0x55        ; 随后我们来验证“Video7显示卡标识寄存器值就是显存起始
        xor     al, 0xea        ; 地址高字节和0xea进行异或操作后的结果值”。因此0x55
        cmp     al, bh          ; 和0xea进行异或操作的结果就就忘等于标高寄存器的测试值。
        jne     novid7
        lea     si, [dscvideo7]
        lea     di, [movideo7]

; 下面根据上述代码判断出的显示卡类型以及取得的相关扩展模式信息（si指向的行列值列表；di
; 指向扩展模式个数和模式号列表），提示用户选择可用的显示模式，并设置成相应显示模式。最后
; 子程序返回系统当前设置的屏幕行列值（ah = 列数； al = 行数）。例如，如果系统中是ATI显
; 示卡, 那么屏幕上会显示以下信息；
; Mode: COLSxROWS:
; 0.    132 x 25
; 1.    132 x 44
; Choose mode by pressing the corresponding number.
; 
; 这段程序首先在屏幕上显示NULL结尾的字符串信息“Mode: COLSxROWS:”。
selmod: push    si
        lea     si, [msg2]
        call    prtstr
        xor     cx, cx
        mov     cl, [di]        ; 此时cl中是检查出的显示卡的扩展模式个数。
        pop     si
        push    si
        push    cx
; 然后在每一行上显示出当前显示 卡可选择的扩展模式行列值，供用户选用。
tbl:    pop     bx              ; bx = 显示卡的扩展模式总个数。
        push    bx
        mov     al, bl
        sub     al, cl
        call    dprnt           ; 以十进制格式显示al中的值。
        call    spcing          ; 显示一个点再空4个空格。
        lodsw                   ; 在ax中加载si指向的行列值，随后si指向下一个word值。
        xchg    al, ah          ; 交换位置后 al = 列数。
        call    dprnt           ; 显示列数；
        xchg    ah, al          ; 此时al中是行数值。
        push    ax
        mov     al, 0x78        ; 显示一个小“x”，即乘号。
        call    prnt1 
        pop     ax              ; 此时al中是行数值。
        call    dprnt           ; 显示行数。
        call    docr            ; 回车换行。
        loop    tbl             ; 再显示下一个行列值。
; 在扩展模式行列值都显示之后，显示“Choose mode by pressing the corresponding number.”。
        pop     cx              ; cl中是显示卡扩展模式总个数值。
        call    docr
        lea     si, [msg3]
        call    prtstr

; 然后从键盘口读取用户按键的扫描码，根据该扫描码确定用户选择的行列值模式号，并利用ROM
; BIOS的显示中断int 0x10功能0x00来设置相应的显示模式。
; "模式个数值+0x80"是所按数字键-1的断开扫描码。对于0--9数字键，它们的断开
; 扫描码分别是：0 - 0x8B; 1 - 0x82; 2 - 0x83; 3 - 0x84; 4 - 0x85;
;             5 - 0x86; 6 - 0x87; 7 - 0x88; 8 - 0x89; 9 - 0x8A。
; 因此，如果读取的键盘断开扫描码小于0x82就表示不是数字键； 如果扫描码等于0x8B则表示用户
; 按下数字0键。
        pop     si              ; 弹出原行列值指针（指向显示卡行列值表开始处）。
        add     cl, 0x80        ; cl + 0x80 = 对应“数字键-1”的断开扫描码。
nonum:  in      al, 0x60        ; Quick and dirty...
        cmp     al, 0x82        ; 若键盘断开扫描码小于0x82则表示不是数字键，忽略该键。
        jb      nonum
        cmp     al, 0x8b        ; 若键盘断开扫描码等于0x8B，表示按下了数字键0。
        je      zero
        cmp     al, cl
        ja      nonum
        jmp     nozero

; 下面把断开扫描码转换成对应的数字按键值，然后利用该值从模式个数和模式号列表中选择对应的
; 模式号。接着调用机器ROM BIOS中断 int 0x10功能0把屏幕设置成模式号指定的模式。最后再
; 利用模式号从显示卡行列值表中选择并在ax返回对应的行列值。
zero:   sub     al, 0x0a        ; al = 0x8b - 0x8a = 0x81
nozero: sub     al, 0x80        ; 再送去0x80就可以得到用户选择了第几个模式。
        dec     al              ; 从0起计数。
        xor     ah, ah          ; int 0x10显示功能号=0（设置显示模式）。
        add     di, ax
        inc     di              ; di指向对应的模式号（路过第1个模式个数字节值） 。
        push    ax
        mov     al, [di]        ; 取模式号->al中， 并调用系统BIOS显示中断功能0.
        int     0x10
        pop     ax
        shl     ax, 1           ; 模式号乘2，转换成为行列值表中对应值的指针。
        add     si, ax
        lodsw                   ; 取对应行列值到ax中（ah = 列数， al = 行数）。
        pop     ds              ; 恢复保存的ds原值。在ax中返回当前显示行列值。
        ret

; 若都不是上面检测的显示卡，那么我们只好采用默认的80 x 25 的标准行列值。
novid7: pop     dx              ; Here could be code to support standard 80x50,80x30
        mov     ax, 0x5019
        ret

; Routine that 'tabs' to next col.
; 显示一个点字符‘.’和4个空格。
spcing: mov     al, 0x2e        ; 显示一个字符‘.’
        call    prnt1 
        mov     al, 0x20
        call    prnt1 
        mov     al, 0x20
        call    prnt1
        mov     al, 0x20
        call    prnt1 
        mov     al, 0x20
        call    prnt1 
        ret

; Routine to print asciiz-string at DS:SI
; 显示位于DS:SI处以NULL（0x00）结尾的字符串。

prtstr: lodsb
        and     al, al
        jz      fin
        call    prnt1           ; 显示al中的一个字符。
        jmp     prtstr
fin:    ret

; Routine to print a decimal value on screen, the value to be
; printed is put in al (i.e 0-255)
; 显示十进制数字的子程序。显示值放在寄存器al中（0--255）。

dprnt:  push    ax
        push    cx
        mov     ah, 0x00
        mov     cl, 0x0a
        idiv    cl
        cmp     al, 0x09
        jbe     lt100
        call    dprnt
        jmp     skip10
lt100:  add     al, 0x30
        call    prnt1
skip10: mov     al, ah
        add     al, 0x30
        call    prnt1
        pop     cx
        pop     ax
        ret

; Part of above routine, this one just prints ascii al
; 上面子程序的一部分。显示al中的一个字符。
; 该子程序使用中断0x10的0x0E功能，以电传方式在屏幕上写一个字符。光标会自动移动到下一个
; 位置处。如果写完一行光标就会移动到下一行开始处。如果已经写完一屏最后一行，则整个屏幕
; 会向上滚动一行。字符0x07（BEL）、0x08（BS）、0x0A（LF）和0x0D（CR）被作为命令不会显示。
; 输入：AL -- 欲写字符；BH -- 显示页号； BL -- 前景显示色（图形方式时）。

prnt1:  push    ax
        push    cx
        mov     bh, 0x00        ; 显示页面
        mov     cx, 0x01
        mov     ah, 0x0e
        int     0x10
        pop     cx
        pop     ax
        ret

; Prints <CR> + <LF>    ; 显示回车+换行。

docr:   push    ax
        push    cx
        mov     bh, 0x00
        mov     ah, 0x0e
        mov     al, 0x0a
        mov     cx, 0x01
        int     0x10
        mov     al, 0x0d
        int     0x10
        pop     cx
        pop     ax
        ret

; 全局描述表开始处。描述符表由多个8字节长的描述符项组成。这里给出了3个描述符项。
; 第1项无用，但须存在。第2项是系统代码描述符，第3项是系统数据描述符。
gdt:
        dw      0, 0, 0, 0      ; dummy         ; 第1个描述符，不用。

; 在GDT表中这里的偏移量是0x08。这是内核代码段选择符的值。
        dw      0x07FF          ; 8MB - limit=2047 (0--2047，因此是2048*4096=8MB)
        dw      0x0000          ; base address = 0
        dw      0x9A00          ; code read/exec        ; 代码段为只读、可执行。
        dw      0x00C0          ; granularity=4096, 286 ; 颗粒度为4096，32位模式。

; 在GDT表中这里的偏移量是0x10。它是内核数据段选择符的值。
        dw      0x07FF          ; 8MB - limit=2047 (2048*4096=8MB)
        dw      0x0000          ; base address=0
        dw      0x9200          ; data read/write       ; 数据段为可读可写。
        dw      0x00C0          ; granularity=4096, 386 ; 颗粒度为4096，32位模式。

; 下面是加载中断描述符表寄存器idtr的指令lidt要求的6字节操作数。前2字节是IDT表的
; 限长，后4字节是idt表的线性地址空间中的32位基地址。CPU要求在进入保护模式之前需设
; 置IDT表，因此这里设置一个长度为0的空表。
idt_48:
        dw      0               ; idt limit=0
        dw      0, 0            ; idt base=0L

; 这是加载全局描述符表寄存gdtr的指令lgdt要求的6字节操作数。前2字节是gdt表的限
; 长，后4字节是gdt表的线性基地址。这里全局表长度设置为2KB（0x7ff即可），因为每8
; 字节组成一个段描述符项，所以表中共可有256项。4字节的线性基地址为 0x0009<<16+
; 0x0200 + gdt, 即0x90200 + gdt。（符号gdt是全局表在本程序段中的偏移地址）
gdt_48:
        dw      0x800           ; gdt limit=2048, 256 GDT entries
        dw      512+gdt, 0x9    ; gdt base = 0x9xxxx

msg1:   db      "Press <RETURN> to see SVGA-modes available or any other key to continue."
        db      0x0d, 0x0a, 0x0a, 0x00
msg2:   db      "Mode: COLSxROWS:"
        db      0x0d, 0x0a, 0x0a, 0x00
msg3:   db      "Choose mode by pressing the corresponding number."
        db      0x0d, 0x0a, 0x00

; 下面是4个显示卡的特征数据串。
idati:          db      "761295520"
idcandt:        db      0xa5            ; 标号idcandt意思是 ID of Chip AND Tech.
idgenoa:        db      0x77, 0x00, 0x66, 0x99
idparadise:     db      "VGA="

; 下面是各种显示卡可使用的扩展模式个数和对应的模式号列表。其中每一行第1个字节是模式个
; 数值，随后的些值是中断0x10功能0（AH=0）可使用的模式号。例如，对于ATI牌子的显示卡，
; 除了标准模式以外还可使用两种扩展模式：0x23和0x33。
; Manufacturer:         Numbermodes:    Mode:
; 厂家：                 模式数量：        模式列表：

moati:          db      0x02,           0x23, 0x33
moahead:        db      0x05,           0x22, 0x23, 0x24, 0x2f, 0x34
mocandt:        db      0x02,           0x60, 0x61
mocirrus:       db      0x04,           0x1f, 0x20, 0x22, 0x31
moeverex:       db      0x0a,           0x03, 0x04, 0x07, 0x08, 0x0a, 0x0b, 0x16, 0x18, 0x21, 0x40
mogenoa:        db      0x0a,           0x58, 0x5a, 0x60, 0x61, 0x62, 0x63, 0x64, 0x72, 0x74, 0x78
moparadise:     db      0x02,           0x55, 0x54
motrident:      db      0x07,           0x50, 0x51, 0x52, 0x57, 0x58, 0x59, 0x5a
motseng:        db      0x05,           0x26, 0x2a, 0x23, 0x24, 0x22
movideo7:       db      0x06,           0x40, 0x43, 0x44, 0x41, 0x42, 0x45

; 下面是各种牌子VGA显示卡可使用的模式对应的列、行值列表。例如ATI显示卡两种扩展模式的
; 列、行值分别是 132 x 25、 132 x 44。
;               msb = Cols      lsb = Rows:
;               高字节 = 列数     低字节 = 行数：

dscati:         dw      0x8419, 0x842c
dscahead:       dw      0x842c, 0x8419, 0x841c, 0xa032, 0x5042
dsccandt:       dw      0x8419, 0x8432
dsccirrus:      dw      0x8419, 0x842c, 0x841e, 0x6425
dsceverex:      dw      0x5022, 0x503c, 0x642b, 0x644b, 0x8419, 0x842c, 0x501e, 0x641b, 0xa040, 0x841e
dscgenoa:       dw      0x5020, 0x642a, 0x8419, 0x841d, 0x8420, 0x842c, 0x843c, 0x503c, 0x5042, 0x644b
dscparadise:    dw      0x8419, 0x842b
dsctrident:     dw      0x501e, 0x502b, 0x503c, 0x8419, 0x841e, 0x842b, 0x843c
dsctseng:       dw      0x503c, 0x6428, 0x8419, 0x841c, 0x842c
dscvideo7:      dw      0x502b, 0x503c, 0x643c, 0x8419, 0x842c, 0x841c