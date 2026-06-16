; ============================================================================
;  Olal OS - Bootloader (16 bits)
;  Carrega o kernel (C) do disco via leitura CHS multi-setor (int 13h/AH=02),
;  habilita A20, entra em modo protegido de 32 bits e salta para o kernel.
;  Bootavel como disquete (compativel com QEMU -fda e com o v86).
; ============================================================================
[org 0x7C00]
[bits 16]

KERNEL_PHYS   equ 0x10000        ; endereco fisico do kernel
KERNEL_SEG    equ 0x1000         ; = KERNEL_PHYS / 16
LOAD_SECTORS  equ 860        ; cobre o kernel maior (com motor JS)
SPT           equ 18             ; setores por trilha (disquete 1.44MB)
HEADS         equ 2

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [BOOT_DRIVE], dl

    mov si, msg_boot
    call print

    call load_kernel

    call enable_a20
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp CODE_SEG:pm_entry

; ---------------------------------------------------------------------------
print:                            ; SI = string terminada em 0
    pusha
    mov ah, 0x0E
.l:
    lodsb
    test al, al
    jz .e
    int 0x10
    jmp .l
.e:
    popa
    ret

; Carrega LOAD_SECTORS setores (a partir do LBA 1) para KERNEL_PHYS, lendo
; uma trilha de cada vez (CHS), de forma contigua na memoria.
load_kernel:
    pusha
    mov bp, LOAD_SECTORS          ; setores restantes
    mov di, 1                     ; LBA atual
    mov word [cur_seg], KERNEL_SEG
.next:
    cmp bp, 0
    je .done
    ; --- LBA (DI) -> CHS ---
    mov ax, di
    xor dx, dx
    mov cx, SPT
    div cx                        ; AX = trilha, DX = setor (0..17)
    inc dx
    mov [c_sec], dl               ; setor (1..18)
    xor dx, dx
    mov cx, HEADS
    div cx                        ; AX = cilindro, DX = cabeca
    mov [c_cyl], al
    mov [c_head], dl
    ; quantos setores ler: min(restante, fim-da-trilha, fim-do-bloco-de-64KB)
    mov al, SPT + 1
    sub al, [c_sec]
    movzx cx, al                  ; cx = disponiveis na trilha (1..18)
    mov ax, [cur_seg]
    and ax, 0x0FFF                ; paragrafo dentro do bloco de 64 KB
    mov bx, 32
    xor dx, dx
    div bx                        ; ax = setores ja usados no bloco
    mov bx, 128
    sub bx, ax                    ; bx = setores ate o limite de 64 KB
    cmp cx, bx
    jbe .c1
    mov cx, bx
.c1:
    cmp cx, bp
    jbe .c2
    mov cx, bp
.c2:
    mov [c_cnt], cx
    mov word [retry], 4
.try:
    mov ah, 0x02
    mov al, [c_cnt]               ; varios setores de uma vez
    mov ch, [c_cyl]
    mov cl, [c_sec]
    mov dh, [c_head]
    mov dl, [BOOT_DRIVE]
    mov bx, [cur_seg]
    mov es, bx
    xor bx, bx
    int 0x13
    jnc .ok
    xor ah, ah                    ; reset do controlador e tenta de novo
    mov dl, [BOOT_DRIVE]
    int 0x13
    dec word [retry]
    jnz .try
    mov si, msg_err
    call print
    jmp $
.ok:
    mov ax, [c_cnt]
    add di, ax                    ; LBA += cnt
    sub bp, ax                    ; restantes -= cnt
    mov dx, 32
    mul dx                        ; ax = cnt * 32 paragrafos
    add [cur_seg], ax
    jmp .next
.done:
    popa
    ret

enable_a20:
    in  al, 0x92
    or  al, 2
    out 0x92, al
    ret

; ---------------------------------------------------------------------------
gdt_start:
    dd 0, 0
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ---------------------------------------------------------------------------
[bits 32]
pm_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov eax, KERNEL_PHYS
    jmp eax

; ---------------------------------------------------------------------------
BOOT_DRIVE  db 0
cur_seg     dw 0
c_sec       db 0
c_head      db 0
c_cyl       db 0
c_cnt       dw 0
retry       dw 0

msg_boot    db "Olal: carregando kernel...", 13, 10, 0
msg_err     db "Olal: ERRO de disco!", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
