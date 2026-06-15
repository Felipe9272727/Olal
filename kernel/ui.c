/* Olal OS - widgets de UI: botoes, barra de topo e teclado virtual */
#include "olal.h"

int g_screen = -1;        /* -1 = home, senao id do app */

int ui_inside(int x, int y, int w, int h){
    return g_ptr.x >= x && g_ptr.x < x+w && g_ptr.y >= y && g_ptr.y < y+h;
}
int ui_hit(int x, int y, int w, int h){
    return g_ptr.clicked && ui_inside(x, y, w, h);
}

int ui_button(int x, int y, int w, int h, const char *label, u32 col, u32 tcol, int ts){
    int hot = ui_inside(x, y, w, h) && g_ptr.down;
    gfx_round(x, y, w, h, 12, hot ? gfx_lighten(col, 40) : col);
    gfx_text_center(x+w/2, y+h/2-8*ts, label, tcol, ts);
    return ui_hit(x, y, w, h);
}

void ui_topbar(const char *title){
    gfx_rect(0, 0, SCRW, 50, 0x0B1220);
    if(ui_button(8, 8, 60, 34, "<", 0x1E293B, 0x2BD17A, 2)) g_screen = -1;
    gfx_text(84, 17, title, 0xF1F5F9, 2);
}

/* ---------- teclado virtual ---------- */
static const char *KB[4] = { "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static int shift = 0;

static int key(int x, int y, int w, const char *lbl, u32 col){
    int hot = ui_inside(x, y, w, 48) && g_ptr.down;
    gfx_round(x, y, w, 48, 8, hot ? gfx_lighten(col, 50) : col);
    gfx_text_center(x+w/2, y+16, lbl, 0xF8FAFC, 2);
    return ui_hit(x, y, w, 48);
}

/* desenha o teclado em y0 e retorna o caractere digitado (0 se nenhum) */
char osk_render(int y0){
    char out = 0;
    char buf[2] = {0,0};
    gfx_rect(0, y0-8, SCRW, SCRH-(y0-8), 0x0E1422);

    int rh = 52;
    for(int r = 0; r < 4; r++){
        int n = strlen(KB[r]);
        int startx = (SCRW - n*48) / 2;
        for(int i = 0; i < n; i++){
            char c = KB[r][i];
            if(shift && c >= 'a' && c <= 'z') c -= 32;
            buf[0] = c;
            if(key(startx + i*48, y0 + r*rh, 46, buf, 0x1F2937)) out = c;
        }
    }
    int yb = y0 + 4*rh;
    if(key(2,        y0+3*rh, 44, shift?"^":"v", shift?0x2BD17A:0x374151)) shift = !shift;
    if(key(SCRW-54,  y0+3*rh, 50, "<-", 0x374151)) out = 8;
    if(key(4,        yb, 300, "espaco", 0x1F2937)) out = ' ';
    if(key(312,      yb, SCRW-316, "Enter", 0x2BD17A)) out = '\n';
    return out;
}
