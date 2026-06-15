/* Olal OS - kernel principal */
#include "olal.h"

/* ---- descoberta de video (PCI + Bochs VBE) ---- */
static u32 pci_read32(int bus,int slot,int fn,int off){
    outl(0xCF8, 0x80000000u|(bus<<16)|(slot<<11)|(fn<<8)|(off&0xFC));
    return inl(0xCFC);
}
static u32 find_lfb(void){
    for(int s=0;s<32;s++){
        if(pci_read32(0,s,0,0)==0xFFFFFFFF) continue;
        if((pci_read32(0,s,0,0x08)>>24)==0x03){
            u32 bar = pci_read32(0,s,0,0x10) & 0xFFFFFFF0u;
            if(bar) return bar;
        }
    }
    return 0xE0000000u;     /* fallback (Bochs/VBE) caso nao ache via PCI */
}
static void set_video(int w,int h){
    outw(0x1CE,4); outw(0x1CF,0);
    outw(0x1CE,1); outw(0x1CF,w);
    outw(0x1CE,2); outw(0x1CF,h);
    outw(0x1CE,3); outw(0x1CF,32);
    outw(0x1CE,4); outw(0x1CF,0x41);
}

/* ---- apps ---- */
typedef struct { const char *name; u32 color; const char *icon; } App;
static App apps[] = {
    {"Olal AI", 0x2BD17A, "AI"},
    {"Terminal",0x3B82F6, ">_"},
    {"Calc",    0xF59E0B, "="},
    {"Notas",   0xA855F7, "N"},
    {"Relogio", 0x22C3DD, "O"},
    {"Config",  0x94A3B8, "%"},
    {"Arquivos",0xEAB308, "F"},
    {"Paint",   0xEF4444, "P"},
    {"OLA-32",  0x8B5CF6, "C"},
};
#define NAPPS (int)(sizeof(apps)/sizeof(apps[0]))

#define GRID_X 28
#define GRID_Y 150
#define ICON   88
#define STEPX  112
#define STEPY  150

static void draw_status(void){
    char t[8]; rtc_time(t);
    gfx_rect(0, 0, SCRW, 44, 0x0B1220);
    gfx_text(18, 14, "olal", 0x2BD17A, 2);
    gfx_text(SCRW-18-gfx_textw(t,2), 14, t, 0xE5E7EB, 2);
    /* pontinhos de status */
    gfx_circle(SCRW-150, 22, 4, 0x2BD17A);
    gfx_round(SCRW-130, 16, 22, 12, 3, 0x2BD17A);
}

static void icon_pos(int i, int *x, int *y){
    *x = GRID_X + (i % 4) * STEPX;
    *y = GRID_Y + (i / 4) * STEPY;
}

static void draw_home(void){
    gfx_vgrad(0, 0, SCRW, SCRH, 0x16233D, 0x0A0E16);
    draw_status();
    gfx_text_center(SCRW/2, 80, "Bom dia", 0xF1F5F9, 3);

    for(int i = 0; i < NAPPS; i++){
        int x, y; icon_pos(i, &x, &y);
        /* sombra + icone */
        gfx_round(x+3, y+5, ICON, ICON, 22, 0x05070C);
        gfx_round(x, y, ICON, ICON, 22, apps[i].color);
        gfx_text_center(x+ICON/2, y+ICON/2-16, apps[i].icon, 0xFFFFFF, 3);
        gfx_text_center(x+ICON/2, y+ICON+8, apps[i].name, 0xCBD5E1, 1);
        if(ui_hit(x, y, ICON, ICON)) g_screen = i;
    }

    /* barra de navegacao inferior */
    gfx_round(SCRW/2-60, SCRH-26, 120, 8, 4, 0x334155);
}

static void draw_app(void){
    switch(g_screen){
        case 0: app_ai();       break;
        case 1: app_terminal(); break;
        case 2: app_calc();     break;
        case 3: app_notes();    break;
        case 4: app_clock();    break;
        case 5: app_config();   break;
        case 6: app_files();    break;
        case 7: app_paint();    break;
        case 8: app_ola32();    break;
        default: g_screen = -1; break;
    }
}

static void draw_cursor(void){
    /* touch real: so mostra um indicador enquanto o dedo esta encostado */
    if(!g_ptr.down) return;
    gfx_circle(g_ptr.x, g_ptr.y, 14, 0x2BD17A);
    gfx_circle(g_ptr.x, g_ptr.y, 10, 0x0A0E16);
}

void kmain(void){
    u32 lfb = find_lfb();
    set_video(SCRW, SCRH);
    gfx_init(lfb);
    ptr_init();

    for(;;){
        ptr_poll();
        if(g_screen < 0) draw_home();
        else draw_app();
        draw_cursor();
        gfx_present();
    }
}
