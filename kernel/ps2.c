/* Olal OS - entrada do ponteiro.
   Dois modos:
   - Touch absoluto (navegador): o wrapper JS escreve a coordenada exata do
     toque num "mailbox" em 0x5000 via write_memory do v86. Touch real:
     o ponto tocado e' a posicao, na hora, sem cursor viajando.
   - Mouse PS/2 relativo (QEMU): fallback por polling, para testar no PC. */
#include "olal.h"

Pointer g_ptr = { SCRW/2, SCRH/2, 0, 0 };

/* ---- mailbox de toque (preenchido pelo JS) ---- */
#define MB_MAGIC 0x4F4C4131u            /* "OLA1" */
typedef volatile struct {
    u32 magic;
    u32 seq;        /* incrementa a cada evento */
    i32 x, y;       /* posicao absoluta do toque */
    u32 down;       /* dedo encostado? */
    u32 clicks;     /* contador de toques concluidos (taps) */
} Mailbox;
static Mailbox *mb = (Mailbox*)0x5000;
static u32 last_clicks = 0;

/* ---- PS/2 (fallback) ---- */
static int cycle = 0;
static u8 pkt[3];
static int prev_down = 0;

static void wait_in(void){ for(int i=0;i<100000;i++) if(!(inb(0x64)&2)) return; }
static void wait_out(void){ for(int i=0;i<100000;i++) if(inb(0x64)&1) return; }
static void mwrite(u8 v){ wait_in(); outb(0x64,0xD4); wait_in(); outb(0x60,v); }
static u8   mread(void){ wait_out(); return inb(0x60); }

void ptr_init(void){
    mb->magic = 0;                                /* limpa o mailbox (lixo da RAM) */
    last_clicks = 0;
    wait_in(); outb(0x64, 0xA8);                  /* habilita aux */
    wait_in(); outb(0x64, 0x20);
    wait_out(); u8 cfg = inb(0x60);
    cfg = (cfg | 2) & ~0x20;
    wait_in(); outb(0x64, 0x60); wait_in(); outb(0x60, cfg);
    mwrite(0xF6); mread();
    mwrite(0xF4); mread();
}

void ptr_poll(void){
    g_ptr.clicked = 0;

    /* modo touch absoluto: o JS escreveu o magic */
    if(mb->magic == MB_MAGIC){
        int x = mb->x, y = mb->y;
        if(x < 0) x = 0; if(x > SCRW-1) x = SCRW-1;
        if(y < 0) y = 0; if(y > SCRH-1) y = SCRH-1;
        g_ptr.x = x; g_ptr.y = y;
        g_ptr.down = mb->down ? 1 : 0;
        u32 c = mb->clicks;
        if(c != last_clicks){ g_ptr.clicked = 1; last_clicks = c; }
        return;
    }

    /* fallback: mouse PS/2 relativo */
    int guard = 0;
    while((inb(0x64) & 1) && guard++ < 1024){
        u8 st = inb(0x64);
        if(!(st & 0x20)){ inb(0x60); continue; }
        u8 b = inb(0x60);
        if(cycle == 0 && !(b & 0x08)) continue;
        pkt[cycle++] = b;
        if(cycle == 3){
            cycle = 0;
            int dx = pkt[1]; if(pkt[0] & 0x10) dx |= 0xFFFFFF00;
            int dy = pkt[2]; if(pkt[0] & 0x20) dy |= 0xFFFFFF00;
            g_ptr.x += dx;
            g_ptr.y -= dy;
            if(g_ptr.x < 0) g_ptr.x = 0;
            if(g_ptr.x > SCRW-1) g_ptr.x = SCRW-1;
            if(g_ptr.y < 0) g_ptr.y = 0;
            if(g_ptr.y > SCRH-1) g_ptr.y = SCRH-1;
            g_ptr.down = pkt[0] & 1;
            if(g_ptr.down && !prev_down) g_ptr.clicked = 1;
            prev_down = g_ptr.down;
        }
    }
}
