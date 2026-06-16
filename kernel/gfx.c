/* Olal OS - biblioteca grafica (framebuffer 32bpp com double buffering) */
#include "olal.h"
#include "font8x16.h"

#define BACKBUF 0x00400000u            /* back buffer em 4 MB */

static u32 *FB;                        /* framebuffer real (LFB) */
static u32 *BB = (u32*)BACKBUF;        /* back buffer */

void gfx_init(u32 lfb){ FB = (u32*)lfb; }

u32 gfx_lighten(u32 c, int amt){
    int r = ((c>>16)&0xFF)+amt, g = ((c>>8)&0xFF)+amt, b = (c&0xFF)+amt;
    if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    if(r<0)r=0; if(g<0)g=0; if(b<0)b=0;
    return (r<<16)|(g<<8)|b;
}

void gfx_present(void){
    u32 n = SCRW * SCRH;
    __asm__ volatile("rep movsl" :: "S"(BB), "D"(FB), "c"(n) : "memory");
}

void gfx_clear(u32 c){
    u32 n = SCRW * SCRH;
    for(u32 i = 0; i < n; i++) BB[i] = c;
}

void gfx_pixel(int x, int y, u32 c){
    if((unsigned)x < SCRW && (unsigned)y < SCRH) BB[y*SCRW + x] = c;
}

void gfx_blend(int x, int y, u32 c, u8 a){
    if((unsigned)x >= SCRW || (unsigned)y >= SCRH) return;
    u32 d = BB[y*SCRW + x];
    u32 dr = (d>>16)&0xFF, dg = (d>>8)&0xFF, db = d&0xFF;
    u32 sr = (c>>16)&0xFF, sg = (c>>8)&0xFF, sb = c&0xFF;
    u32 rr = (sr*a + dr*(255-a)) / 255;
    u32 rg = (sg*a + dg*(255-a)) / 255;
    u32 rb = (sb*a + db*(255-a)) / 255;
    BB[y*SCRW + x] = (rr<<16)|(rg<<8)|rb;
}

void gfx_rect(int x, int y, int w, int h, u32 c){
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x+w > SCRW ? SCRW : x+w, y1 = y+h > SCRH ? SCRH : y+h;
    for(int yy = y0; yy < y1; yy++){
        u32 *row = &BB[yy*SCRW];
        for(int xx = x0; xx < x1; xx++) row[xx] = c;
    }
}

/* retangulo de cantos arredondados (raio r) */
void gfx_round(int x, int y, int w, int h, int r, u32 c){
    if(r*2 > w) r = w/2;
    if(r*2 > h) r = h/2;
    for(int yy = 0; yy < h; yy++){
        for(int xx = 0; xx < w; xx++){
            int dx = -1, dy = -1;
            if(xx < r && yy < r){ dx = r-1-xx; dy = r-1-yy; }
            else if(xx >= w-r && yy < r){ dx = xx-(w-r); dy = r-1-yy; }
            else if(xx < r && yy >= h-r){ dx = r-1-xx; dy = yy-(h-r); }
            else if(xx >= w-r && yy >= h-r){ dx = xx-(w-r); dy = yy-(h-r); }
            if(dx >= 0){
                if(dx*dx + dy*dy > r*r) continue;          /* fora do canto */
            }
            gfx_pixel(x+xx, y+yy, c);
        }
    }
}

void gfx_round_border(int x, int y, int w, int h, int r, u32 c, int t){
    gfx_round(x, y, w, h, r, c);
    /* desenha so a borda recortando o interior depois (uso simples: 2 chamadas) */
    (void)t;
}

void gfx_vgrad(int x, int y, int w, int h, u32 top, u32 bot){
    u32 tr=(top>>16)&0xFF, tg=(top>>8)&0xFF, tb=top&0xFF;
    u32 br=(bot>>16)&0xFF, bg=(bot>>8)&0xFF, bb=bot&0xFF;
    for(int yy = 0; yy < h; yy++){
        int num = yy, den = h>1?h-1:1;
        u32 r = tr + (int)(br-tr)*num/den;
        u32 g = tg + (int)(bg-tg)*num/den;
        u32 b = tb + (int)(bb-tb)*num/den;
        u32 c = (r<<16)|(g<<8)|b;
        gfx_rect(x, y+yy, w, 1, c);
    }
}

void gfx_circle(int cx, int cy, int r, u32 c){
    for(int yy = -r; yy <= r; yy++)
        for(int xx = -r; xx <= r; xx++)
            if(xx*xx + yy*yy <= r*r) gfx_pixel(cx+xx, cy+yy, c);
}

void gfx_char(int x, int y, char ch, u32 c, int s){
    if(ch < 32 || ch > 126) return;
    const u8 *g = font8x16[ch - 32];
    for(int row = 0; row < 16; row++){
        u8 bits = g[row];
        for(int col = 0; col < 8; col++){
            if(bits & (1<<col)){
                if(s == 1) gfx_pixel(x+col, y+row, c);
                else gfx_rect(x+col*s, y+row*s, s, s, c);
            }
        }
    }
}

void gfx_text(int x, int y, const char *t, u32 c, int s){
    int cx = x;
    for(int i = 0; t[i]; i++){
        if(t[i] == '\n'){ cx = x; y += 16*s; continue; }
        gfx_char(cx, y, t[i], c, s);
        cx += 8*s;
    }
}

int gfx_textw(const char *t, int s){ return strlen(t) * 8 * s; }

void gfx_text_center(int cx, int y, const char *t, u32 c, int s){
    gfx_text(cx - gfx_textw(t, s)/2, y, t, c, s);
}
