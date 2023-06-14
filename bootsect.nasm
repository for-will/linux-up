BOOTSEQ  equ 0x7c0

jmp BOOTSEQ:go

go:
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0xfef4

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

msg1:
db 0x0d, 0x0a, 'Loading'
times 510-($-$$) db 0x00
db 0x55, 0xaa