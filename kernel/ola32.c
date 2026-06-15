/* Olal OS - integracao da arquitetura OLA-32 (nossa CPU) dentro do kernel.
   Inclui o emulador e roda um programa proprio (assembly OLA-32). */
#define OLA32_EMBED
#include "../arch/oemu.c"
#include "ola_prog.h"

/* roda o programa de demonstracao da OLA-32 e devolve a saida */
int ola32_demo(char *out, int max){
    int n = (int)(sizeof(ola_prog) / sizeof(ola_prog[0]));
    return ola32_exec(ola_prog, n, out, max);
}
