/* Olal OS - IA (rede neural no kernel).
   Placeholder temporario: sera substituido por uma rede neural char-level real. */
#include "olal.h"

static char ctx[128];

void ai_reset(void){ ctx[0] = 0; }

void ai_feed(const char *prompt){
    strcpy_s(ctx, prompt, sizeof(ctx));
}

int ai_step(char *out, int maxlen){
    const char *r = "(rede neural sendo carregada...)";
    int i = 0;
    for(; r[i] && i < maxlen; i++) out[i] = r[i];
    return i;
}
