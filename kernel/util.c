/* Olal OS - utilidades basicas (freestanding) */
#include "olal.h"

void *memset(void *d, int c, size_t n){
    u8 *p = (u8*)d;
    while(n--) *p++ = (u8)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n){
    u8 *pd = (u8*)d; const u8 *ps = (const u8*)s;
    while(n--) *pd++ = *ps++;
    return d;
}

int strlen(const char *s){
    int n = 0;
    while(s[n]) n++;
    return n;
}

void strcpy_s(char *d, const char *s, int max){
    int i = 0;
    for(; s[i] && i < max-1; i++) d[i] = s[i];
    d[i] = 0;
}

/* inteiro com sinal -> string decimal */
char *itoa(int v, char *buf){
    char tmp[16];
    int i = 0, neg = 0;
    unsigned int u;
    if(v < 0){ neg = 1; u = (unsigned int)(-v); } else u = (unsigned int)v;
    if(u == 0) tmp[i++] = '0';
    while(u){ tmp[i++] = '0' + (u % 10); u /= 10; }
    int j = 0;
    if(neg) buf[j++] = '-';
    while(i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}
