; ============================================================================
;  Olal OS - Bootloader (16 bits)
;  Carrega o kernel (C) do disco via leitura LBA estendida (int 13h / AH=42h),
;  habilita A20, entra em modo protegido de 32 bits e salta para o kernel.
; ============================================================================
[org 0x7C00]
[bits 16]

KERNEL_PHYS   equ 0x10000        ; endereco fisico do kernel
KERNEL_SEG    equ 0x1000         ; = KERNEL_PHYS / 16
LOAD_SECTORS  equ 600            ; setores a carregar (300 KB)

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

; Carrega LOAD_SECTORS setores a partir do LBA 1 para KERNEL_PHYS, em blocos
; de 64 setores, usando a leitura estendida da BIOS.
load_kernel:
    pusha
    mov ebx, 1                    ; LBA atual
    mov word [cur_seg], KERNEL_SEG
    mov bp, LOAD_SECTORS          ; setores restantes
.loop:
    cmp bp, 0
    je .done
    mov cx, 64
    cmp bp, cx
    jae .have
    mov cx, bp
.have:
    mov word [dap_count], cx
    mov word [dap_off], 0
    mov ax, [cur_seg]
    mov [dap_seg], ax
    mov [dap_lba], ebx
    mov dword [dap_lba + 4], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .error

    ; avanca LBA, segmento e contador
    movzx eax, cx
    add ebx, eax                 ; LBA += cx
    sub bp, cx                   ; restantes -= cx
    mov dx, 32
    mul dx                       ; ax = cx * 32 (paragrafos por setor)
    add [cur_seg], ax
    jmp .loop
.done:
    popa
    ret
.error:
    mov si, msg_err
    call print
    jmp $

enable_a20:
    in  al, 0x92
    or  al, 2
    out 0x92, al
    ret

; ---------------------------------------------------------------------------
;  GDT (flat: base 0, limite 4 GB)
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
dap:
    db 0x10                       ; tamanho do pacote
    db 0
dap_count:  dw 0
dap_off:    dw 0
dap_seg:    dw 0
dap_lba:    dd 0
            dd 0

msg_boot    db "Olal: carregando kernel...", 13, 10, 0
msg_err     db "Olal: ERRO de disco!", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
