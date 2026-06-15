; Olal OS - stubs de interrupcao (32 bits)
[bits 32]
global idt_load
global irq0_stub
global isr_default
global irq_default
extern timer_irq            ; tratador C do timer (faz o escalonamento)

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; IRQ0 (timer): salva contexto, chama o escalonador C (que troca o ESP),
; manda EOI e restaura o (novo) contexto.
irq0_stub:
    pushad
    push esp                ; passa o ESP atual para o C (ponteiro p/ stack salva)
    call timer_irq          ; retorna em EAX o novo ESP a usar
    mov esp, eax            ; troca de pilha (troca de tarefa)
    mov al, 0x20
    out 0x20, al            ; EOI ao PIC mestre
    popad
    iret

; exceções (0-31): congela de forma limpa para diagnostico
isr_default:
    cli
    hlt
    jmp isr_default

; outras IRQs (mascaradas, nao devem ocorrer): EOI e volta
irq_default:
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    iret
