/* Olal OS - rede: driver NE2000 (RTL8029) + pilha de rede (Ethernet/ARP/IP/UDP/DHCP).
   A placa NE2000 e emulada pelo v86; com um relay, fala com a internet real. */
#include "olal.h"

/* ---------- registradores NE2000 (offsets da base de I/O) ---------- */
#define NE_CR   0x00
#define NE_ISR  0x07
#define NE_DCR  0x0E    /* page0: data config */
#define NE_RBCR0 0x0A
#define NE_RBCR1 0x0B
#define NE_RSAR0 0x08
#define NE_RSAR1 0x09
#define NE_RCR  0x0C
#define NE_TCR  0x0D
#define NE_TPSR 0x04
#define NE_TBCR0 0x05
#define NE_TBCR1 0x06
#define NE_PSTART 0x01
#define NE_PSTOP 0x02
#define NE_BNRY 0x03
#define NE_IMR  0x0F
#define NE_DATA 0x10
#define NE_RESET 0x1F

#define RXSTART 0x46
#define RXSTOP  0x60
#define TXSTART 0x40

static u16 nic_base = 0;
u8  net_mac[6];
int net_have_nic = 0;
volatile u32 net_tx = 0, net_rx = 0;

static u32 pci_cfg(int bus,int slot,int fn,int off){
    outl(0xCF8, 0x80000000u|(bus<<16)|(slot<<11)|(fn<<8)|(off&0xFC));
    return inl(0xCFC);
}

/* acha a NE2000 (vendor 0x10EC, device 0x8029) e devolve a base de I/O */
static u16 pci_find_ne2k(void){
    for(int s = 0; s < 32; s++){
        u32 id = pci_cfg(0,s,0,0x00);
        if(id == 0x802910ECu){
            u32 bar0 = pci_cfg(0,s,0,0x10);
            return (u16)(bar0 & 0xFFFC);     /* BAR0 = base de I/O */
        }
    }
    return 0;
}

static void ne_out(u8 reg, u8 v){ outb(nic_base + reg, v); }
static u8   ne_in(u8 reg){ return inb(nic_base + reg); }

/* le 'len' bytes da memoria da placa (remote DMA) para buf */
static void ne_read(u16 src, u8 *buf, int len){
    ne_out(NE_RBCR0, len & 0xFF); ne_out(NE_RBCR1, (len>>8)&0xFF);
    ne_out(NE_RSAR0, src & 0xFF); ne_out(NE_RSAR1, (src>>8)&0xFF);
    ne_out(NE_CR, 0x0A);                     /* remote read DMA + start */
    for(int i = 0; i < len; i++) buf[i] = inb(nic_base + NE_DATA);
}

/* escreve 'len' bytes em buf para a memoria da placa (remote DMA) */
static void ne_write(u16 dst, const u8 *buf, int len){
    ne_out(NE_ISR, 0x40);                    /* limpa RDC */
    ne_out(NE_RBCR0, len & 0xFF); ne_out(NE_RBCR1, (len>>8)&0xFF);
    ne_out(NE_RSAR0, dst & 0xFF); ne_out(NE_RSAR1, (dst>>8)&0xFF);
    ne_out(NE_CR, 0x12);                      /* remote write DMA + start */
    for(int i = 0; i < len; i++) outb(nic_base + NE_DATA, buf[i]);
    for(int g = 0; g < 100000; g++) if(ne_in(NE_ISR) & 0x40) break;
}

void ne2k_send(const u8 *frame, int len){
    if(!net_have_nic) return;
    if(len < 60) len = 60;                    /* tamanho minimo Ethernet */
    ne_write(TXSTART << 8, frame, len);
    ne_out(NE_TPSR, TXSTART);
    ne_out(NE_TBCR0, len & 0xFF); ne_out(NE_TBCR1, (len>>8)&0xFF);
    ne_out(NE_CR, 0x26);                       /* transmit + start */
    net_tx++;
}

/* recebe um frame (poll do anel de RX). Retorna o tamanho ou 0. */
int ne2k_poll(u8 *buf, int max){
    if(!net_have_nic) return 0;
    ne_out(NE_CR, 0x61);                       /* page 1 */
    u8 curr = ne_in(0x07);
    ne_out(NE_CR, 0x21);                       /* page 0 */
    u8 bnry = ne_in(NE_BNRY);
    u8 next = bnry + 1; if(next >= RXSTOP) next = RXSTART;
    if(next == curr) return 0;                 /* anel vazio */

    u8 hdr[4];
    ne_read(next << 8, hdr, 4);                /* pacote comeca na pagina bnry+1 */
    int len = (hdr[3] << 8 | hdr[2]) - 4;
    if(len < 0) len = 0;
    if(len > max) len = max;
    ne_read((next << 8) + 4, buf, len);

    u8 nx = hdr[1];                            /* proxima pagina */
    u8 nb = (nx == RXSTART) ? RXSTOP - 1 : nx - 1;
    ne_out(NE_BNRY, nb);
    net_rx++;
    return len;
}

/* ---------- pilha de rede: IP / UDP / DHCP ---------- */
u32 net_ip = 0, net_gw = 0, net_dns = 0, net_mask = 0;
int dhcp_state = 0;            /* 0=init 1=discover 2=request 3=ligado */
static u32 dhcp_xid = 0x4F4C4131u;
static u32 dhcp_offer_ip = 0, dhcp_server_ip = 0;
static u32 last_send_tick = 0;

static u16 hton16(u16 v){ return (u16)((v >> 8) | (v << 8)); }

static u16 ip_checksum(const u8 *d, int len){
    u32 s = 0;
    for(int i = 0; i + 1 < len; i += 2) s += (d[i] << 8) | d[i+1];
    if(len & 1) s += d[len-1] << 8;
    while(s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (u16)~s;
}

/* envia um datagrama UDP (broadcast) - usado pelo DHCP */
static void udp_send_bcast(u16 sport, u16 dport, const u8 *payload, int plen){
    u8 f[600];
    int n = 0;
    for(int i=0;i<6;i++) f[n++]=0xFF;                 /* eth dst broadcast */
    for(int i=0;i<6;i++) f[n++]=net_mac[i];           /* eth src */
    f[n++]=0x08; f[n++]=0x00;                          /* ethertype IPv4 */
    int ip_off = n;
    int iplen = 20 + 8 + plen;
    f[n++]=0x45; f[n++]=0x00;                          /* ver/ihl, tos */
    f[n++]=(iplen>>8)&0xFF; f[n++]=iplen&0xFF;         /* total length */
    f[n++]=0x00; f[n++]=0x00;                          /* id */
    f[n++]=0x00; f[n++]=0x00;                          /* flags/frag */
    f[n++]=64; f[n++]=17;                              /* ttl, proto UDP */
    f[n++]=0x00; f[n++]=0x00;                          /* checksum (depois) */
    f[n++]=0;f[n++]=0;f[n++]=0;f[n++]=0;               /* src 0.0.0.0 */
    f[n++]=0xFF;f[n++]=0xFF;f[n++]=0xFF;f[n++]=0xFF;   /* dst 255.255.255.255 */
    u16 c = ip_checksum(f+ip_off, 20);
    f[ip_off+10]=(c>>8)&0xFF; f[ip_off+11]=c&0xFF;
    f[n++]=(sport>>8)&0xFF; f[n++]=sport&0xFF;         /* UDP src */
    f[n++]=(dport>>8)&0xFF; f[n++]=dport&0xFF;         /* UDP dst */
    int ulen = 8 + plen;
    f[n++]=(ulen>>8)&0xFF; f[n++]=ulen&0xFF;           /* UDP length */
    f[n++]=0x00; f[n++]=0x00;                          /* UDP checksum (0=off) */
    for(int i=0;i<plen;i++) f[n++]=payload[i];
    ne2k_send(f, n);
}

/* monta e envia um pacote DHCP (type=1 discover, 3 request) */
static void dhcp_send(int type){
    u8 d[300];
    for(int i=0;i<300;i++) d[i]=0;
    d[0]=1; d[1]=1; d[2]=6; d[3]=0;                    /* op,htype,hlen,hops */
    d[4]=(dhcp_xid>>24); d[5]=(dhcp_xid>>16); d[6]=(dhcp_xid>>8); d[7]=dhcp_xid;
    for(int i=0;i<6;i++) d[28+i]=net_mac[i];           /* chaddr */
    d[236]=0x63; d[237]=0x82; d[238]=0x53; d[239]=0x63;/* magic cookie */
    int o=240;
    d[o++]=53; d[o++]=1; d[o++]=(u8)type;              /* msg type */
    if(type==3){                                       /* request: requested ip + server */
        d[o++]=50; d[o++]=4;
        d[o++]=dhcp_offer_ip>>24; d[o++]=dhcp_offer_ip>>16; d[o++]=dhcp_offer_ip>>8; d[o++]=dhcp_offer_ip;
        d[o++]=54; d[o++]=4;
        d[o++]=dhcp_server_ip>>24; d[o++]=dhcp_server_ip>>16; d[o++]=dhcp_server_ip>>8; d[o++]=dhcp_server_ip;
    }
    d[o++]=55; d[o++]=3; d[o++]=1; d[o++]=3; d[o++]=6; /* param request: mask,router,dns */
    d[o++]=255;                                        /* fim */
    udp_send_bcast(68, 67, d, o);
}

void dhcp_start(void){
    dhcp_state = 1;
    dhcp_send(1);                                      /* discover */
    last_send_tick = g_ticks;
}

static u32 rd32be(const u8 *p){ return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]; }

/* processa um pacote DHCP recebido (resposta do servidor) */
static void dhcp_recv(const u8 *d, int len){
    if(len < 240) return;
    if(rd32be(d+236) != 0x63825363) return;            /* magic cookie */
    int type = 0; u32 mask=0, gw=0, dns=0, srv=0;
    int o = 240;
    while(o < len && d[o] != 255){
        int op = d[o++]; if(o>=len) break; int l = d[o++];
        if(op==53 && l>=1) type = d[o];
        else if(op==1 && l>=4) mask = rd32be(d+o);
        else if(op==3 && l>=4) gw = rd32be(d+o);
        else if(op==6 && l>=4) dns = rd32be(d+o);
        else if(op==54 && l>=4) srv = rd32be(d+o);
        o += l;
    }
    u32 yiaddr = rd32be(d+16);
    if(type==2){                                       /* OFFER -> manda REQUEST */
        dhcp_offer_ip = yiaddr; dhcp_server_ip = srv ? srv : yiaddr;
        dhcp_state = 2; dhcp_send(3); last_send_tick = g_ticks;
    } else if(type==5){                                /* ACK -> ligado */
        net_ip = yiaddr; net_mask = mask; net_gw = gw; net_dns = dns;
        dhcp_state = 3;
    }
}

/* chamado pelo loop principal: recebe frames e cuida do DHCP */
void net_poll(void){
    if(!net_have_nic) return;
    static u8 buf[1600];
    for(int k = 0; k < 8; k++){
        int len = ne2k_poll(buf, sizeof(buf));
        if(!len) break;
        u16 eth = (buf[12]<<8) | buf[13];
        if(eth == 0x0800 && len >= 14+20+8){           /* IPv4 */
            int ihl = (buf[14] & 0x0F) * 4;
            if(buf[14+9] == 17){                        /* UDP */
                int uoff = 14 + ihl;
                u16 dport = (buf[uoff+2]<<8)|buf[uoff+3];
                if(dport == 68)                          /* resposta DHCP */
                    dhcp_recv(buf + uoff + 8, len - uoff - 8);
            }
        }
    }
    /* reenvia DHCP se ainda nao ligou (timeout ~1.5s) */
    if(dhcp_state && dhcp_state < 3 && (g_ticks - last_send_tick) > 150){
        dhcp_send(dhcp_state == 1 ? 1 : 3);
        last_send_tick = g_ticks;
    }
}

void net_init(void){
    nic_base = pci_find_ne2k();
    if(!nic_base) { net_have_nic = 0; return; }

    ne_out(NE_RESET, ne_in(NE_RESET));         /* reset */
    for(int g = 0; g < 100000; g++) if(ne_in(NE_ISR) & 0x80) break;
    ne_out(NE_ISR, 0xFF);

    ne_out(NE_CR, 0x21);                        /* stop, page0 */
    ne_out(NE_DCR, 0x48);                       /* byte mode, normal */
    ne_out(NE_RBCR0, 0); ne_out(NE_RBCR1, 0);
    ne_out(NE_RCR, 0x20);                       /* monitor (sem recv ainda) */
    ne_out(NE_TCR, 0x02);                       /* loopback p/ setup */
    ne_out(NE_TPSR, TXSTART);
    ne_out(NE_PSTART, RXSTART); ne_out(NE_BNRY, RXSTART);
    ne_out(NE_PSTOP, RXSTOP);

    u8 prom[32];
    ne_read(0, prom, 32);                       /* le o PROM (MAC) */
    for(int i = 0; i < 6; i++) net_mac[i] = prom[i*2]; /* PROM duplica os bytes */

    ne_out(NE_CR, 0x61);                        /* page1 */
    for(int i = 0; i < 6; i++) ne_out(0x01 + i, net_mac[i]); /* PAR0-5 */
    ne_out(0x07, RXSTART + 1);                  /* CURR */
    ne_out(NE_CR, 0x21);                        /* page0 */

    ne_out(NE_ISR, 0xFF);
    ne_out(NE_IMR, 0x00);                       /* sem IRQ (vamos por polling) */
    ne_out(NE_RCR, 0x04);                       /* aceita broadcast */
    ne_out(NE_TCR, 0x00);                       /* TX normal */
    ne_out(NE_CR, 0x22);                        /* start */

    net_have_nic = 1;
    dhcp_start();
}
