; ============================================================================
;  Olal OS - Bootloader (Estagio 1)
;  Roda em 16 bits (modo real), carregado pela BIOS em 0x7C00.
;  Tarefas:
;    1. Configurar segmentos e pilha
;    2. Carregar o kernel do disco para a memoria (0x1000)
;    3. Habilitar a linha A20
;    4. Carregar a GDT e entrar em modo protegido 32 bits
;    5. Saltar para o kernel
; ============================================================================

[org 0x7C00]                 ; a BIOS carrega o setor de boot aqui
[bits 16]                    ; comecamos em modo real (16 bits)

KERNEL_OFFSET  equ 0x1000    ; onde o kernel sera carregado na memoria
KERNEL_SECTORS equ 32        ; quantos setores (512B) do kernel carregar

; ----------------------------------------------------------------------------
start:
    cli
    xor ax, ax               ; zera os registradores de segmento
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00           ; pilha logo abaixo do bootloader
    sti

    mov [BOOT_DRIVE], dl      ; a BIOS deixa o numero do drive em DL

    mov si, msg_boot
    call print_string

    call load_kernel         ; le o kernel do disco

    mov si, msg_loaded
    call print_string

    call enable_a20          ; libera acesso acima de 1MB

    cli
    lgdt [gdt_descriptor]     ; carrega a tabela de descritores globais

    mov eax, cr0             ; liga o bit PE (Protected Mode Enable)
    or  eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:protected_mode   ; far jump -> esvazia o pipeline e entra em PM

; ----------------------------------------------------------------------------
;  Rotinas de 16 bits
; ----------------------------------------------------------------------------

; Imprime string terminada em 0 (SI = ponteiro) via servico de video da BIOS
print_string:
    pusha
    mov ah, 0x0E
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    popa
    ret

; Carrega KERNEL_SECTORS setores a partir do LBA 1 para ES:KERNEL_OFFSET.
; Le um setor por vez, convertendo LBA -> CHS (robusto entre trilhas).
load_kernel:
    pusha
    mov bx, KERNEL_OFFSET     ; ES:BX = destino
    mov di, 1                 ; LBA atual (setor 0 = boot, kernel comeca no 1)
    mov si, KERNEL_SECTORS    ; setores restantes
.read_loop:
    cmp si, 0
    je .done

    mov ax, di
    call lba_to_chs           ; preenche abs_cyl / abs_head / abs_sector

    mov ah, 0x02              ; BIOS: ler setores
    mov al, 1                 ; um setor por vez
    mov ch, [abs_cyl]
    mov cl, [abs_sector]
    mov dh, [abs_head]
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .disk_error

    add bx, 512               ; avanca o buffer
    inc di                    ; proximo LBA
    dec si
    jmp .read_loop
.done:
    popa
    ret
.disk_error:
    mov si, msg_disk_err
    call print_string
    jmp $

; Converte LBA (AX) -> CHS para um disquete (18 setores/trilha, 2 cabecas)
lba_to_chs:
    push bx
    push ax
    xor dx, dx
    mov bx, 18                ; setores por trilha
    div bx                    ; AX = LBA/18 , DX = LBA mod 18
    inc dx
    mov [abs_sector], dl      ; setor = (LBA mod 18) + 1
    xor dx, dx
    mov bx, 2                 ; numero de cabecas
    div bx                    ; AX = cilindro , DX = cabeca
    mov [abs_head], dl
    mov [abs_cyl], al
    pop ax
    pop bx
    ret

; Habilita a linha A20 pelo metodo rapido (porta 0x92)
enable_a20:
    in  al, 0x92
    or  al, 2
    out 0x92, al
    ret

; ----------------------------------------------------------------------------
;  GDT - Global Descriptor Table (modelo flat: base 0, limite 4GB)
; ----------------------------------------------------------------------------
gdt_start:
gdt_null:
    dd 0x0
    dd 0x0
gdt_code:                     ; segmento de codigo 0x08
    dw 0xFFFF                 ; limite (bits 0-15)
    dw 0x0000                 ; base  (bits 0-15)
    db 0x00                   ; base  (bits 16-23)
    db 10011010b              ; presente, anel 0, codigo, exec/read
    db 11001111b              ; granularidade 4K, 32 bits + limite (16-19)
    db 0x00                   ; base  (bits 24-31)
gdt_data:                     ; segmento de dados 0x10
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b              ; presente, anel 0, dados, read/write
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ----------------------------------------------------------------------------
;  Codigo de 32 bits (ja em modo protegido)
; ----------------------------------------------------------------------------
[bits 32]
protected_mode:
    mov ax, DATA_SEG          ; recarrega os segmentos com o seletor de dados
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000          ; pilha do kernel bem acima do codigo

    jmp KERNEL_OFFSET         ; passa o controle ao kernel

; ----------------------------------------------------------------------------
;  Dados
; ----------------------------------------------------------------------------
BOOT_DRIVE   db 0
abs_sector   db 0
abs_head     db 0
abs_cyl      db 0

msg_boot     db "Olal: iniciando bootloader...", 13, 10, 0
msg_loaded   db "Olal: kernel carregado, entrando em modo protegido", 13, 10, 0
msg_disk_err db "Olal: ERRO de leitura de disco!", 13, 10, 0

; Assinatura do setor de boot
times 510 - ($ - $$) db 0
dw 0xAA55
