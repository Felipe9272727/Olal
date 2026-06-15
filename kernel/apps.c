/* Olal OS - aplicativos */
#include "olal.h"

/* desenha texto com quebra de linha numa regiao */
static void wrapped(int x, int y, int maxcols, const char *t, u32 col, int s){
    int cx = x, cy = y, c = 0;
    for(int i = 0; t[i]; i++){
        if(t[i] == '\n'){ cx = x; cy += 16*s; c = 0; continue; }
        gfx_char(cx, cy, t[i], col, s);
        cx += 8*s; c++;
        if(c >= maxcols){ cx = x; cy += 16*s; c = 0; }
    }
}

/* ================= Calculadora ================= */
static long acc = 0, cur = 0; static char op = 0; static int typing = 0;
static long compute(long a, char o, long b){
    switch(o){ case '+': return a+b; case '-': return a-b;
               case '*': return a*b; case '/': return b? a/b : 0; }
    return b;
}
void app_calc(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x111827, 0x0A0E16);
    ui_topbar("Calculadora");
    /* display */
    gfx_round(16, 70, SCRW-32, 90, 14, 0x1E293B);
    char b[20]; itoa(typing? cur : acc, b);
    gfx_text(SCRW-28-gfx_textw(b,4), 96, b, 0xF8FAFC, 4);

    const char *K = "789/456*123-C0=+";
    int kw = (SCRW-40)/4, kh = 96, x0 = 16, y0 = 180;
    for(int i = 0; i < 16; i++){
        int r = i/4, c = i%4;
        char lbl[2] = { K[i], 0 };
        char ch = K[i];
        u32 col = 0x1F2937;
        if(ch=='/'||ch=='*'||ch=='-'||ch=='+') col = 0x2563EB;
        else if(ch=='=') col = 0x2BD17A;
        else if(ch=='C') col = 0xDC2626;
        if(ui_button(x0 + c*(kw+5), y0 + r*(kh+5), kw, kh, lbl, col, 0xFFFFFF, 3)){
            if(ch>='0'&&ch<='9'){ cur = cur*10 + (ch-'0'); typing = 1; }
            else if(ch=='C'){ acc=cur=0; op=0; typing=0; }
            else if(ch=='='){ if(op) acc = compute(acc,op,cur); op=0; cur=0; typing=0; }
            else { if(op) acc = compute(acc,op,cur); else acc = cur; op = ch; cur=0; typing=0; }
        }
    }
}

/* ================= Notas ================= */
static char notes[1024]; static int nlen = 0;
void app_notes(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x14233B, 0x0A0E16);
    ui_topbar("Notas");
    gfx_round(10, 58, SCRW-20, 470, 12, 0x0E1626);
    notes[nlen] = 0;
    wrapped(22, 72, 54, notes, 0xE5E7EB, 1);
    /* cursor piscando simples */
    char c = osk_render(560);
    if(c == 8){ if(nlen>0) nlen--; }
    else if(c && nlen < (int)sizeof(notes)-2) notes[nlen++] = c;
}

/* ================= Terminal ================= */
static char term[2048] = "Olal OS terminal v0.4\nDigite 'help'.\n";
static int tlen = 35;
static char line[80]; static int llen = 0;
static void tputs(const char *s){ for(int i=0; s[i] && tlen<(int)sizeof(term)-2; i++) term[tlen++]=s[i]; }
static int streq(const char *a, const char *b){ int i=0; for(;a[i]&&b[i];i++) if(a[i]!=b[i]) return 0; return a[i]==b[i]; }
static void run_cmd(void){
    line[llen]=0;
    tputs("olal$ "); tputs(line); tputs("\n");
    if(streq(line,"help")) tputs("cmds: help clear date ver echo ls ai\n");
    else if(streq(line,"clear")){ tlen=0; term[0]=0; }
    else if(streq(line,"ver")) tputs("Olal OS v0.4 (32-bit, kernel em C)\n");
    else if(streq(line,"date")){ char d[12]; rtc_date(d); tputs(d); tputs("\n"); }
    else if(streq(line,"ls")) tputs("notas.txt  sistema/  olal.ai\n");
    else if(streq(line,"ai")) tputs("Abra o app 'Olal AI' :)\n");
    else if(line[0]=='e'&&line[1]=='c'&&line[2]=='h'&&line[3]=='o'&&line[4]==' ') { tputs(line+5); tputs("\n"); }
    else if(llen>0){ tputs("cmd nao encontrado: "); tputs(line); tputs("\n"); }
    llen=0;
}
void app_terminal(void){
    gfx_rect(0,0,SCRW,SCRH, 0x05080E);
    ui_topbar("Terminal");
    term[tlen]=0;
    wrapped(8, 60, 58, term, 0x4ADE80, 1);
    /* linha de entrada */
    line[llen]=0;
    char p[88]; int k=0; const char*pr="olal$ ";
    for(int i=0;pr[i];i++) p[k++]=pr[i];
    for(int i=0;i<llen;i++) p[k++]=line[i];
    p[k++]='_'; p[k]=0;
    gfx_text(8, 520, p, 0xE5E7EB, 1);
    char c = osk_render(560);
    if(c=='\n') run_cmd();
    else if(c==8){ if(llen>0) llen--; }
    else if(c && llen<78) line[llen++]=c;
}

/* ================= Relogio ================= */
void app_clock(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x0B1F3A, 0x05070C);
    ui_topbar("Relogio");
    char t[10]; rtc_full(t);
    gfx_text_center(SCRW/2, 330, t, 0x2BD17A, 6);
    char d[12]; rtc_date(d);
    gfx_text_center(SCRW/2, 420, d, 0x94A3B8, 2);
}

/* ================= Config ================= */
void app_config(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x111827, 0x0A0E16);
    ui_topbar("Config");
    gfx_round(16, 70, SCRW-32, 120, 14, 0x1E293B);
    gfx_text(34, 86, "Olal OS", 0xF8FAFC, 3);
    gfx_text(34, 128, "versao 0.4 - 32 bits", 0x94A3B8, 1);
    gfx_text(34, 150, "kernel em C + assembly", 0x94A3B8, 1);

    gfx_round(16, 210, SCRW-32, 70, 14, 0x1E293B);
    gfx_text(34, 226, "IA integrada (rede neural)", 0xE5E7EB, 1);
    gfx_text(34, 248, "roda no proprio kernel", 0x6EE7B7, 1);

    ui_button(16, 300, SCRW-32, 64, "Reiniciar", 0xDC2626, 0xFFFFFF, 2);
    if(ui_hit(16,300,SCRW-32,64)){ outb(0x64,0xFE); }
}

/* ================= Arquivos ================= */
void app_files(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x14233B, 0x0A0E16);
    ui_topbar("Arquivos");
    const char *names[] = {"notas.txt","sistema/","olal.ai","wallpaper.raw","readme.md"};
    const char *sz[] = {"1 KB","-","98 KB","1.5 MB","2 KB"};
    for(int i=0;i<5;i++){
        int y = 70 + i*80;
        gfx_round(16, y, SCRW-32, 68, 12, 0x1E293B);
        gfx_round(28, y+14, 40, 40, 8, 0x2563EB);
        gfx_text(40, y+22, "F", 0xFFFFFF, 2);
        gfx_text(84, y+14, names[i], 0xF1F5F9, 2);
        gfx_text(84, y+44, sz[i], 0x94A3B8, 1);
    }
}

/* ================= Paint ================= */
#define MAXPTS 6000
static struct { short x, y; u8 col; } pts[MAXPTS];
static int npts = 0; static int pcol = 0;
static const u32 PAL[6] = {0xEF4444,0x22C55E,0x3B82F6,0xEAB308,0xF8FAFC,0x111827};
void app_paint(void){
    gfx_rect(0,0,SCRW,SCRH, 0x0A0E16);
    ui_topbar("Paint");
    int cy0 = 58, ch = 580;
    gfx_rect(8, cy0, SCRW-16, ch, 0xF8FAFC);
    /* desenha pontos salvos */
    for(int i=0;i<npts;i++) gfx_circle(pts[i].x, pts[i].y, 5, PAL[pts[i].col]);
    /* desenhar com o dedo */
    if(g_ptr.down && g_ptr.y > cy0+6 && g_ptr.y < cy0+ch-6 && g_ptr.x>14 && g_ptr.x<SCRW-14 && npts<MAXPTS){
        pts[npts].x = g_ptr.x; pts[npts].y = g_ptr.y; pts[npts].col = pcol; npts++;
    }
    /* paleta */
    for(int i=0;i<6;i++){
        int x = 16 + i*60, y = SCRH-58;
        if(pcol==i) gfx_round(x-3, y-3, 54, 54, 12, 0xFFFFFF);
        gfx_round(x, y, 48, 48, 10, PAL[i]);
        if(ui_hit(x,y,48,48)) pcol = i;
    }
    if(ui_button(SCRW-90, SCRH-58, 78, 48, "limpar", 0x374151, 0xFFFFFF, 1)) npts = 0;
}

/* ================= Sistema (multitarefa) ================= */
void app_sistema(void){
    gfx_vgrad(0,0,SCRW,SCRH, 0x0B1F1A, 0x05070C);
    ui_topbar("Sistema");
    char b[20];

    gfx_round(16, 64, SCRW-32, 120, 12, 0x0E1A18);
    gfx_text(30, 78, "Uptime (s):", 0x94A3B8, 1);
    itoa((int)(g_ticks/100), b); gfx_text(190, 74, b, 0x6EE7B7, 2);
    gfx_text(30, 110, "Trocas de contexto:", 0x94A3B8, 1);
    itoa((int)g_switches, b); gfx_text(240, 106, b, 0xE5E7EB, 2);
    gfx_text(30, 142, "Tarefas ativas:", 0x94A3B8, 1);
    itoa(task_count(), b); gfx_text(200, 138, b, 0xFBBF24, 2);

    gfx_round(16, 200, SCRW-32, 150, 12, 0x0E1A18);
    gfx_text(30, 212, "contadores das tarefas:", 0xA7F3D0, 1);
    for(int i = 0; i < 3; i++){
        int y = 236 + i*36;
        gfx_text(30, y, "tarefa", 0x94A3B8, 2);
        itoa(i+1, b); gfx_text(30+7*16, y, b, 0x94A3B8, 2);
        itoa((int)g_work[i], b); gfx_text(170, y, b, 0x6EE7B7, 2);
    }

    if(ui_button(16, 380, (SCRW-40)/2, 70, "criar tarefa", 0x2563EB, 0xFFFFFF, 2)) task_spawn();
    if(ui_button(SCRW/2+4, 380, (SCRW-40)/2, 70, "parar", 0xDC2626, 0xFFFFFF, 2)) task_kill_demos();

    gfx_text(20, 480, "multitarefa preemptiva real:", 0x94A3B8, 1);
    gfx_text(20, 500, "o timer troca de tarefa 100x/s.", 0x94A3B8, 1);
    gfx_text(20, 520, "crie tarefas e veja a UI dividir a", 0x94A3B8, 1);
    gfx_text(20, 540, "CPU com elas (contadores subindo).", 0x94A3B8, 1);
}

/* ================= OLA-32 (nossa CPU) ================= */
void app_ola32(void){
    static char out[512]; static int ran = 0;
    if(!ran){ ola32_demo(out, sizeof(out)); ran = 1; }
    gfx_vgrad(0,0,SCRW,SCRH, 0x1A1030, 0x05070C);
    ui_topbar("OLA-32 CPU");
    gfx_text(20, 70, "Arquitetura criada do zero", 0xC4B5FD, 1);
    gfx_text(20, 90, "para o Olal: 16 regs, RISC 32-bit", 0x94A3B8, 1);

    gfx_round(16, 120, SCRW-32, 90, 12, 0x140C24);
    gfx_text(30, 134, "programa: fibonacci.s", 0xA78BFA, 1);
    gfx_text(30, 158, "rodando na nossa CPU:", 0x94A3B8, 1);
    gfx_text(30, 182, out, 0x6EE7B7, 2);

    gfx_round(16, 230, SCRW-32, 250, 12, 0x140C24);
    gfx_text(30, 244, "ola-32 assembly:", 0xA78BFA, 1);
    const char *code =
        "  li   r1, 0\n  li   r2, 1\n  li   r3, 12\nloop:\n"
        "  sys  r1, 2\n  add  r6, r1, r2\n  add  r1, r0, r2\n"
        "  add  r2, r0, r6\n  addi r4, r4, 1\n  bne  r4, r3, loop\n  hlt";
    gfx_text(30, 268, code, 0xCBD5E1, 1);

    gfx_text(20, 500, "a CPU, o montador e o emulador", 0x94A3B8, 1);
    gfx_text(20, 520, "sao 100% nossos (arch/).", 0x94A3B8, 1);
}

/* ================= Olal AI ================= */
static char chat[2048] = "Oi! Eu sou a Olal AI, uma rede neural pequena\nrodando dentro do kernel. Fale comigo:\n\n";
static int clen = 0;
static char ain[80]; static int ainlen = 0;
static void cputs(const char *s){ for(int i=0;s[i]&&clen<(int)sizeof(chat)-2;i++) chat[clen++]=s[i]; }
void app_ai(void){
    if(clen==0) clen = strlen(chat);
    gfx_vgrad(0,0,SCRW,SCRH, 0x0E2A20, 0x05070C);
    ui_topbar("Olal AI");
    gfx_round(8, 56, SCRW-16, 420, 12, 0x0A1A14);
    chat[clen]=0;
    wrapped(20, 68, 54, chat, 0xD1FAE5, 1);
    /* entrada */
    char p[88]; int k=0; const char*pr="voce: ";
    for(int i=0;pr[i];i++) p[k++]=pr[i];
    for(int i=0;i<ainlen;i++) p[k++]=ain[i];
    p[k++]='_'; p[k]=0;
    gfx_text(16, 490, p, 0x6EE7B7, 1);
    char c = osk_render(560);
    if(c=='\n'){
        ain[ainlen]=0;
        cputs("voce: "); cputs(ain); cputs("\n");
        cputs("olal: ");
        ai_feed(ain);
        char resp[160];
        int n = ai_step(resp, 150);
        resp[n]=0; cputs(resp); cputs("\n\n");
        ainlen=0;
    } else if(c==8){ if(ainlen>0) ainlen--; }
    else if(c && ainlen<78) ain[ainlen++]=c;
}
