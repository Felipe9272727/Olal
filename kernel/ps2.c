/* Olal OS - mouse PS/2 (polling). No celular, o toque vira movimento+clique. */
#include "olal.h"

Pointer g_ptr = { SCRW/2, SCRH/2, 0, 0 };

static int cycle = 0;
static u8 pkt[3];
static int prev_down = 0;

static void wait_in(void){ for(int i=0;i<100000;i++) if(!(inb(0x64)&2)) return; }
static void wait_out(void){ for(int i=0;i<100000;i++) if(inb(0x64)&1) return; }
static void mwrite(u8 v){ wait_in(); outb(0x64,0xD4); wait_in(); outb(0x60,v); }
static u8   mread(void){ wait_out(); return inb(0x60); }

void ptr_init(void){
    wait_in(); outb(0x64, 0xA8);                 /* habilita aux */
    wait_in(); outb(0x64, 0x20);                 /* le config */
    wait_out(); u8 cfg = inb(0x60);
    cfg = (cfg | 2) & ~0x20;                      /* IRQ12 on, clock on */
    wait_in(); outb(0x64, 0x60); wait_in(); outb(0x60, cfg);
    mwrite(0xF6); mread();                         /* defaults */
    mwrite(0xF4); mread();                         /* enable reporting */
}

void ptr_poll(void){
    g_ptr.clicked = 0;
    int guard = 0;
    while((inb(0x64) & 1) && guard++ < 64){
        u8 st = inb(0x64);
        if(!(st & 0x20)){ inb(0x60); continue; }   /* dado do teclado: ignora */
        u8 b = inb(0x60);
        if(cycle == 0 && !(b & 0x08)) continue;    /* sincroniza pacote */
        pkt[cycle++] = b;
        if(cycle == 3){
            cycle = 0;
            int dx = pkt[1]; if(pkt[0] & 0x10) dx |= 0xFFFFFF00;
            int dy = pkt[2]; if(pkt[0] & 0x20) dy |= 0xFFFFFF00;
            g_ptr.x += dx;
            g_ptr.y -= dy;                          /* eixo Y invertido */
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
