; OLA-32 demo: imprime os primeiros 12 numeros de Fibonacci
; (rodando na arquitetura OLA-32, criada do zero para o Olal OS)
      li   r1, 0          ; a
      li   r2, 1          ; b
      li   r3, 12         ; quantos
      li   r4, 0          ; i
      li   r5, 32         ; ' ' (espaco)
loop:
      sys  r1, 2          ; imprime a (inteiro)
      sys  r5, 1          ; imprime espaco
      add  r6, r1, r2     ; r6 = a + b
      add  r1, r0, r2     ; a = b
      add  r2, r0, r6     ; b = r6
      addi r4, r4, 1      ; i++
      bne  r4, r3, loop   ; repete ate i == 12
      hlt
