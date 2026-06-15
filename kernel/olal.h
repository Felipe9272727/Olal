/* Olal OS - cabecalho comum: tipos, I/O de portas e prototipos */
#ifndef OLAL_H
#define OLAL_H
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint32_t size_t;

/* ----- I/O de portas ----- */
static inline void outb(u16 p, u8 v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline u8   inb(u16 p){ u8 r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outw(u16 p, u16 v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline u16  inw(u16 p){ u16 r; __asm__ volatile("inw %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outl(u16 p, u32 v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline u32  inl(u16 p){ u32 r; __asm__ volatile("inl %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void io_wait(void){ outb(0x80, 0); }

/* ----- util ----- */
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
int    strlen(const char *s);
char  *itoa(int v, char *buf);
void   strcpy_s(char *d, const char *s, int max);

/* ----- graficos ----- */
#define SCRW 480
#define SCRH 800
void gfx_init(u32 lfb);
void gfx_present(void);
void gfx_clear(u32 c);
void gfx_pixel(int x, int y, u32 c);
void gfx_blend(int x, int y, u32 c, u8 a);
void gfx_rect(int x, int y, int w, int h, u32 c);
void gfx_round(int x, int y, int w, int h, int r, u32 c);
void gfx_round_border(int x, int y, int w, int h, int r, u32 c, int t);
void gfx_vgrad(int x, int y, int w, int h, u32 top, u32 bot);
void gfx_circle(int cx, int cy, int r, u32 c);
void gfx_char(int x, int y, char ch, u32 c, int s);
void gfx_text(int x, int y, const char *t, u32 c, int s);
int  gfx_textw(const char *t, int s);
void gfx_text_center(int cx, int y, const char *t, u32 c, int s);

/* ----- entrada (mouse/touch + teclado) ----- */
typedef struct { int x, y; int down; int clicked; } Pointer;
void   ptr_init(void);
void   ptr_poll(void);
extern Pointer g_ptr;

/* ----- relogio ----- */
void rtc_time(char *out);   /* "HH:MM" */

#endif
