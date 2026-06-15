/* Olal OS - leitura do relogio de tempo real (CMOS) */
#include "olal.h"

static u8 cmos(u8 reg){ outb(0x70, reg); return inb(0x71); }
static u8 bcd(u8 v){ return (v & 0x0F) + ((v >> 4) * 10); }

/* preenche "HH:MM" */
void rtc_time(char *out){
    u8 h = bcd(cmos(0x04));
    u8 m = bcd(cmos(0x02));
    out[0] = '0' + h/10; out[1] = '0' + h%10;
    out[2] = ':';
    out[3] = '0' + m/10; out[4] = '0' + m%10;
    out[5] = 0;
}
