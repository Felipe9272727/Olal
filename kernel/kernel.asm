; ============================================================================
;  Olal OS - Kernel grafico estilo Android (modo protegido, 32 bits)
;  Modo de video VGA 13h: 320x200, 256 cores, framebuffer linear em 0xA0000.
;  Recursos:
;    - Double buffering (desenha em 0x30000 e copia para a tela)
;    - Driver de mouse PS/2 (no celular, o TOQUE vira clique)
;    - Fonte 8x8 propria (font8x8.inc) para desenhar texto
;    - Tela inicial com barra de status e grade de "apps"
;    - Teclado virtual na tela
;    - Apps: Terminal, Calculadora, Relogio (RTC), Notas, Sobre, Reiniciar
; ============================================================================

[bits 32]
[org 0x1000]

VGA      equ 0xA0000          ; framebuffer da tela
BACKBUF  equ 0x30000          ; buffer de fundo (desenhamos aqui primeiro)
SW       equ 320              ; largura da tela
SH       equ 200              ; altura da tela
COM1     equ 0x3F8

; --- paleta personalizada (indices) ---
C_BLACK  equ 0
C_WHITE  equ 15
C_BG     equ 16
C_BAR    equ 17
C_GREEN  equ 18
C_BLUE   equ 20
C_ORANGE equ 21
C_RED    equ 22
C_PURPLE equ 23
C_GREY   equ 24
C_DGREY  equ 25
C_KEY    equ 26
C_KEYHI  equ 27
C_CYAN   equ 29

CURSOR_W equ 8
CURSOR_H equ 12

; ============================================================================
;  Entrada do kernel
; ============================================================================
kernel_main:
    cld
    call serial_init
    mov esi, serial_banner
    call serial_print

    mov dword [g_scale], 1
    mov dword [mouse_x], 160
    mov dword [mouse_y], 100
    mov byte  [calc_op], '+'

    call set_palette
    call mouse_init
    mov dword [state], 0

.loop:
    call mouse_poll
    mov al, [mouse_click]
    test al, al
    jz .render
    mov byte [mouse_click], 0
    call handle_click
.render:
    call render
    call draw_cursor
    call present
    call delay
    jmp .loop

; ============================================================================
;  Render: despacha conforme o estado atual
; ============================================================================
render:
    mov eax, [state]
    cmp eax, 0
    je render_home
    cmp eax, 1
    je render_text
    cmp eax, 4
    je render_text
    cmp eax, 2
    je render_calc
    cmp eax, 3
    je render_clock
    cmp eax, 5
    je render_about
    ret

; ---- Tela inicial ----------------------------------------------------------
render_home:
    mov byte [g_color], C_BG
    call clear_back
    call draw_statusbar
    mov dword [tmp_i], 0
.iloop:
    mov eax, [tmp_i]
    cmp eax, 6
    jge .done
    call draw_icon
    inc dword [tmp_i]
    jmp .iloop
.done:
    ret

draw_statusbar:
    pusha
    mov dword [g_x], 0
    mov dword [g_y], 0
    mov dword [g_w], SW
    mov dword [g_h], 14
    mov byte  [g_color], C_BAR
    call fill_rect
    mov dword [g_scale], 1
    mov dword [g_x], 4
    mov dword [g_y], 3
    mov byte  [g_color], C_GREEN
    mov esi, str_olal
    call draw_string
    call get_time
    mov dword [g_x], SW - 8*8 - 4
    mov dword [g_y], 3
    mov byte  [g_color], C_WHITE
    mov esi, time_str
    call draw_string
    popa
    ret

draw_icon:
    pusha
    mov esi, [tmp_i]
    mov eax, [icon_cx + esi*4]   ; centro x
    mov ecx, [icon_cy + esi*4]   ; centro y
    mov ebx, eax
    sub ebx, 25
    mov [g_x], ebx
    mov ebx, ecx
    sub ebx, 25
    mov [g_y], ebx
    mov dword [g_w], 50
    mov dword [g_h], 50
    movzx ebx, byte [icon_col + esi]
    mov [g_color], bl
    call fill_rect
    ; rotulo centralizado abaixo do icone
    mov esi, [tmp_i]
    mov esi, [icon_lbl + esi*4]
    call strlen                  ; ecx = tamanho
    shl ecx, 2                   ; tamanho * 4 (metade de 8)
    mov esi, [tmp_i]
    mov eax, [icon_cx + esi*4]
    sub eax, ecx
    mov [g_x], eax
    mov esi, [tmp_i]
    mov eax, [icon_cy + esi*4]
    add eax, 30
    mov [g_y], eax
    mov dword [g_scale], 1
    mov byte  [g_color], C_WHITE
    mov esi, [tmp_i]
    mov esi, [icon_lbl + esi*4]
    call draw_string
    popa
    ret

; ---- App de texto (Terminal / Notas) --------------------------------------
render_text:
    mov byte [g_color], C_BG
    call clear_back
    call draw_backbar
    ; area de texto
    mov dword [g_x], 4
    mov dword [g_y], 16
    mov dword [g_w], 312
    mov dword [g_h], 98
    mov byte  [g_color], C_DGREY
    call fill_rect
    ; conteudo
    mov ecx, [textlen]
    mov byte [textbuf + ecx], 0
    mov dword [g_scale], 1
    mov dword [g_x], 6
    mov dword [g_y], 20
    mov byte  [g_color], C_WHITE
    mov esi, textbuf
    call draw_text_area
    call draw_vkeyboard
    ret

; ---- Calculadora -----------------------------------------------------------
render_calc:
    mov byte [g_color], C_BG
    call clear_back
    call draw_backbar
    mov dword [g_x], 4
    mov dword [g_y], 16
    mov dword [g_w], 312
    mov dword [g_h], 42
    mov byte  [g_color], C_DGREY
    call fill_rect
    ; valor a mostrar
    mov eax, [calc_typing]
    test eax, eax
    jz .acc
    mov eax, [calc_cur]
    jmp .conv
.acc:
    mov eax, [calc_acc]
.conv:
    mov edi, numbuf
    call int_to_str
    mov esi, numbuf
    call strlen
    mov eax, ecx
    imul eax, 24                 ; 8 * escala(3)
    mov ebx, 312
    sub ebx, eax
    sub ebx, 6
    mov [g_x], ebx
    mov dword [g_y], 26
    mov dword [g_scale], 3
    mov byte  [g_color], C_WHITE
    mov esi, numbuf
    call draw_string
    mov dword [g_scale], 1
    call draw_calc_keys
    ret

draw_calc_keys:
    pusha
    xor ebx, ebx                 ; indice 0..15
.l:
    cmp ebx, 16
    jge .done
    mov eax, ebx
    xor edx, edx
    mov ecx, 4
    div ecx                      ; eax=linha, edx=coluna
    mov esi, edx
    imul esi, 78
    add esi, 4
    mov [g_x], esi
    mov edi, eax
    imul edi, 34
    add edi, 64
    mov [g_y], edi
    mov dword [g_w], 72
    mov dword [g_h], 30
    mov al, [calc_keys + ebx]
    mov dl, C_KEY
    cmp al, '+'
    je .op
    cmp al, '-'
    je .op
    cmp al, '*'
    je .op
    cmp al, '='
    je .op
    cmp al, 'C'
    je .clr
    jmp .setc
.op:
    mov dl, C_GREEN
    jmp .setc
.clr:
    mov dl, C_RED
.setc:
    mov [g_color], dl
    call fill_rect
    ; rotulo da tecla
    mov al, [calc_keys + ebx]
    mov [g_char], al
    mov esi, [g_x]
    add esi, 32
    mov [g_x], esi
    mov edi, [g_y]
    add edi, 11
    mov [g_y], edi
    mov dword [g_scale], 1
    mov byte  [g_color], C_WHITE
    call draw_char_scaled
    inc ebx
    jmp .l
.done:
    popa
    ret

; ---- Relogio (le o RTC via CMOS) ------------------------------------------
render_clock:
    mov byte [g_color], C_BG
    call clear_back
    call draw_backbar
    call get_time
    mov dword [g_scale], 3
    mov dword [g_x], 52
    mov dword [g_y], 90
    mov byte  [g_color], C_GREEN
    mov esi, time_str
    call draw_string
    mov dword [g_scale], 1
    mov dword [g_x], 120
    mov dword [g_y], 60
    mov byte  [g_color], C_WHITE
    mov esi, str_relogio
    call draw_string
    ret

; ---- Sobre -----------------------------------------------------------------
render_about:
    mov byte [g_color], C_BG
    call clear_back
    call draw_backbar
    mov dword [g_scale], 1
    mov dword [g_x], 8
    mov dword [g_y], 28
    mov byte  [g_color], C_WHITE
    mov esi, about_text
    call draw_text_area
    ret

draw_backbar:
    pusha
    mov dword [g_x], 0
    mov dword [g_y], 0
    mov dword [g_w], SW
    mov dword [g_h], 14
    mov byte  [g_color], C_BAR
    call fill_rect
    mov dword [g_scale], 1
    mov dword [g_x], 4
    mov dword [g_y], 3
    mov byte  [g_color], C_GREEN
    mov esi, str_back
    call draw_string
    popa
    ret

; ============================================================================
;  Tratamento de clique (toque)
; ============================================================================
handle_click:
    mov eax, [state]
    cmp eax, 0
    je hc_home
    ; em um app: botao "voltar" (canto superior esquerdo)
    mov eax, [mouse_y]
    cmp eax, 16
    jge .inapp
    mov eax, [mouse_x]
    cmp eax, 60
    jge .inapp
    mov dword [state], 0
    ret
.inapp:
    mov eax, [state]
    cmp eax, 1
    je hc_text
    cmp eax, 4
    je hc_text
    cmp eax, 2
    je hc_calc
    ret

hc_home:
    mov dword [tmp_i], 0
.l:
    mov eax, [tmp_i]
    cmp eax, 6
    jge .done
    mov esi, eax
    mov ecx, [icon_cx + esi*4]
    mov edx, [icon_cy + esi*4]
    mov eax, [mouse_x]
    mov ebx, ecx
    sub ebx, 25
    cmp eax, ebx
    jl .next
    add ebx, 50
    cmp eax, ebx
    jg .next
    mov eax, [mouse_y]
    mov ebx, edx
    sub ebx, 25
    cmp eax, ebx
    jl .next
    add ebx, 50
    cmp eax, ebx
    jg .next
    mov eax, [tmp_i]
    call open_app
    ret
.next:
    inc dword [tmp_i]
    jmp .l
.done:
    ret

; eax = indice do icone (0..5)
open_app:
    cmp eax, 5
    je .reboot
    cmp eax, 1                   ; calculadora -> reseta estado
    jne .nocalc
    mov dword [calc_acc], 0
    mov dword [calc_cur], 0
    mov dword [calc_typing], 0
    mov byte  [calc_op], '+'
.nocalc:
    inc eax                      ; indice+1 = id do estado
    mov [state], eax
    ret
.reboot:
    call do_reboot
    ret

hc_text:
    call vkb_hit
    test al, al
    jz .done
    call text_append
.done:
    ret

text_append:
    cmp al, 0x08
    je .bs
    mov ecx, [textlen]
    cmp ecx, 600
    jge .done
    mov edi, textbuf
    add edi, ecx
    mov [edi], al
    inc dword [textlen]
    ret
.bs:
    mov ecx, [textlen]
    test ecx, ecx
    jz .done
    dec dword [textlen]
.done:
    ret

hc_calc:
    mov eax, [mouse_y]
    cmp eax, 64
    jl .none
    sub eax, 64
    xor edx, edx
    mov ecx, 34
    div ecx
    cmp eax, 4
    jge .none
    mov [tmp_row], eax
    mov eax, [mouse_x]
    cmp eax, 4
    jl .none
    sub eax, 4
    xor edx, edx
    mov ecx, 78
    div ecx
    cmp eax, 4
    jge .none
    cmp edx, 72
    jge .none
    mov ebx, [tmp_row]
    imul ebx, 4
    add ebx, eax
    mov al, [calc_keys + ebx]
    call calc_input
.none:
    ret

calc_input:
    cmp al, '0'
    jl .nd
    cmp al, '9'
    jg .nd
    movzx ebx, al
    sub ebx, '0'
    mov eax, [calc_cur]
    imul eax, 10
    add eax, ebx
    mov [calc_cur], eax
    mov dword [calc_typing], 1
    ret
.nd:
    cmp al, 'C'
    je .clear
    cmp al, '='
    je .eq
    cmp al, '+'
    je .setop
    cmp al, '-'
    je .setop
    cmp al, '*'
    je .setop
    ret
.setop:
    call calc_apply
    mov [calc_op], al
    mov dword [calc_cur], 0
    mov dword [calc_typing], 0
    ret
.eq:
    call calc_apply
    mov byte [calc_op], '+'
    mov dword [calc_cur], 0
    mov dword [calc_typing], 0
    ret
.clear:
    mov dword [calc_acc], 0
    mov dword [calc_cur], 0
    mov byte  [calc_op], '+'
    mov dword [calc_typing], 0
    ret

calc_apply:
    push eax
    push ebx
    mov ebx, [calc_cur]
    mov eax, [calc_acc]
    cmp byte [calc_op], '-'
    je .sub
    cmp byte [calc_op], '*'
    je .mul
    add eax, ebx
    jmp .store
.sub:
    sub eax, ebx
    jmp .store
.mul:
    imul eax, ebx
.store:
    mov [calc_acc], eax
    pop ebx
    pop eax
    ret

; ============================================================================
;  Teclado virtual
; ============================================================================
draw_vkeyboard:
    pusha
    mov esi, kb_row0
    mov dword [kb_sx], 0
    mov dword [kb_cnt], 10
    mov dword [kb_y], 118
    call draw_key_row
    mov esi, kb_row1
    mov dword [kb_sx], 0
    mov dword [kb_cnt], 10
    mov dword [kb_y], 134
    call draw_key_row
    mov esi, kb_row2
    mov dword [kb_sx], 16
    mov dword [kb_cnt], 9
    mov dword [kb_y], 150
    call draw_key_row
    mov esi, kb_row3
    mov dword [kb_sx], 48
    mov dword [kb_cnt], 7
    mov dword [kb_y], 166
    call draw_key_row
    ; linha especial
    mov dword [g_x], 0
    mov dword [g_y], 182
    mov dword [g_w], 60
    mov dword [g_h], 15
    mov byte  [g_color], C_KEY
    call fill_rect
    mov dword [g_scale], 1
    mov dword [g_x], 18
    mov dword [g_y], 186
    mov byte  [g_color], C_WHITE
    mov esi, str_bs
    call draw_string
    mov dword [g_x], 64
    mov dword [g_y], 182
    mov dword [g_w], 156
    mov dword [g_h], 15
    mov byte  [g_color], C_KEY
    call fill_rect
    mov dword [g_x], 224
    mov dword [g_y], 182
    mov dword [g_w], 95
    mov dword [g_h], 15
    mov byte  [g_color], C_KEYHI
    call fill_rect
    mov dword [g_x], 240
    mov dword [g_y], 186
    mov byte  [g_color], C_WHITE
    mov esi, str_enter
    call draw_string
    popa
    ret

draw_key_row:
    pusha
    xor ebx, ebx
.l:
    cmp ebx, [kb_cnt]
    jge .done
    mov eax, ebx
    imul eax, 32
    add eax, [kb_sx]
    mov [g_x], eax
    mov ecx, eax                 ; guarda x do retangulo
    mov eax, [kb_y]
    mov [g_y], eax
    mov dword [g_w], 30
    mov dword [g_h], 15
    mov byte  [g_color], C_KEY
    call fill_rect
    mov al, [esi + ebx]
    mov [g_char], al
    mov eax, ecx
    add eax, 11
    mov [g_x], eax
    mov eax, [kb_y]
    add eax, 4
    mov [g_y], eax
    mov dword [g_scale], 1
    mov byte  [g_color], C_WHITE
    call draw_char_scaled
    inc ebx
    jmp .l
.done:
    popa
    ret

; Retorna em AL o caractere da tecla tocada, ou 0
vkb_hit:
    push ebx
    push ecx
    push edx
    push esi
    mov eax, [mouse_y]
    cmp eax, 118
    jl .none
    cmp eax, 197
    jge .none
    sub eax, 118
    xor edx, edx
    mov ecx, 16
    div ecx
    cmp eax, 0
    je .r0
    cmp eax, 1
    je .r1
    cmp eax, 2
    je .r2
    cmp eax, 3
    je .r3
    jmp .r4
.r0:
    mov esi, kb_row0
    mov ebx, 0
    mov ecx, 10
    jmp .crow
.r1:
    mov esi, kb_row1
    mov ebx, 0
    mov ecx, 10
    jmp .crow
.r2:
    mov esi, kb_row2
    mov ebx, 16
    mov ecx, 9
    jmp .crow
.r3:
    mov esi, kb_row3
    mov ebx, 48
    mov ecx, 7
    jmp .crow
.crow:
    mov eax, [mouse_x]
    sub eax, ebx
    jl .none
    xor edx, edx
    push ecx
    mov ecx, 32
    div ecx
    pop ecx
    cmp eax, ecx
    jge .none
    cmp edx, 30
    jge .none
    movzx eax, byte [esi + eax]
    jmp .ret
.r4:
    mov eax, [mouse_x]
    cmp eax, 64
    jl .bs
    cmp eax, 224
    jl .space
    mov eax, 0x0A
    jmp .ret
.bs:
    mov eax, 0x08
    jmp .ret
.space:
    mov eax, ' '
    jmp .ret
.none:
    xor eax, eax
.ret:
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret

; ============================================================================
;  Primitivas graficas (desenham no BACKBUF)
; ============================================================================
clear_back:
    pusha
    mov edi, BACKBUF
    mov ecx, SW*SH
    mov al, [g_color]
    rep stosb
    popa
    ret

present:
    pusha
    mov esi, BACKBUF
    mov edi, VGA
    mov ecx, SW*SH/4
    rep movsd
    popa
    ret

fill_rect:                       ; g_x,g_y,g_w,g_h,g_color
    pusha
    mov ebx, [g_y]
.rl:
    mov eax, [g_y]
    add eax, [g_h]
    cmp ebx, eax
    jge .done
    mov eax, ebx
    imul eax, SW
    add eax, [g_x]
    add eax, BACKBUF
    mov edi, eax
    mov ecx, [g_w]
    mov al, [g_color]
    rep stosb
    inc ebx
    jmp .rl
.done:
    popa
    ret

; Desenha um caractere (g_char) com escala (g_scale) em g_x,g_y, cor g_color
draw_char_scaled:
    pusha
    movzx eax, byte [g_char]
    cmp eax, 32
    jb .done
    cmp eax, 126
    ja .done
    sub eax, 32
    shl eax, 3
    add eax, font8x8
    mov esi, eax                 ; ponteiro do glifo
    xor ebx, ebx                 ; linha 0..7
.row:
    cmp ebx, 8
    jge .done
    movzx ebp, byte [esi + ebx]  ; bits da linha
    xor ecx, ecx                 ; coluna 0..7
.col:
    cmp ecx, 8
    jge .nextrow
    bt ebp, ecx
    jnc .skip
    mov eax, ecx
    imul eax, [g_scale]
    add eax, [g_x]
    mov [blk_x], eax
    mov eax, ebx
    imul eax, [g_scale]
    add eax, [g_y]
    mov [blk_y], eax
    xor edx, edx                 ; dy
.fy:
    cmp edx, [g_scale]
    jge .skip
    mov eax, [blk_y]
    add eax, edx
    imul eax, SW
    add eax, [blk_x]
    add eax, BACKBUF
    mov edi, eax
    push ecx
    mov ecx, [g_scale]
    mov al, [g_color]
    rep stosb
    pop ecx
    inc edx
    jmp .fy
.skip:
    inc ecx
    jmp .col
.nextrow:
    inc ebx
    jmp .row
.done:
    popa
    ret

; Desenha string terminada em 0 (ESI), avancando em g_x; trata 0x0A
draw_string:
    pusha
    mov eax, [g_x]
    mov [g_strx0], eax
    mov ebx, [g_x]
    mov eax, [g_scale]
    shl eax, 3
    mov [g_step], eax
.next:
    lodsb
    test al, al
    jz .done
    cmp al, 0x0A
    je .nl
    mov [g_char], al
    mov [g_x], ebx
    call draw_char_scaled
    add ebx, [g_step]
    jmp .next
.nl:
    mov ebx, [g_strx0]
    mov eax, [g_y]
    add eax, [g_step]
    mov [g_y], eax
    jmp .next
.done:
    popa
    ret

; Desenha texto com quebra de linha automatica numa area (escala 1)
draw_text_area:                  ; ESI = string
    pusha
    mov eax, [g_x]
    mov [g_textx0], eax
    mov ebx, [g_x]
    mov edx, [g_y]
.next:
    lodsb
    test al, al
    jz .done
    cmp al, 0x0A
    je .nl
    mov [g_char], al
    mov [g_x], ebx
    mov [g_y], edx
    mov dword [g_scale], 1
    call draw_char_scaled
    add ebx, 8
    cmp ebx, 312
    jl .next
.nl:
    mov ebx, [g_textx0]
    add edx, 9
    cmp edx, 110
    jge .done
    jmp .next
.done:
    popa
    ret

; Cursor do mouse (setinha)
draw_cursor:
    pusha
    mov esi, cursor_data
    xor ebx, ebx                 ; linha
.r:
    cmp ebx, CURSOR_H
    jge .done
    xor ecx, ecx                 ; coluna
.c:
    cmp ecx, CURSOR_W
    jge .nr
    movzx eax, byte [esi]
    inc esi
    test eax, eax
    jz .skip
    mov edx, [mouse_x]
    add edx, ecx
    cmp edx, SW
    jge .skip
    mov edi, [mouse_y]
    add edi, ebx
    cmp edi, SH
    jge .skip
    push eax
    mov eax, edi
    imul eax, SW
    add eax, edx
    add eax, BACKBUF
    mov edi, eax
    pop eax
    cmp eax, 1
    jne .white
    mov byte [edi], C_BLACK
    jmp .skip
.white:
    mov byte [edi], C_WHITE
.skip:
    inc ecx
    jmp .c
.nr:
    inc ebx
    jmp .r
.done:
    popa
    ret

; ============================================================================
;  Utilidades
; ============================================================================
strlen:                          ; ESI -> ECX
    push esi
    xor ecx, ecx
.l:
    mov al, [esi]
    test al, al
    jz .e
    inc ecx
    inc esi
    jmp .l
.e:
    pop esi
    ret

; Converte inteiro com sinal (EAX) em string em EDI
int_to_str:
    push eax
    push ebx
    push ecx
    push edx
    test eax, eax
    jns .pos
    mov byte [edi], '-'
    inc edi
    neg eax
.pos:
    xor ecx, ecx
    mov ebx, 10
    test eax, eax
    jnz .dl
    mov byte [edi], '0'
    inc edi
    jmp .term
.dl:
    test eax, eax
    jz .emit
    xor edx, edx
    div ebx
    add dl, '0'
    push edx
    inc ecx
    jmp .dl
.emit:
    cmp ecx, 0
    je .term
    pop edx
    mov [edi], dl
    inc edi
    dec ecx
    jmp .emit
.term:
    mov byte [edi], 0
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Le um registrador do CMOS: AL = registrador -> AL = valor
cmos_read:
    out 0x70, al
    in  al, 0x71
    ret

bcd2bin:                         ; AL (BCD) -> AL (binario)
    push ebx
    push ecx
    movzx ebx, al
    mov ecx, ebx
    and ecx, 0x0F
    shr ebx, 4
    imul ebx, 10
    add ecx, ebx
    mov eax, ecx
    pop ecx
    pop ebx
    ret

put2:                            ; EAX (0..99) -> 2 digitos em [EDI], avanca EDI
    push eax
    push ebx
    push edx
    xor edx, edx
    mov ebx, 10
    div ebx
    add al, '0'
    mov [edi], al
    inc edi
    add dl, '0'
    mov [edi], dl
    inc edi
    pop edx
    pop ebx
    pop eax
    ret

; Le o RTC e monta time_str = "HH:MM:SS"
get_time:
    pusha
    mov al, 0x04
    call cmos_read
    call bcd2bin
    mov bl, al                   ; horas
    mov al, 0x02
    call cmos_read
    call bcd2bin
    mov bh, al                   ; minutos
    mov al, 0x00
    call cmos_read
    call bcd2bin
    mov dl, al                   ; segundos
    mov edi, time_str
    movzx eax, bl
    call put2
    mov byte [edi], ':'
    inc edi
    movzx eax, bh
    call put2
    mov byte [edi], ':'
    inc edi
    movzx eax, dl
    call put2
    mov byte [edi], 0
    popa
    ret

; Programa a paleta DAC com as cores personalizadas
set_palette:
    pusha
    mov esi, palette_data
.next:
    mov al, [esi]
    cmp al, 0xFF
    je .done
    mov dx, 0x3C8
    out dx, al
    inc esi
    mov dx, 0x3C9
    mov al, [esi]
    out dx, al
    inc esi
    mov al, [esi]
    out dx, al
    inc esi
    mov al, [esi]
    out dx, al
    inc esi
    jmp .next
.done:
    popa
    ret

do_reboot:
    mov al, 0xFE
    out 0x64, al
    cli
    hlt
    jmp $

delay:
    push ecx
    mov ecx, 250000
.l:
    dec ecx
    jnz .l
    pop ecx
    ret

; ============================================================================
;  Driver de mouse PS/2 (polling)
; ============================================================================
ps2_wait_in:                     ; espera o buffer de entrada esvaziar
    push eax
    push ecx
    mov ecx, 100000
.w:
    in al, 0x64
    test al, 0x02
    jz .ok
    dec ecx
    jnz .w
.ok:
    pop ecx
    pop eax
    ret

ps2_wait_out:                    ; espera ter dado no buffer de saida
    push eax
    push ecx
    mov ecx, 100000
.w:
    in al, 0x64
    test al, 0x01
    jnz .ok
    dec ecx
    jnz .w
.ok:
    pop ecx
    pop eax
    ret

mouse_write:                     ; AL = byte a enviar ao mouse
    push eax
    call ps2_wait_in
    mov al, 0xD4
    out 0x64, al
    call ps2_wait_in
    pop eax
    out 0x60, al
    ret

mouse_read_ack:
    call ps2_wait_out
    in al, 0x60
    ret

mouse_init:
    call ps2_wait_in
    mov al, 0xA8                 ; habilita o dispositivo auxiliar (mouse)
    out 0x64, al
    ; le e ajusta o byte de configuracao
    call ps2_wait_in
    mov al, 0x20
    out 0x64, al
    call ps2_wait_out
    in al, 0x60
    and al, 0xDD                 ; sem IRQ12 (polling) e clock do mouse ligado
    mov bl, al
    call ps2_wait_in
    mov al, 0x60
    out 0x64, al
    call ps2_wait_in
    mov al, bl
    out 0x60, al
    ; mouse: restaura padroes e habilita envio de dados
    mov al, 0xF6
    call mouse_write
    call mouse_read_ack
    mov al, 0xF4
    call mouse_write
    call mouse_read_ack
    ret

mouse_poll:
    pusha
.again:
    in al, 0x64
    test al, 0x01
    jz .done
    test al, 0x20                ; dado vindo do mouse?
    jz .kbd
    in al, 0x60
    movzx ebx, byte [mouse_cycle]
    cmp ebx, 0
    je .b0
    cmp ebx, 1
    je .b1
    mov [mouse_dy_raw], al
    mov byte [mouse_cycle], 0
    call mouse_apply
    jmp .again
.b0:
    test al, 0x08                ; bit 3 = pacote valido
    jz .again
    mov [mouse_flags], al
    mov byte [mouse_cycle], 1
    jmp .again
.b1:
    mov [mouse_dx_raw], al
    mov byte [mouse_cycle], 2
    jmp .again
.kbd:
    in al, 0x60                  ; consome bytes do teclado fisico
    mov [last_scancode], al
    jmp .again
.done:
    popa
    ret

mouse_apply:
    pusha
    ; eixo X
    movzx eax, byte [mouse_dx_raw]
    mov bl, [mouse_flags]
    test bl, 0x10
    jz .xp
    or eax, 0xFFFFFF00
.xp:
    add [mouse_x], eax
    mov eax, [mouse_x]
    cmp eax, 0
    jge .x1
    mov dword [mouse_x], 0
    jmp .ydo
.x1:
    cmp eax, SW-1
    jle .ydo
    mov dword [mouse_x], SW-1
.ydo:
    ; eixo Y (invertido)
    movzx eax, byte [mouse_dy_raw]
    mov bl, [mouse_flags]
    test bl, 0x20
    jz .yp
    or eax, 0xFFFFFF00
.yp:
    neg eax
    add [mouse_y], eax
    mov eax, [mouse_y]
    cmp eax, 0
    jge .y1
    mov dword [mouse_y], 0
    jmp .btn
.y1:
    cmp eax, SH-1
    jle .btn
    mov dword [mouse_y], SH-1
.btn:
    ; deteccao de borda de subida do botao esquerdo
    mov bl, [mouse_flags]
    and bl, 0x01
    mov bh, [mouse_btn_prev]
    mov [mouse_btn_prev], bl
    cmp bl, 1
    jne .nc
    cmp bh, 0
    jne .nc
    mov byte [mouse_click], 1
.nc:
    popa
    ret

; ============================================================================
;  Driver serial COM1 (usado para verificacao automatica)
; ============================================================================
serial_init:
    mov dx, COM1 + 1
    xor al, al
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x80
    out dx, al
    mov dx, COM1 + 0
    mov al, 0x03
    out dx, al
    mov dx, COM1 + 1
    xor al, al
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x03
    out dx, al
    mov dx, COM1 + 2
    mov al, 0xC7
    out dx, al
    mov dx, COM1 + 4
    mov al, 0x0B
    out dx, al
    ret

serial_putc:
    push edx
    push eax
    mov ah, al
.wait:
    mov dx, COM1 + 5
    in al, dx
    test al, 0x20
    jz .wait
    mov al, ah
    mov dx, COM1
    out dx, al
    pop eax
    pop edx
    ret

serial_print:                    ; ESI
    pusha
.n:
    lodsb
    test al, al
    jz .e
    call serial_putc
    jmp .n
.e:
    popa
    ret

; ============================================================================
;  Dados
; ============================================================================
state          dd 0
g_x            dd 0
g_y            dd 0
g_w            dd 0
g_h            dd 0
g_scale        dd 1
g_step         dd 8
g_strx0        dd 0
g_textx0       dd 0
g_color        db 15
g_char         db 0
blk_x          dd 0
blk_y          dd 0
tmp_i          dd 0
tmp_row        dd 0

mouse_x        dd 160
mouse_y        dd 100
mouse_cycle    db 0
mouse_flags    db 0
mouse_dx_raw   db 0
mouse_dy_raw   db 0
mouse_btn_prev db 0
mouse_click    db 0
last_scancode  db 0

kb_sx          dd 0
kb_cnt         dd 0
kb_y           dd 0

calc_acc       dd 0
calc_cur       dd 0
calc_typing    dd 0
calc_op        db '+'

textlen        dd 0
textbuf        times 640 db 0
numbuf         times 16  db 0
time_str       times 16  db 0

str_olal       db "Olal", 0
str_back       db "< Olal", 0
str_relogio    db "Relogio (RTC)", 0
str_bs         db "<-", 0
str_enter      db "Enter", 0

kb_row0        db "1234567890", 0
kb_row1        db "qwertyuiop", 0
kb_row2        db "asdfghjkl", 0
kb_row3        db "zxcvbnm", 0

calc_keys      db "789+456-123*C0= "

serial_banner  db "[OLAL] kernel grafico online em modo protegido 32-bit", 0x0A, 0

about_text:
    db "Olal OS  v0.2", 0x0A
    db "Sistema em Assembly x86", 0x0A
    db "Modo protegido 32 bits", 0x0A
    db "Video VGA 320x200", 0x0A
    db "Mouse/Touch PS-2", 0x0A
    db "Teclado virtual", 0x0A, 0x0A
    db "Toque < Olal p/ voltar", 0

; --- icones da tela inicial ---
icon_cx  dd 53, 160, 267, 53, 160, 267
icon_cy  dd 55, 55, 55, 135, 135, 135
icon_col db C_BLUE, C_ORANGE, C_GREEN, C_PURPLE, C_CYAN, C_RED
icon_lbl dd lbl_term, lbl_calc, lbl_clock, lbl_notes, lbl_about, lbl_reboot

lbl_term   db "Terminal", 0
lbl_calc   db "Calc", 0
lbl_clock  db "Relogio", 0
lbl_notes  db "Notas", 0
lbl_about  db "Sobre", 0
lbl_reboot db "Reboot", 0

; --- paleta: indice, R, G, B (0..63) ; 0xFF termina ---
palette_data:
    db 16,  4,  7, 12
    db 17,  2,  3,  6
    db 18, 15, 55, 33
    db 20, 16, 34, 58
    db 21, 63, 40,  8
    db 22, 58, 20, 24
    db 23, 38, 20, 56
    db 24, 40, 44, 50
    db 25, 14, 16, 22
    db 26, 28, 32, 40
    db 27, 18, 55, 34
    db 29, 10, 52, 56
    db 0xFF

; --- cursor do mouse: 0=transparente 1=contorno 2=preenchimento ---
cursor_data:
    db 1,0,0,0,0,0,0,0
    db 1,1,0,0,0,0,0,0
    db 1,2,1,0,0,0,0,0
    db 1,2,2,1,0,0,0,0
    db 1,2,2,2,1,0,0,0
    db 1,2,2,2,2,1,0,0
    db 1,2,2,2,2,2,1,0
    db 1,2,2,2,2,2,2,1
    db 1,2,2,2,1,1,1,1
    db 1,2,1,2,1,0,0,0
    db 1,1,0,1,2,1,0,0
    db 0,0,0,1,2,1,0,0

%include "kernel/font8x8.inc"
