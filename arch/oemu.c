/* Emulador da arquitetura OLA-32 (criada para o Olal OS).
   Compila standalone no PC:  gcc arch/oemu.c -o oemu && ./oemu prog.bin
   Tambem e' incluido pelo kernel do Olal (app OLA-32) via OLA32_EMBED. */
#ifndef OLA32_EMBED
#include <stdio.h>
#include <stdlib.h>
#endif

typedef unsigned int u32;
typedef int i32;

/* escreve no buffer de saida (sem libc) */
static int oappend(char *out, int *len, int max, const char *s){
    while(*s && *len < max-1) out[(*len)++] = *s++;
    out[*len] = 0;
    return *len;
}
static void oint(char *out, int *len, int max, i32 v){
    char b[16]; int i = 0, neg = v < 0;
    u32 u = neg ? -v : v;
    if(!u) b[i++] = '0';
    while(u){ b[i++] = '0' + u % 10; u /= 10; }
    if(neg && *len < max-1) out[(*len)++] = '-';
    while(i && *len < max-1) out[(*len)++] = b[--i];
    out[*len] = 0;
}

/* roda 'n' instrucoes do programa; escreve saida em out; retorna o tamanho */
int ola32_exec(const u32 *prog, int n, char *out, int outmax){
    i32 reg[16]; u32 dmem[4096];
    for(int i = 0; i < 16; i++) reg[i] = 0;
    for(int i = 0; i < 4096; i++) dmem[i] = 0;
    int pc = 0, len = 0, steps = 0;
    out[0] = 0;

    while(pc >= 0 && pc < n && steps++ < 2000000){
        u32 w = prog[pc];
        int op = (w >> 26) & 0x3F;
        int rd = (w >> 22) & 0xF, rs = (w >> 18) & 0xF, rt = (w >> 14) & 0xF;
        i32 im = w & 0x3FFFF;
        if(im & 0x20000) im |= 0xFFFC0000;       /* sinal (18 bits) */
        u32 addr = w & 0x3FFFFFF;
        int next = pc + 1;

        switch(op){
            case 0x00: break;                                  /* nop */
            case 0x01: reg[rd] = reg[rs] + reg[rt]; break;     /* add */
            case 0x02: reg[rd] = reg[rs] - reg[rt]; break;     /* sub */
            case 0x03: reg[rd] = reg[rs] & reg[rt]; break;     /* and */
            case 0x04: reg[rd] = reg[rs] | reg[rt]; break;     /* or  */
            case 0x05: reg[rd] = reg[rs] ^ reg[rt]; break;     /* xor */
            case 0x06: reg[rd] = reg[rs] << reg[rt]; break;    /* shl */
            case 0x07: reg[rd] = (u32)reg[rs] >> reg[rt]; break;/* shr */
            case 0x08: reg[rd] = reg[rs] < reg[rt]; break;     /* slt */
            case 0x09: reg[rd] = reg[rs] * reg[rt]; break;     /* mul */
            case 0x10: reg[rd] = reg[rs] + im; break;          /* addi */
            case 0x11: reg[rd] = im; break;                    /* li */
            case 0x12: { u32 a = reg[rs] + im; if(a < 4096) reg[rd] = dmem[a]; } break; /* lw */
            case 0x13: { u32 a = reg[rs] + im; if(a < 4096) dmem[a] = reg[rd]; } break; /* sw */
            case 0x14: if(reg[rd] == reg[rs]) next = pc + 1 + im; break; /* beq */
            case 0x15: if(reg[rd] != reg[rs]) next = pc + 1 + im; break; /* bne */
            case 0x16: next = addr; break;                     /* jmp */
            case 0x17: reg[15] = pc + 1; next = addr; break;   /* jal */
            case 0x18: next = reg[rs]; break;                  /* jr */
            case 0x1F:                                          /* sys */
                if(im == 1){ char c[2] = { (char)(reg[rs] & 0xFF), 0 }; oappend(out, &len, outmax, c); }
                else if(im == 2) oint(out, &len, outmax, reg[rs]);
                else if(im == 0) pc = n;
                break;
            case 0x3F: pc = n; continue;                        /* hlt */
            default: break;
        }
        reg[0] = 0;                                             /* r0 sempre 0 */
        pc = next;
    }
    return len;
}

#ifndef OLA32_EMBED
int main(int argc, char **argv){
    if(argc < 2){ printf("uso: %s prog.bin\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if(!f){ printf("nao abriu %s\n", argv[1]); return 1; }
    static u32 prog[65536]; int n = 0;
    while(n < 65536 && fread(&prog[n], 4, 1, f) == 1) n++;
    fclose(f);
    static char out[8192];
    int len = ola32_exec(prog, n, out, sizeof(out));
    printf("%.*s\n", len, out);
    return 0;
}
#endif
