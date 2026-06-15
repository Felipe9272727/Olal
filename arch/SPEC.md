# OLA-32 — arquitetura de CPU do Olal

Uma ISA RISC de 32 bits **criada do zero** para o Olal OS (a "nossa ARM").
Instruções de tamanho fixo (32 bits), registradores de 32 bits, modelo
load/store.

## Registradores
- `r0`..`r15` — 16 registradores de 32 bits. `r0` é **sempre 0** (escritas são
  ignoradas). `r15` é usado como *link register* por `jal`.
- `pc` — contador de programa (em número de instruções, não bytes).

## Memória
- Memória de dados separada, endereçada por **palavra** (índice de 32 bits).
- `lw`/`sw` acessam `mem[rs + imm]`.

## Formato das instruções (32 bits)
```
[31:26] opcode (6)
R: [25:22] rd  [21:18] rs  [17:14] rt  [13:0] funct
I: [25:22] rd  [21:18] rs  [17:0] imm (com sinal, 18 bits)
J: [25:0] addr
```

## Conjunto de instruções
| op   | mnem | tipo | efeito |
|------|------|------|--------|
| 0x00 | nop  | -    | nada |
| 0x01 | add  | R    | rd = rs + rt |
| 0x02 | sub  | R    | rd = rs - rt |
| 0x03 | and  | R    | rd = rs & rt |
| 0x04 | or   | R    | rd = rs \| rt |
| 0x05 | xor  | R    | rd = rs ^ rt |
| 0x06 | shl  | R    | rd = rs << rt |
| 0x07 | shr  | R    | rd = rs >> rt |
| 0x08 | slt  | R    | rd = (rs < rt) ? 1 : 0 (com sinal) |
| 0x09 | mul  | R    | rd = rs * rt |
| 0x10 | addi | I    | rd = rs + imm |
| 0x11 | li   | I    | rd = imm |
| 0x12 | lw   | I    | rd = mem[rs + imm] |
| 0x13 | sw   | I    | mem[rs + imm] = rd |
| 0x14 | beq  | I    | se rd == rs: pc += imm |
| 0x15 | bne  | I    | se rd != rs: pc += imm |
| 0x16 | jmp  | J    | pc = addr |
| 0x17 | jal  | J    | r15 = pc + 1; pc = addr |
| 0x18 | jr   | R    | pc = rs |
| 0x1F | sys  | I    | chamada de sistema (imm = código) |
| 0x3F | hlt  | -    | para a CPU |

### sys (I-type: rs = registrador, imm = código)
- imm = 1 → imprime o caractere em `rs` (byte baixo)
- imm = 2 → imprime o inteiro em `rs`
- imm = 0 → para

## Montador e emulador
- `arch/oasm.py` — montador (assembly OLA-32 → código de máquina).
- `arch/oemu.c` — emulador (roda o código de máquina). Compila standalone no
  PC e também é embutido no Olal OS como o app **OLA-32**.
