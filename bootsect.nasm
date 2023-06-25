BOOTSEQ  equ 0x7c0

extern _print
        
        call _print

; do_test:
;         ; in al, 0x64             ; 8042 status port      ; 读AT键盘控制状态寄存器。
;         mov al, 1
;         test al, 2 
;         lea ax, [msg1]
        ; lea cx, [go]
        ; jmp cx
        ; mov     ax, 0xf0
        ; shl     al, 1

; jmp BOOTSEQ:go
        ; dq 0x00, 0x00
        ; db 0x00

        ; fninit
        ; fstsw   ax
        ; fsetpm
        ; fplegacy

        xor     eax, eax
.1:     inc     eax
        mov     [0x000000], eax
        cmp     [0x100000], eax
        je      .1

align 4
        db 0x11, 0x22, 0x33, 0x44
; .1:      mov     ax, 0xbb

go:

        ; dw 0x00eb, 0x00eb
        ; jmp $+2
        ; repe    cmpsb

        jmp .1

        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0xfef4
.1:      mov     bx, 0xff

        lea ax, msg1
        call get_enter
        push 0xffff
        mov bp, sp
        call print_nl
        call print_hex
        hlt

get_enter:
nokey:  in      al, 0x60        ; 读取键盘控制缓冲中的扫描码
        mov     ah, 0
        push    ax
        mov     bp, sp
        call    print_nl
        call    print_hex
        pop     ax
        cmp     al, 0x82        ; 与最小断开码0x82比较。
        jb      nokey           ; 若小于0x82，表示还没有松按键松开。
        cmp     al, 0xe0
        ja      nokey           ; 若大于 0xe0，表示收到的是扩展扫描码前缀。
        cmp     al, 0x9c        ; 若断开码是0x9c表示用户按下/松开了回车键，
        jnz     get_enter       ; 于是程序跳转去检查系统是否具有SVGA模式。
        ; mov     ax, 0x019       ; 否则设置默认行列值 AL=25行、AH=80列。
        ; pop     ds
        ret

; Get video card data
        mov ah, 0x0f
        int 0x10
        push bx
        push ax
        mov bp, sp
        call print_nl
        call print_hex
        add bp, 2
        call print_nl
        call print_hex

; Get memory size (extended mem, kB) 
        mov ah, 0x88
        int 0x15
        push ax
        mov bp, sp
        call print_nl
        call print_hex
        hlt

int_1eh:
        push 0
        pop fs
        mov bx, 0x78
        lgs si, [fs:bx]     ; gs=fs,si=bx

        mov dx, 0xfef4
        mov di, dx          ; es:di is destination ; dx=0xfef4
        mov cx, 6           ; copy 12 bytes
        cld

        ; rep movsw word ptr es:[di], word ptr ds:[si]
        rep gs movsw

        mov di, dx
        mov byte [di+4], 0x23

        mov [fs:bx], di
        mov [fs:bx+2], es

clear_screen:
        mov ah, 0           ; Set video mode
        mov al, 0x03        ; AL = video mode
        int 0x10

show_msg1:
        xor dx, dx          ; DH = Row, DL = Column
        mov cx, 9           ; CX = Number of characters in string
        mov bh, 0x00        ; BH = Page Number
        mov bl, 0x03        ; BL = Color ; 0x03 = Cyan
        mov bp, msg1        ; ES:BP = Offset of string
        mov ah, 0x13        ; Write string (EGA+, meaning PC AT minimum)
        mov al, 0x01        ; AL = Write mode
        int 0x10
        hlt

test_jmp:
        mov cx, 0xfff0
        mov bx, 0x1f
        add cx, bx
        jle b
        ; jnc b
        ; jz b

        ; inc word [track]

        a:
        mov ax, 0x11
        b:
        mov ax, 0x22

;
;   print_all is for debugging purpose.
;   It will print out all of the registers. The assumption is that this is
;   called from a routine, with a stack frame like
;   dx
;   cx
;   bx
;   ax
;   error
;   ret <- sp
;
print_all:
        mov cx, 5                       ; error code + 4 registers
        mov bp, sp

print_loop:
        push cx                         ; save count left
        call print_nl                   ; nl for readability
        jnc no_reg                      ; see if register name is needed
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

;   print hex is for debugging purpose, and prints the word
;   pointed to by ss:bp in hexadecmial
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

msg1:
db 0x0d, 0x0a, 'Loading'

track: dw 0x00

times 510-($-$$) db 0x00
db 0x55, 0xaa