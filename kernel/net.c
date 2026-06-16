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
    static u8 tb[1600];
    int i = 0; for(; i < len && i < 1600; i++) tb[i] = frame[i];
    while(i < 60) tb[i++] = 0;                 /* zera o padding minimo */
    len = i;
    ne_write(TXSTART << 8, tb, len);
    ne_out(NE_TPSR, TXSTART);
    ne_out(NE_TBCR0, len & 0xFF); ne_out(NE_TBCR1, (len>>8)&0xFF);
    ne_out(NE_CR, 0x26);                       /* transmit + start */
    /* espera o fim da transmissao (PTX/TXE) para nao atropelar o proximo TX */
    for(int g = 0; g < 200000; g++) if(ne_in(NE_ISR) & 0x0A) break;
    ne_out(NE_ISR, 0x0A);                       /* limpa PTX|TXE */
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
    /* le os dados tratando a volta do anel (ring wrap) */
    u32 start = (next << 8) + 4;
    u32 ringend = RXSTOP << 8;
    if(start + len > ringend){
        int first = ringend - start;
        ne_read(start, buf, first);
        ne_read(RXSTART << 8, buf + first, len - first);
    } else {
        ne_read(start, buf, len);
    }

    u8 nx = hdr[1];                            /* proxima pagina */
    u8 nb = (nx == RXSTART) ? RXSTOP - 1 : nx - 1;
    ne_out(NE_BNRY, nb);
    ne_out(NE_ISR, 0x01);                      /* limpa PRX (pacote recebido) */
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

/* ---------- ARP (descobrir o MAC do gateway) ---------- */
static u8 gw_mac[6];
int net_gw_known = 0;
static u16 ip_id = 1;

static void arp_request(u32 ip){
    u8 f[42]; int n = 0;
    for(int i=0;i<6;i++) f[n++]=0xFF;
    for(int i=0;i<6;i++) f[n++]=net_mac[i];
    f[n++]=0x08; f[n++]=0x06; f[n++]=0x00; f[n++]=0x01;
    f[n++]=0x08; f[n++]=0x00; f[n++]=6; f[n++]=4; f[n++]=0x00; f[n++]=0x01;
    for(int i=0;i<6;i++) f[n++]=net_mac[i];
    f[n++]=net_ip>>24; f[n++]=net_ip>>16; f[n++]=net_ip>>8; f[n++]=net_ip;
    for(int i=0;i<6;i++) f[n++]=0;
    f[n++]=ip>>24; f[n++]=ip>>16; f[n++]=ip>>8; f[n++]=ip;
    ne2k_send(f, n);
}

/* envia um pacote IP unicast (roteado pelo gateway) */
static void ip_send(u32 dst, u8 proto, const u8 *payload, int plen){
    u8 f[1600]; int n = 0;
    for(int i=0;i<6;i++) f[n++]=gw_mac[i];
    for(int i=0;i<6;i++) f[n++]=net_mac[i];
    f[n++]=0x08; f[n++]=0x00;
    int ipoff = n, iplen = 20 + plen;
    f[n++]=0x45; f[n++]=0x00; f[n++]=(iplen>>8); f[n++]=iplen;
    f[n++]=(ip_id>>8); f[n++]=ip_id; ip_id++;
    f[n++]=0x40; f[n++]=0x00; f[n++]=64; f[n++]=proto; f[n++]=0; f[n++]=0;
    f[n++]=net_ip>>24; f[n++]=net_ip>>16; f[n++]=net_ip>>8; f[n++]=net_ip;
    f[n++]=dst>>24; f[n++]=dst>>16; f[n++]=dst>>8; f[n++]=dst;
    u16 c = ip_checksum(f+ipoff, 20); f[ipoff+10]=(c>>8); f[ipoff+11]=c;
    for(int i=0;i<plen;i++) f[n++]=payload[i];
    ne2k_send(f, n);
}

static void udp_send(u32 dst, u16 sport, u16 dport, const u8 *d, int dlen){
    u8 p[600]; int n = 0;
    p[n++]=(sport>>8); p[n++]=sport; p[n++]=(dport>>8); p[n++]=dport;
    int ulen = 8 + dlen; p[n++]=(ulen>>8); p[n++]=ulen; p[n++]=0; p[n++]=0;
    for(int i=0;i<dlen;i++) p[n++]=d[i];
    ip_send(dst, 17, p, n);
}

/* ---------- DNS ---------- */
u32 dns_result = 0; int dns_state = 0;    /* 0 idle,1 query,2 ok,3 falhou */
static u16 dns_id = 0x4142;
static u32 dns_tick = 0;

static void dns_query(const char *host){
    dns_tick = g_ticks;
    u8 q[300]; int n = 0;
    q[n++]=(dns_id>>8); q[n++]=dns_id; q[n++]=0x01; q[n++]=0x00;
    q[n++]=0; q[n++]=1; q[n++]=0; q[n++]=0; q[n++]=0; q[n++]=0; q[n++]=0; q[n++]=0;
    int i = 0;
    while(host[i]){
        int s = i; while(host[i] && host[i] != '.') i++;
        q[n++] = i - s; for(int j=s;j<i;j++) q[n++]=host[j];
        if(host[i]=='.') i++;
    }
    q[n++]=0; q[n++]=0; q[n++]=1; q[n++]=0; q[n++]=1;
    dns_state = 1;
    udp_send(net_dns, 5353, 53, q, n);
}

static void dns_recv(const u8 *d, int len){
    if(len < 12) return;
    int an = (d[6]<<8)|d[7];
    if(an == 0){ dns_state = 3; return; }
    int o = 12;
    while(o < len && d[o]){ if((d[o]&0xC0)==0xC0){ o+=2; goto q; } o += d[o]+1; }
    o++;
q:  o += 4;
    for(int a=0; a<an && o+12<=len; a++){
        if((d[o]&0xC0)==0xC0) o += 2;
        else { while(o<len && d[o]) o += d[o]+1; o++; }
        int type = (d[o]<<8)|d[o+1]; o += 8;
        int rdlen = (d[o]<<8)|d[o+1]; o += 2;
        if(type==1 && rdlen==4){
            dns_result = (d[o]<<24)|(d[o+1]<<16)|(d[o+2]<<8)|d[o+3];
            dns_state = 2; return;
        }
        o += rdlen;
    }
    dns_state = 3;
}

/* ---------- TCP (cliente minimo) + HTTP ---------- */
static struct { u32 dst; u16 sport, dport; u32 seq, ack; int state; } tcp;
/* state: 0 fechado, 1 syn enviado, 2 estabelecido, 3 fechando */
char http_buf[24000]; int http_len = 0; int http_phase = 0;
/* http_phase: 0 idle, 1 dns, 2 conectando, 3 recebendo, 4 pronto, 5 erro */
static char req_host[64], req_path[160];
static char http_req[260]; static int http_req_len = 0;
static u32 http_get_seq = 0, http_last_tick = 0;

static void tcp_seg(u8 flags, const u8 *data, int dlen){
    u8 s[1600]; int n = 0;
    s[n++]=(tcp.sport>>8); s[n++]=tcp.sport; s[n++]=(tcp.dport>>8); s[n++]=tcp.dport;
    s[n++]=tcp.seq>>24; s[n++]=tcp.seq>>16; s[n++]=tcp.seq>>8; s[n++]=tcp.seq;
    s[n++]=tcp.ack>>24; s[n++]=tcp.ack>>16; s[n++]=tcp.ack>>8; s[n++]=tcp.ack;
    s[n++]=0x50; s[n++]=flags; s[n++]=0x20; s[n++]=0x00;   /* offset 5, win 8192 */
    int cpos = n; s[n++]=0; s[n++]=0; s[n++]=0; s[n++]=0;
    for(int i=0;i<dlen;i++) s[n++]=data[i];
    u32 sum = 0;
    sum += (net_ip>>16)&0xFFFF; sum += net_ip&0xFFFF;
    sum += (tcp.dst>>16)&0xFFFF; sum += tcp.dst&0xFFFF;
    sum += 6; sum += n;
    for(int i=0;i+1<n;i+=2) sum += (s[i]<<8)|s[i+1];
    if(n&1) sum += s[n-1]<<8;
    while(sum>>16) sum = (sum&0xFFFF)+(sum>>16);
    u16 c = ~sum; s[cpos]=(c>>8); s[cpos+1]=c;
    ip_send(tcp.dst, 6, s, n);
}

static void http_get_send(void){
    char *r = http_req; int n = 0;
    const char *p1 = "GET ";
    for(int i=0;p1[i];i++) r[n++]=p1[i];
    for(int i=0;req_path[i];i++) r[n++]=req_path[i];
    const char *p2 = " HTTP/1.1\r\nHost: ";
    for(int i=0;p2[i];i++) r[n++]=p2[i];
    for(int i=0;req_host[i];i++) r[n++]=req_host[i];
    const char *p3 = "\r\nConnection: close\r\nUser-Agent: Olal\r\n\r\n";
    for(int i=0;p3[i];i++) r[n++]=p3[i];
    http_req_len = n;
    http_get_seq = tcp.seq;
    http_last_tick = g_ticks;
    tcp_seg(0x18, (u8*)http_req, n);   /* PSH|ACK */
    tcp.seq += n;
}

static void tcp_input(const u8 *s, int len){
    if(len < 20) return;
    u8 flags = s[13];
    u32 their_seq = (s[4]<<24)|(s[5]<<16)|(s[6]<<8)|s[7];
    int doff = (s[12]>>4)*4;
    int dlen = len - doff;

    if(tcp.state == 1 && (flags & 0x12) == 0x12){     /* SYN+ACK */
        tcp.ack = their_seq + 1;
        tcp.seq = (s[8]<<24)|(s[9]<<16)|(s[10]<<8)|s[11];   /* their ack = nosso seq */
        tcp.state = 2;
        tcp_seg(0x10, 0, 0);            /* ACK */
        http_get_send();
        http_phase = 3;
        return;
    }
    if(tcp.state >= 2){
        if(dlen > 0){
            http_last_tick = g_ticks;  /* recebendo: pausa a retransmissao */
            for(int i=0;i<dlen && http_len < (int)sizeof(http_buf)-1; i++)
                http_buf[http_len++] = s[doff+i];
            http_buf[http_len] = 0;
            tcp.ack = their_seq + dlen;
            tcp_seg(0x10, 0, 0);        /* ACK dos dados */
            /* completa pela Content-Length (sem depender do FIN) */
            int he = -1;
            for(int i=0;i+3<http_len;i++)
                if(http_buf[i]=='\r'&&http_buf[i+1]=='\n'&&http_buf[i+2]=='\r'&&http_buf[i+3]=='\n'){ he=i+4; break; }
            if(he >= 0){
                int cl = -1;
                for(int i=0;i+16<he;i++)
                    if((http_buf[i]=='C'||http_buf[i]=='c')&&(http_buf[i+1]=='o')&&http_buf[i+8]=='-'){
                        int j=i+14; while(http_buf[j]==' '||http_buf[j]==':') j++;
                        if(http_buf[j]>='0'&&http_buf[j]<='9'){ cl=0; while(http_buf[j]>='0'&&http_buf[j]<='9'){ cl=cl*10+(http_buf[j]-'0'); j++; } break; }
                    }
                if(cl >= 0 && http_len >= he + cl) http_phase = 4;
            }
        }
        if(flags & 0x01){              /* FIN */
            tcp.ack = their_seq + dlen + 1;
            tcp_seg(0x10, 0, 0);
            tcp.state = 0;
            http_phase = 4;            /* pronto */
        }
    }
}

static void tcp_connect(u32 dst, u16 port){
    tcp.dst = dst; tcp.dport = port; tcp.sport = 40000 + (g_ticks & 0x3FFF);
    tcp.seq = 0x1000 + (g_ticks * 7); tcp.ack = 0; tcp.state = 1;
    http_len = 0; http_buf[0] = 0;
    tcp_seg(0x02, 0, 0);               /* SYN */
}

/* inicia uma navegacao: separa host/caminho e comeca pelo DNS */
void browse(const char *url){
    int i = 0;
    /* pula o esquema "http://" apenas se houver "://" */
    for(int j = 0; url[j] && url[j] != '/'; j++){
        if(url[j]==':' && url[j+1]=='/' && url[j+2]=='/'){ i = j+3; break; }
    }
    int h = 0;
    while(url[i] && url[i] != '/' && h < 62) req_host[h++] = url[i++];
    req_host[h] = 0;
    int p = 0;
    if(url[i] != '/') req_path[p++] = '/';
    while(url[i] && p < 158) req_path[p++] = url[i++];
    req_path[p] = 0;
    http_len = 0; http_buf[0] = 0; dns_state = 0; http_phase = 1;
    dns_query(req_host);
}

/* ---------- DHCP retransmit / loop principal ---------- */
void net_poll(void){
    if(!net_have_nic) return;
    /* manutencao da placa: recupera de overflow do anel de RX e limpa ISR */
    u8 isr = ne_in(NE_ISR);
    if(isr & 0x10){                            /* OVW: anel de RX transbordou */
        u8 was = ne_in(NE_CR);
        ne_out(NE_CR, 0x21);                   /* stop */
        ne_out(NE_RBCR0, 0); ne_out(NE_RBCR1, 0);
        ne_out(NE_TCR, 0x02);                  /* loopback durante o reset */
        ne_out(NE_CR, 0x22);                   /* start */
        ne_out(NE_BNRY, RXSTART);
        ne_out(NE_CR, 0x61); ne_out(0x07, RXSTART + 1); ne_out(NE_CR, 0x21);
        ne_out(NE_ISR, 0x10);                  /* limpa OVW */
        ne_out(NE_TCR, 0x00);
        (void)was;
    }
    static u8 buf[1600];
    for(int k = 0; k < 12; k++){
        int len = ne2k_poll(buf, sizeof(buf));
        if(!len) break;
        u16 eth = (buf[12]<<8) | buf[13];
        if(eth == 0x0806 && len >= 42){              /* ARP */
            u16 oper = (buf[20]<<8)|buf[21];
            if(oper == 2){                            /* reply: aprende MAC */
                u32 sip = (buf[28]<<24)|(buf[29]<<16)|(buf[30]<<8)|buf[31];
                if(sip == net_gw){ for(int i=0;i<6;i++) gw_mac[i]=buf[22+i]; net_gw_known=1; }
            } else if(oper == 1){                     /* request p/ nosso IP: responde */
                u32 tip = (buf[38]<<24)|(buf[39]<<16)|(buf[40]<<8)|buf[41];
                if(tip == net_ip && net_ip){
                    u8 r[42]; int n=0;
                    for(int i=0;i<6;i++) r[n++]=buf[6+i];
                    for(int i=0;i<6;i++) r[n++]=net_mac[i];
                    r[n++]=0x08;r[n++]=0x06;r[n++]=0;r[n++]=1;r[n++]=0x08;r[n++]=0;
                    r[n++]=6;r[n++]=4;r[n++]=0;r[n++]=2;
                    for(int i=0;i<6;i++) r[n++]=net_mac[i];
                    r[n++]=net_ip>>24;r[n++]=net_ip>>16;r[n++]=net_ip>>8;r[n++]=net_ip;
                    for(int i=0;i<6;i++) r[n++]=buf[22+i];
                    for(int i=0;i<4;i++) r[n++]=buf[28+i];
                    ne2k_send(r, n);
                }
            }
        }
        else if(eth == 0x0800 && len >= 34){          /* IPv4 */
            int ihl = (buf[14] & 0x0F) * 4;
            u8 proto = buf[14+9];
            int off = 14 + ihl;
            int iptot = (buf[16]<<8) | buf[17];        /* tamanho real (sem padding) */
            if(14 + iptot < len) len = 14 + iptot;
            if(proto == 17){                           /* UDP */
                u16 dport = (buf[off+2]<<8)|buf[off+3];
                u16 sport = (buf[off+0]<<8)|buf[off+1];
                if(dport == 68) dhcp_recv(buf+off+8, len-off-8);
                else if(sport == 53) dns_recv(buf+off+8, len-off-8);
            } else if(proto == 6){                     /* TCP */
                u16 dport = (buf[off+2]<<8)|buf[off+3];
                if(dport == tcp.sport) tcp_input(buf+off, len-off);
            }
        }
    }

    if(dhcp_state && dhcp_state < 3 && (g_ticks - last_send_tick) > 150){
        dhcp_send(dhcp_state == 1 ? 1 : 3);
        last_send_tick = g_ticks;
    }
    /* depois do DHCP, descobre o MAC do gateway */
    if(dhcp_state == 3 && !net_gw_known && (g_ticks - last_send_tick) > 30){
        arp_request(net_gw); last_send_tick = g_ticks;
    }
    /* retransmite a query DNS se a resposta nao chegou */
    if(http_phase == 1 && dns_state == 1 && net_gw_known && (g_ticks - dns_tick) > 80)
        dns_query(req_host);
    /* maquina de navegacao: DNS pronto -> conecta */
    if(http_phase == 1 && dns_state == 2){
        if(net_gw_known){ tcp_connect(dns_result, 80); http_phase = 2; }
    }
    if(http_phase == 1 && dns_state == 3) http_phase = 5;   /* dns falhou */
    /* retransmite o GET enquanto espera a resposta (o gateway injeta a
       pagina em cima de uma retransmissao, de forma sincrona) */
    if(http_phase == 3 && http_req_len && (g_ticks - http_last_tick) > 100){
        u32 s = tcp.seq; tcp.seq = http_get_seq;
        tcp_seg(0x18, (u8*)http_req, http_req_len);
        tcp.seq = s;
        http_last_tick = g_ticks;
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
