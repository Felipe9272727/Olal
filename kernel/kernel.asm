; ============================================================================
;  Olal OS - Kernel (modo protegido, 32 bits)
;  Carregado pelo bootloader em 0x1000 com a CPU ja em modo protegido.
;  Recursos:
;    - Driver de video VGA em modo texto (80x25): imprimir, cores, scroll,
;      backspace e cursor de hardware.
;    - Driver de teclado PS/2 por polling (mapa de scancodes US).
;    - Saida pela porta serial COM1 (usada para verificacao automatica).
;    - Mini-shell interativo que ecoa o que voce digita.
; ============================================================================

[bits 32]
[org 0x1000]                 ; mesmo endereco em que o bootloader nos carregou

VGA_MEM   equ 0xB8000        ; memoria de video em modo texto
VGA_COLS  equ 80
VGA_ROWS  equ 25
VGA_CELLS equ VGA_COLS * VGA_ROWS
COM1      equ 0x3F8

; ----------------------------------------------------------------------------
;  Ponto de entrada
; ----------------------------------------------------------------------------
kernel_main:
    call serial_init

    mov esi, serial_banner       ; marca de verificacao na porta serial
    call serial_print

    mov byte [current_color], 0x0F
    call clear_screen

    mov byte [current_color], 0x0A      ; verde claro
    mov esi, banner
    call print_string

    mov byte [current_color], 0x0F      ; branco
    mov esi, welcome
    call print_string

    call print_prompt

; --- laco principal: le o teclado e ecoa ---
.loop:
    call read_key
    test al, al
    jz .loop

    cmp al, 0x0A                 ; Enter?
    je .enter

    call print_char             ; ecoa o caractere na tela
    call serial_putc            ; e espelha na serial
    jmp .loop
.enter:
    mov al, 0x0A
    call print_char
    call print_prompt
    jmp .loop

; ----------------------------------------------------------------------------
;  Shell
; ----------------------------------------------------------------------------
print_prompt:
    pusha
    mov byte [current_color], 0x0B      ; ciano
    mov esi, prompt
    call print_string
    mov byte [current_color], 0x0F
    popa
    ret

; ----------------------------------------------------------------------------
;  Driver de video VGA
; ----------------------------------------------------------------------------

; Imprime um caractere (AL) tratando newline, backspace e scroll
print_char:
    pusha
    mov ebx, [cursor_pos]

    cmp al, 0x0A
    je .newline
    cmp al, 0x08
    je .backspace

    ; caractere imprimivel
    mov edx, ebx
    shl edx, 1                   ; offset = posicao * 2
    add edx, VGA_MEM
    mov ah, [current_color]
    mov [edx], al                ; byte do caractere
    mov [edx + 1], ah            ; byte de atributo (cor)
    inc ebx
    jmp .check_scroll

.newline:
    mov eax, ebx
    xor edx, edx
    mov ecx, VGA_COLS
    div ecx                      ; EDX = coluna atual
    sub ebx, edx                 ; volta para a coluna 0
    add ebx, VGA_COLS            ; desce uma linha
    jmp .check_scroll

.backspace:
    cmp ebx, 0
    je .done
    dec ebx
    mov edx, ebx
    shl edx, 1
    add edx, VGA_MEM
    mov byte [edx], ' '
    mov ah, [current_color]
    mov [edx + 1], ah
    jmp .done

.check_scroll:
    cmp ebx, VGA_CELLS
    jl .done
    call scroll_screen
    mov ebx, (VGA_ROWS - 1) * VGA_COLS

.done:
    mov [cursor_pos], ebx
    mov ecx, ebx
    call update_cursor
    popa
    ret

; Imprime string terminada em 0 (ESI = ponteiro)
print_string:
    pusha
.next:
    lodsb
    test al, al
    jz .end
    call print_char
    jmp .next
.end:
    popa
    ret

; Rola a tela uma linha para cima
scroll_screen:
    pusha
    mov esi, VGA_MEM + VGA_COLS * 2     ; segunda linha
    mov edi, VGA_MEM                    ; primeira linha
    mov ecx, (VGA_ROWS - 1) * VGA_COLS  ; celulas a mover
    rep movsw
    ; limpa a ultima linha
    mov edi, VGA_MEM + (VGA_ROWS - 1) * VGA_COLS * 2
    mov ecx, VGA_COLS
    mov ah, [current_color]
    mov al, ' '
.clear:
    mov [edi], ax
    add edi, 2
    loop .clear
    popa
    ret

; Limpa a tela inteira e zera o cursor
clear_screen:
    pusha
    mov edi, VGA_MEM
    mov ecx, VGA_CELLS
    mov ah, [current_color]
    mov al, ' '
.fill:
    mov [edi], ax
    add edi, 2
    loop .fill
    mov dword [cursor_pos], 0
    xor ecx, ecx
    call update_cursor
    popa
    ret

; Atualiza o cursor de hardware (ECX = posicao em celulas)
update_cursor:
    pusha
    mov ebx, ecx
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x3D5
    mov al, bl
    out dx, al
    mov dx, 0x3D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x3D5
    mov al, bh
    out dx, al
    popa
    ret

; ----------------------------------------------------------------------------
;  Driver de teclado PS/2 (polling)
;  Retorna em AL o ASCII da tecla, ou 0 se nada pronto / tecla nao mapeada.
; ----------------------------------------------------------------------------
read_key:
    in al, 0x64
    test al, 1                   ; buffer de saida cheio?
    jz .none
    in al, 0x60                  ; le o scancode
    test al, 0x80                ; bit alto = tecla solta -> ignora
    jnz .none
    movzx ebx, al
    mov al, [scancode_table + ebx]
    ret
.none:
    xor al, al
    ret

; ----------------------------------------------------------------------------
;  Driver serial COM1 (38400 8N1) - usado para verificacao
; ----------------------------------------------------------------------------
serial_init:
    mov dx, COM1 + 1
    xor al, al
    out dx, al                   ; desliga interrupcoes
    mov dx, COM1 + 3
    mov al, 0x80
    out dx, al                   ; habilita DLAB
    mov dx, COM1 + 0
    mov al, 0x03
    out dx, al                   ; divisor baixo (38400)
    mov dx, COM1 + 1
    xor al, al
    out dx, al                   ; divisor alto
    mov dx, COM1 + 3
    mov al, 0x03
    out dx, al                   ; 8 bits, sem paridade, 1 stop
    mov dx, COM1 + 2
    mov al, 0xC7
    out dx, al                   ; habilita e limpa FIFO
    mov dx, COM1 + 4
    mov al, 0x0B
    out dx, al
    ret

serial_putc:                     ; AL = caractere
    push edx
    push eax
    mov ah, al
.wait:
    mov dx, COM1 + 5
    in al, dx
    test al, 0x20                ; transmissor pronto?
    jz .wait
    mov al, ah
    mov dx, COM1
    out dx, al
    pop eax
    pop edx
    ret

serial_print:                    ; ESI = string terminada em 0
    pusha
.next:
    lodsb
    test al, al
    jz .end
    call serial_putc
    jmp .next
.end:
    popa
    ret

; ----------------------------------------------------------------------------
;  Dados
; ----------------------------------------------------------------------------
cursor_pos    dd 0
current_color db 0x0F

banner:
    db "================================================", 0x0A
    db "          Olal OS  -  v0.1  (32-bit)            ", 0x0A
    db "================================================", 0x0A, 0x0A, 0

welcome:
    db "Kernel em modo protegido de 32 bits ativo.", 0x0A
    db "Driver de video VGA e teclado PS/2 prontos.", 0x0A
    db "Digite algo (Enter para nova linha):", 0x0A, 0x0A, 0

prompt:
    db "Olal> ", 0

serial_banner:
    db "[OLAL] kernel online em modo protegido 32-bit", 0x0A, 0

; Mapa de scancodes (Set 1, layout US) -> ASCII
scancode_table:
    db 0,   0,   '1', '2', '3', '4', '5', '6'      ; 00-07
    db '7', '8', '9', '0', '-', '=', 0x08, 0x09    ; 08-0F  (0E=bksp 0F=tab)
    db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'      ; 10-17
    db 'o', 'p', '[', ']', 0x0A, 0,   'a', 's'     ; 18-1F  (1C=enter 1D=ctrl)
    db 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'      ; 20-27
    db "'", '`', 0,   '\', 'z', 'x', 'c', 'v'      ; 28-2F  (2A=lshift)
    db 'b', 'n', 'm', ',', '.', '/', 0,   '*'      ; 30-37  (36=rshift)
    db 0,   ' ', 0,   0,   0,   0,   0,   0        ; 38-3F  (39=espaco)
    times 128 - ($ - scancode_table) db 0
