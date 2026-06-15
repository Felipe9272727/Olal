; Stub de entrada do kernel (32 bits). Configura a pilha e chama kmain().
[bits 32]
global _start
extern kmain

_start:
    mov esp, 0x90000
    fninit                  ; inicializa a FPU (x87) para a IA usar float
    call kmain
.hang:
    cli
    hlt
    jmp .hang
