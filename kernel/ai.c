/* Olal OS - IA: rede neural char-level (MLP) rodando no kernel.
   Pesos gerados por tools/train.py (kernel/model.h). */
#include "olal.h"
#include "model.h"

static char prompt[64];
static u32 rng = 2463534242u;

static u32 rnd(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
static float frand(void){ return (rnd() >> 8) * (1.0f / 16777216.0f); }

/* exp(x) com x87/float: reducao para 2^y */
static float fexp(float x){
    if(x > 80.0f) x = 80.0f;
    if(x < -80.0f) x = -80.0f;
    float y = x * 1.4426950409f;            /* log2(e) */
    int i = (int)y;
    if((float)i > y) i--;                    /* floor */
    float f = y - (float)i;
    float p = 1.0f + f*(0.6931472f + f*(0.2402265f + f*(0.0555041f + f*0.0096181f)));
    if(i > 0){ while(i--) p *= 2.0f; }
    else { while(i++) p *= 0.5f; }
    return p;
}
static float ftanh(float x){
    float e = fexp(2.0f * x);
    return (e - 1.0f) / (e + 1.0f);
}

static int idx_of(char c){
    if(c >= 'A' && c <= 'Z') c += 32;
    for(int i = 0; i < M_V; i++) if(M_VOCAB[i] == c) return i;
    return 0;                                /* indice 0 = espaco */
}

static void forward(const int *ctx, float *logits){
    static float e[M_CTX*M_EMB];
    static float h[M_HID];
    for(int j = 0; j < M_CTX; j++)
        for(int k = 0; k < M_EMB; k++)
            e[j*M_EMB + k] = M_EMBW[ctx[j]*M_EMB + k];
    for(int i = 0; i < M_HID; i++){
        float s = M_B1[i];
        for(int n = 0; n < M_CTX*M_EMB; n++) s += e[n] * M_W1[n*M_HID + i];
        h[i] = ftanh(s);
    }
    for(int o = 0; o < M_V; o++){
        float s = M_B2[o];
        for(int i = 0; i < M_HID; i++) s += h[i] * M_W2[i*M_V + o];
        logits[o] = s;
    }
}

void ai_reset(void){ prompt[0] = 0; }

void ai_feed(const char *p){
    strcpy_s(prompt, p, sizeof(prompt));
    rng += inb(0x71) + 7919;                 /* mistura um pouco de entropia do RTC */
    if(rng == 0) rng = 1;
}

/* sementes coerentes (trechos do corpus, >= M_CTX chars) para o contexto */
static const char *SEEDS[] = {
    "a gente a entender melhor o mundo ",
    "o sistema operacional cuida da tela ",
    "eu sou uma rede neural pequena que ",
    "a tecnologia muda o mundo e nos ajuda ",
    "vamos conversar mais sobre o seu dia ",
    "o computador e uma maquina que segue ",
    "a curiosidade move o ser humano a ",
    "estudar um pouco a cada dia faz uma ",
    "a leitura abre portas para mundos novos ",
    "um amigo de verdade escuta e apoia ",
    "respira fundo vai dar tudo certo um ",
    "ajudar o proximo deixa o mundo um lugar ",
    "acredite mais no seu potencial voce e ",
    "a vida e feita de pequenos momentos que ",
};
#define NSEEDS (int)(sizeof(SEEDS)/sizeof(SEEDS[0]))

/* gera ate maxlen caracteres; varia a resposta conforme o prompt; coerente */
int ai_step(char *out, int maxlen){
    u32 h = 0;
    for(int i = 0; prompt[i]; i++) h = h*31 + (u8)prompt[i];
    const char *seed = SEEDS[(h + rnd()) % NSEEDS];
    int slen = strlen(seed);

    int ctx[M_CTX];
    for(int i = 0; i < M_CTX; i++){
        int p = slen - M_CTX + i;
        ctx[i] = idx_of(p >= 0 ? seed[p] : ' ');
    }

    float logits[M_V], ps[M_V];
    float temp = 0.5f;
    int n = 0;
    while(n < maxlen){
        forward(ctx, logits);
        float mx = logits[0];
        for(int i = 1; i < M_V; i++) if(logits[i] > mx) mx = logits[i];
        float sum = 0;
        for(int i = 0; i < M_V; i++){ ps[i] = fexp((logits[i]-mx)/temp); sum += ps[i]; }
        float r = frand() * sum;
        int pick = M_V - 1;
        for(int i = 0; i < M_V; i++){ r -= ps[i]; if(r <= 0){ pick = i; break; } }

        char c = M_VOCAB[pick];
        out[n++] = c;
        for(int i = 0; i < M_CTX-1; i++) ctx[i] = ctx[i+1];
        ctx[M_CTX-1] = pick;
        if((c == '.' || c == '?') && n > 24) break;
    }
    return n;
}
