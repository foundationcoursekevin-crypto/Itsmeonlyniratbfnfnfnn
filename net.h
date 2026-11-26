/* PixelBeeOS 3.1 - network stack (NIC independent): Ethernet, ARP, IPv4, ICMP echo, UDP, DHCP client, DNS resolver,
 * a single-connection TCP client and an HTTP/1.1 GET client (redirects + chunked bodies).
 *
 * Design: fully polled, no interrupts. Every wait is bounded by a timeout, so a dead network can never hang the OS
 * (it can only make one command wait a few seconds). One TCP connection at a time. No IP fragmentation, no TLS:
 * https:// sites are NOT reachable. Needs from the includer: u8/u16/u32, memcpy/memset, now_ms().
 */
#ifndef PIXELBEE_NET_H
#define PIXELBEE_NET_H

typedef struct { int (*send)(const u8 *f, int n); int (*recv)(u8 *f, int max); u8 mac[6]; int up; char name[28]; } NetNic;
static NetNic net_nic;
static u8 net_ip[4], net_mask[4], net_gw[4], net_dns[4]; static int net_cfg; static u32 net_lease;
static u8 net_rxb[2048], net_txb[2048];
static u32 net_stat_rx, net_stat_tx, net_stat_bad;

static int net_eq(const u8 *a, const u8 *b, int n) { for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0; return 1; }
static void put16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void put32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static u16 get16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u32 get32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u32 net_sum(const u8 *d, int n, u32 s) { for (; n > 1; n -= 2, d += 2) s += (u32)(d[0] << 8 | d[1]); if (n) s += (u32)d[0] << 8; return s; }
static u16 net_fold(u32 s) { while (s >> 16) s = (s & 0xFFFF) + (s >> 16); return (u16)~s; }
static u16 net_csum(const u8 *d, int n) { return net_fold(net_sum(d, n, 0)); }

/* ------------------------------------------------------------ ethernet + ARP */
static struct { u8 ip[4], mac[6]; int ok; } arp_t[8]; static int arp_next;
static int arp_lookup(const u8 *ip, u8 *mac) { for (int i = 0; i < 8; i++) if (arp_t[i].ok && net_eq(arp_t[i].ip, ip, 4)) { memcpy(mac, arp_t[i].mac, 6); return 1; } return 0; }
static void arp_learn(const u8 *ip, const u8 *mac) {
    int s = -1; for (int i = 0; i < 8; i++) if (arp_t[i].ok && net_eq(arp_t[i].ip, ip, 4)) s = i;
    if (s < 0) { s = arp_next; arp_next = (arp_next + 1) & 7; }
    memcpy(arp_t[s].ip, ip, 4); memcpy(arp_t[s].mac, mac, 6); arp_t[s].ok = 1;
}
static int net_send_frame(const u8 *f, int n) {
    static u8 pad[64]; if (!net_nic.send || !net_nic.up) return -1;
    if (n < 60) { memcpy(pad, f, (unsigned)n); memset(pad + n, 0, (unsigned)(60 - n)); f = pad; n = 60; }
    net_stat_tx++; return net_nic.send(f, n);
}
static void arp_send(int op, const u8 *tmac, const u8 *tip) {
    u8 f[60]; static const u8 bc[6] = {255, 255, 255, 255, 255, 255}; memset(f, 0, sizeof f);
    memcpy(f, op == 1 ? bc : tmac, 6); memcpy(f + 6, net_nic.mac, 6); put16(f + 12, 0x0806);
    put16(f + 14, 1); put16(f + 16, 0x0800); f[18] = 6; f[19] = 4; put16(f + 20, (u16)op);
    memcpy(f + 22, net_nic.mac, 6); memcpy(f + 28, net_ip, 4); if (op == 2) memcpy(f + 32, tmac, 6); memcpy(f + 38, tip, 4);
    net_send_frame(f, 60);
}
static int net_poll(void);
static int arp_resolve(const u8 *ip, u8 *mac) {
    if (arp_lookup(ip, mac)) return 1;
    for (int t = 0; t < 3; t++) { arp_send(1, 0, ip); u32 s = now_ms(); while (now_ms() - s < 500) { net_poll(); if (arp_lookup(ip, mac)) return 1; } }
    return 0;
}

/* ------------------------------------------------------------ IPv4 */
static u16 ip_id;
static int net_ip_send(const u8 *dst, u8 proto, const u8 *pl, int len) {
    static const u8 bc4[4] = {255, 255, 255, 255}; u8 mac[6]; if (len > 1480) return -1;
    if (net_eq(dst, bc4, 4)) memset(mac, 255, 6);
    else {
        const u8 *hop = dst; int local = 1; for (int i = 0; i < 4; i++) if ((dst[i] & net_mask[i]) != (net_ip[i] & net_mask[i])) local = 0;
        if (!local) { hop = net_gw; if (!(net_gw[0] | net_gw[1] | net_gw[2] | net_gw[3])) return -2; }
        if (!arp_resolve(hop, mac)) return -3;
    }
    u8 *f = net_txb; memcpy(f, mac, 6); memcpy(f + 6, net_nic.mac, 6); put16(f + 12, 0x0800);
    u8 *ip = f + 14; ip[0] = 0x45; ip[1] = 0; put16(ip + 2, (u16)(20 + len)); put16(ip + 4, ++ip_id); put16(ip + 6, 0x4000); ip[8] = 64; ip[9] = proto;
    put16(ip + 10, 0); memcpy(ip + 12, net_ip, 4); memcpy(ip + 16, dst, 4); put16(ip + 10, net_csum(ip, 20));
    memcpy(ip + 20, pl, (unsigned)len); return net_send_frame(f, 34 + len);
}
static u16 net_l4_csum(const u8 *src, const u8 *dst, u8 proto, const u8 *seg, int n) {
    u8 ph[12]; memcpy(ph, src, 4); memcpy(ph + 4, dst, 4); ph[8] = 0; ph[9] = proto; put16(ph + 10, (u16)n);
    return net_fold(net_sum(seg, n, net_sum(ph, 12, 0)));
}
static int net_udp_send(const u8 *dst, u16 sp, u16 dp, const u8 *d, int n) {
    static u8 u[1500]; if (n > 1400) return -1; put16(u, sp); put16(u + 2, dp); put16(u + 4, (u16)(8 + n)); put16(u + 6, 0); memcpy(u + 8, d, (unsigned)n);
    u16 c = net_l4_csum(net_ip, dst, 17, u, 8 + n); put16(u + 6, c ? c : 0xFFFF); return net_ip_send(dst, 17, u, 8 + n);
}

/* ------------------------------------------------------------ ICMP (ping) */
static u16 ping_id = 0xB33, ping_seq; static int ping_got; static u32 ping_t, ping_rtt;
static int net_ping(const u8 *ip, u32 *rtt) {
    u8 p[40]; memset(p, 0, sizeof p); p[0] = 8; put16(p + 4, ping_id); put16(p + 6, ++ping_seq); for (int i = 8; i < 40; i++) p[i] = (u8)(i + 'a');
    put16(p + 2, net_csum(p, 40)); ping_got = 0; ping_t = now_ms();
    int r = net_ip_send(ip, 1, p, 40); if (r) return r == -3 ? -3 : -1;
    while (now_ms() - ping_t < 2000) { net_poll(); if (ping_got) { *rtt = ping_rtt; return 0; } }
    return -4;
}

/* ------------------------------------------------------------ DHCP client */
static u32 dhcp_xid; static int dhcp_stage, dhcp_got; static u8 d_off[4], d_srv[4], d_mask[4], d_gw[4], d_dns[4]; static u32 d_lease;
static void dhcp_send(int type) {
    static u8 m[320]; memset(m, 0, sizeof m); m[0] = 1; m[1] = 1; m[2] = 6; put32(m + 4, dhcp_xid); put16(m + 10, 0x8000); memcpy(m + 28, net_nic.mac, 6); put32(m + 236, 0x63825363u);
    int i = 240; m[i++] = 53; m[i++] = 1; m[i++] = (u8)type;
    m[i++] = 61; m[i++] = 7; m[i++] = 1; memcpy(m + i, net_nic.mac, 6); i += 6;
    if (type == 3) { m[i++] = 50; m[i++] = 4; memcpy(m + i, d_off, 4); i += 4; m[i++] = 54; m[i++] = 4; memcpy(m + i, d_srv, 4); i += 4; }
    m[i++] = 55; m[i++] = 4; m[i++] = 1; m[i++] = 3; m[i++] = 6; m[i++] = 51; m[i++] = 255;
    static const u8 bc4[4] = {255, 255, 255, 255}; net_udp_send(bc4, 68, 67, m, i < 300 ? 300 : i);
}
static void dhcp_input(const u8 *d, int n) {
    if (n < 241 || d[0] != 2 || get32(d + 4) != dhcp_xid || !net_eq(d + 28, net_nic.mac, 6) || get32(d + 236) != 0x63825363u) return;
    int type = 0; u8 mask[4] = {0}, gw[4] = {0}, dns[4] = {0}, srv[4] = {0}; u32 lease = 0; int i = 240;
    while (i < n) { u8 c = d[i++]; if (c == 255) break; if (c == 0) continue; if (i >= n) break; int l = d[i++]; if (i + l > n) break;
        if (c == 53 && l >= 1) type = d[i]; else if (c == 1 && l >= 4) memcpy(mask, d + i, 4); else if (c == 3 && l >= 4) memcpy(gw, d + i, 4);
        else if (c == 6 && l >= 4) memcpy(dns, d + i, 4); else if (c == 51 && l >= 4) lease = get32(d + i); else if (c == 54 && l >= 4) memcpy(srv, d + i, 4);
        i += l; }
    if (type == 2 && dhcp_stage == 1) { memcpy(d_off, d + 16, 4); memcpy(d_srv, srv, 4); dhcp_got = 2; }
    else if (type == 5 && dhcp_stage == 2) { memcpy(d_off, d + 16, 4); memcpy(d_mask, mask, 4); memcpy(d_gw, gw, 4); memcpy(d_dns, dns, 4); d_lease = lease; dhcp_got = 5; }
    else if (type == 6) dhcp_got = 6;
}
static int net_dhcp_n(int tries) {
    if (!net_nic.up) return -1; net_cfg = 0; memset(net_ip, 0, 4); memset(net_mask, 0, 4); memset(net_gw, 0, 4);
    for (int a = 0; a < tries; a++) {
        dhcp_xid = now_ms() * 2654435761u ^ ((u32)net_nic.mac[5] << 8 | (u32)a); dhcp_stage = 1; dhcp_got = 0; dhcp_send(1);
        u32 s = now_ms(); while (now_ms() - s < 2000 && dhcp_got != 2) net_poll(); if (dhcp_got != 2) continue;
        dhcp_stage = 2; dhcp_got = 0; dhcp_send(3); s = now_ms(); while (now_ms() - s < 2000 && dhcp_got != 5 && dhcp_got != 6) net_poll();
        if (dhcp_got == 5) { memcpy(net_ip, d_off, 4); memcpy(net_mask, d_mask, 4); memcpy(net_gw, d_gw, 4); memcpy(net_dns, d_dns, 4); net_lease = d_lease; net_cfg = 1; dhcp_stage = 0; return 0; }
    }
    dhcp_stage = 0; return -4;
}
static int net_dhcp(void) { return net_dhcp_n(3); }

/* ------------------------------------------------------------ DNS */
static u16 dns_id, dns_port; static int dns_got; static u8 dns_ans[4];
static void dns_input(const u8 *d, int n) {
    if (n < 12 || get16(d) != dns_id || !(d[2] & 0x80)) return; if (d[3] & 0x0F) { dns_got = -1; return; }
    int qd = get16(d + 4), an = get16(d + 6), p = 12;
    for (int q = 0; q < qd; q++) { while (p < n && d[p]) { if ((d[p] & 0xC0) == 0xC0) { p++; break; } p += d[p] + 1; } p += 1 + 4; }
    for (int a = 0; a < an && p < n; a++) {
        while (p < n && d[p]) { if ((d[p] & 0xC0) == 0xC0) { p++; break; } p += d[p] + 1; } p++;
        if (p + 10 > n) return; int type = get16(d + p), rl = get16(d + p + 8); p += 10; if (p + rl > n) return;
        if (type == 1 && rl == 4) { memcpy(dns_ans, d + p, 4); dns_got = 1; return; }
        p += rl;
    }
    dns_got = -1;
}
static int net_dns_query(const char *name, u8 *out) {
    static u8 q[300]; int n = 12, l = 0; while (name[l]) l++; if (l < 1 || l > 253) return -1;
    memset(q, 0, 12); q[2] = 0x01; q[5] = 1; int lab = n++, ln = 0;
    for (int i = 0; i <= l; i++) { if (i == l || name[i] == '.') { if (ln < 1 || ln > 63) return -1; q[lab] = (u8)ln; lab = n++; ln = 0; } else { q[n++] = (u8)name[i]; ln++; } }
    q[lab] = 0; n = lab + 1; put16(q + n, 1); put16(q + n + 2, 1); n += 4;
    if (!(net_dns[0] | net_dns[1] | net_dns[2] | net_dns[3])) return -2;
    for (int t = 0; t < 3; t++) {
        dns_id = (u16)(now_ms() * 40503u + (u32)t); dns_port = (u16)(40000 + (dns_id & 0xFFF)); dns_got = 0; put16(q, dns_id);
        if (net_udp_send(net_dns, dns_port, 53, q, n) < 0) return -3;
        u32 s = now_ms(); while (now_ms() - s < 1500 && !dns_got) net_poll();
        if (dns_got == 1) { memcpy(out, dns_ans, 4); dns_port = 0; return 0; } if (dns_got == -1) { dns_port = 0; return -5; }
    }
    dns_port = 0; return -4;
}
static int net_parse_ip(const char *s, u8 *out) {
    int part = 0, val = 0, digits = 0; for (;; s++) {
        if (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); digits++; if (val > 255 || digits > 3) return 0; }
        else if (*s == '.' || !*s) { if (!digits || part > 3) return 0; out[part++] = (u8)val; val = digits = 0; if (!*s) break; } else return 0;
    }
    return part == 4;
}
static int net_resolve(const char *host, u8 *out) { if (net_parse_ip(host, out)) return 0; return net_dns_query(host, out); }

/* ------------------------------------------------------------ TCP client (one connection) */
static struct { int state, fin, rst, full; u8 rip[4]; u16 lport, rport; u32 snd_una, snd_nxt, rcv_nxt; u8 *rx; int rxlen, rxmax; } tcp;
static int tcp_tx(u32 seq, u8 fl, const u8 *d, int n) {
    static u8 s[1500]; int hl = (fl & 2) ? 24 : 20; if (n > 1400) return -1;
    put16(s, tcp.lport); put16(s + 2, tcp.rport); put32(s + 4, seq); put32(s + 8, (fl & 0x10) ? tcp.rcv_nxt : 0); s[12] = (u8)(hl << 2); s[13] = fl;
    int win = tcp.rxmax - tcp.rxlen; if (win > 65535) win = 65535; if (win < 0) win = 0; put16(s + 14, (u16)win); put16(s + 16, 0); put16(s + 18, 0);
    if (fl & 2) { s[20] = 2; s[21] = 4; put16(s + 22, 1400); }
    if (n) memcpy(s + hl, d, (unsigned)n); put16(s + 16, net_l4_csum(net_ip, tcp.rip, 6, s, hl + n)); return net_ip_send(tcp.rip, 6, s, hl + n);
}
static void tcp_input(const u8 *src, const u8 *seg, int n) {
    if (n < 20 || !tcp.state || !net_eq(src, tcp.rip, 4) || get16(seg) != tcp.rport || get16(seg + 2) != tcp.lport) return;
    if (net_l4_csum(src, net_ip, 6, seg, n) != 0) { net_stat_bad++; return; }
    u32 seq = get32(seg + 4), ack = get32(seg + 8); int off = (seg[12] >> 4) * 4; u8 fl = seg[13]; if (off < 20 || off > n) return;
    const u8 *pl = seg + off; int plen = n - off;
    if (fl & 4) { tcp.state = 0; tcp.rst = 1; return; }
    if (tcp.state == 1) { if ((fl & 0x12) == 0x12 && ack == tcp.snd_nxt) { tcp.rcv_nxt = seq + 1; tcp.snd_una = ack; tcp.state = 2; tcp_tx(tcp.snd_nxt, 0x10, 0, 0); } return; }
    if ((fl & 0x10) && (int)(ack - tcp.snd_una) > 0 && (int)(ack - tcp.snd_nxt) <= 0) tcp.snd_una = ack;
    if (plen > 0 || (fl & 1)) {
        if (seq == tcp.rcv_nxt) {
            if (plen > 0) { if (tcp.rxlen + plen > tcp.rxmax) { tcp.full = 1; tcp_tx(tcp.snd_nxt, 0x10, 0, 0); return; } memcpy(tcp.rx + tcp.rxlen, pl, (unsigned)plen); tcp.rxlen += plen; tcp.rcv_nxt += (u32)plen; }
            if (fl & 1) { tcp.rcv_nxt++; tcp.fin = 1; }
        }
        tcp_tx(tcp.snd_nxt, 0x10, 0, 0);                    /* ACK (a duplicate ACK when the segment was out of order) */
    }
}
/* connect, send one request segment, read until the server closes / done() says complete / the link goes idle. */
static int tcp_get(const u8 *ip, u16 port, const u8 *req, int reqlen, u8 *buf, int bufmax, int *outlen, int (*done)(const u8 *, int)) {
    u32 iss = now_ms() * 7919u + 12345u; memset(&tcp, 0, sizeof tcp); memcpy(tcp.rip, ip, 4); tcp.rport = port; tcp.lport = (u16)(49152 + (now_ms() & 0x3FFF));
    tcp.snd_una = iss; tcp.snd_nxt = iss + 1; tcp.rx = buf; tcp.rxmax = bufmax; tcp.state = 1; *outlen = 0; if (reqlen > 1400) return -1;
    for (int t = 0; t < 4 && tcp.state == 1; t++) { if (tcp_tx(iss, 0x02, 0, 0) < 0) { tcp.state = 0; return -3; } u32 s = now_ms(); while (now_ms() - s < 1000 && tcp.state == 1) net_poll(); }
    if (tcp.state != 2) { int rst = tcp.rst; tcp.state = 0; return rst ? -2 : -4; }
    u32 rseq = tcp.snd_nxt; tcp.snd_nxt += (u32)reqlen;
    for (int t = 0; t < 4 && tcp.snd_una != tcp.snd_nxt && !tcp.rxlen && !tcp.fin && tcp.state; t++) { tcp_tx(rseq, 0x18, req, reqlen); u32 s = now_ms(); while (now_ms() - s < 1000 && tcp.snd_una != tcp.snd_nxt && !tcp.rxlen && !tcp.fin && tcp.state) net_poll(); }
    if (tcp.snd_una != tcp.snd_nxt && !tcp.rxlen && !tcp.fin) { tcp.state = 0; return -4; }
    u32 start = now_ms(), last = start; int lastlen = -1;
    while (!tcp.fin && tcp.state && now_ms() - last < 6000 && now_ms() - start < 25000) {
        net_poll(); if (tcp.rxlen != lastlen) { lastlen = tcp.rxlen; last = now_ms(); if (done && done(tcp.rx, tcp.rxlen)) break; }
    }
    if (tcp.state == 2) tcp_tx(tcp.snd_nxt, tcp.fin ? 0x11 : 0x14, 0, 0);       /* FIN after the server's FIN; RST if we gave up */
    *outlen = tcp.rxlen; int rst = tcp.rst; tcp.state = 0; return (rst && !*outlen) ? -2 : 0;
}

/* Streaming TCP for TLS: open, write (stop-and-wait, retransmits), wait for buffered bytes, consume, close. */
static int tcp_open(const u8 *ip, u16 port, u8 *buf, int bufmax) {
    u32 iss = now_ms() * 7919u + 12345u; memset(&tcp, 0, sizeof tcp); memcpy(tcp.rip, ip, 4); tcp.rport = port; tcp.lport = (u16)(49152 + (now_ms() & 0x3FFF));
    tcp.snd_una = iss; tcp.snd_nxt = iss + 1; tcp.rx = buf; tcp.rxmax = bufmax; tcp.state = 1;
    for (int t = 0; t < 4 && tcp.state == 1; t++) { if (tcp_tx(iss, 0x02, 0, 0) < 0) { tcp.state = 0; return -3; } u32 s = now_ms(); while (now_ms() - s < 1000 && tcp.state == 1) net_poll(); }
    if (tcp.state != 2) { int rst = tcp.rst; tcp.state = 0; return rst ? -2 : -4; } return 0;
}
static int tcp_write(const u8 *d, int n) {
    while (n > 0) {
        int c = n > 1400 ? 1400 : n; u32 seq = tcp.snd_nxt; tcp.snd_nxt += (u32)c; int ok = 0;
        for (int t = 0; t < 4 && !ok && tcp.state == 2; t++) { tcp_tx(seq, 0x18, d, c); u32 s = now_ms(); while (now_ms() - s < 1000 && tcp.snd_una != tcp.snd_nxt && tcp.state == 2) net_poll(); ok = tcp.snd_una == tcp.snd_nxt; }
        if (!ok) return -4; d += c; n -= c;
    }
    return 0;
}
static int tcp_need(int n, u32 ms) {
    u32 s = now_ms(); for (;;) { if (tcp.rxlen >= n) return 1; if (tcp.state != 2 || tcp.fin || now_ms() - s >= ms) return 0; net_poll(); }
}
static const u8 *tcp_data(void) { return tcp.rx; }
static void tcp_eat(int n) { int was = tcp.rxmax - tcp.rxlen; if (n > tcp.rxlen) n = tcp.rxlen; memmove(tcp.rx, tcp.rx + n, (unsigned)(tcp.rxlen - n)); tcp.rxlen -= n; if (was < 4096 && tcp.state == 2) tcp_tx(tcp.snd_nxt, 0x10, 0, 0); }   /* window update */
static void tcp_close(void) { if (tcp.state == 2) tcp_tx(tcp.snd_nxt, 0x11, 0, 0); tcp.state = 0; }

/* ------------------------------------------------------------ frame receive */
static void net_rx_frame(const u8 *f, int n) {
    if (n < 14) return; u16 et = get16(f + 12);
    if (et == 0x0806) {
        if (n < 42 || get16(f + 14) != 1 || get16(f + 16) != 0x0800) return; int op = get16(f + 20);
        if (op == 2 || (op == 1 && net_cfg && net_eq(f + 38, net_ip, 4))) arp_learn(f + 28, f + 22);
        if (op == 1 && net_cfg && net_eq(f + 38, net_ip, 4)) arp_send(2, f + 22, f + 28);
        return;
    }
    if (et != 0x0800 || n < 34) return; const u8 *ip = f + 14; if ((ip[0] >> 4) != 4) return; int ihl = (ip[0] & 15) * 4, tl = get16(ip + 2);
    if (ihl < 20 || tl < ihl || tl > n - 14) return; if (net_csum(ip, ihl) != 0) { net_stat_bad++; return; } if (get16(ip + 6) & 0x3FFF) return;
    static const u8 bc4[4] = {255, 255, 255, 255}; if (net_cfg && !net_eq(ip + 16, net_ip, 4) && !net_eq(ip + 16, bc4, 4)) return;
    const u8 *src = ip + 12, *pl = ip + ihl; int plen = tl - ihl; u8 proto = ip[9];
    if (proto == 1 && plen >= 8) {
        if (pl[0] == 8 && net_cfg) { static u8 r[1500]; if (plen > 1480) return; memcpy(r, pl, (unsigned)plen); r[0] = 0; put16(r + 2, 0); put16(r + 2, net_csum(r, plen)); net_ip_send(src, 1, r, plen); }
        else if (pl[0] == 0 && get16(pl + 4) == ping_id && get16(pl + 6) == ping_seq) { ping_rtt = now_ms() - ping_t; ping_got = 1; }
    } else if (proto == 17 && plen >= 8) {
        int ul = get16(pl + 4); if (ul < 8 || ul > plen) return; u16 dp = get16(pl + 2);
        if (get16(pl + 6) && net_l4_csum(src, ip + 16, 17, pl, ul) != 0) { net_stat_bad++; return; }
        if (dp == 68) dhcp_input(pl + 8, ul - 8); else if (dns_port && dp == dns_port) dns_input(pl + 8, ul - 8);
    } else if (proto == 6) tcp_input(src, pl, plen);
}
static int net_poll(void) {
    int cnt = 0; if (!net_nic.recv || !net_nic.up) return 0;
    for (int i = 0; i < 16; i++) { int n = net_nic.recv(net_rxb, (int)sizeof net_rxb); if (n <= 0) break; net_stat_rx++;
#ifdef NET_HAVE_TLS
        rng_event((u32)n ^ now_ms());
#endif
        net_rx_frame(net_rxb, n); cnt++; }
    return cnt;
}

/* ------------------------------------------------------------ HTTP/1.1 GET */
static u8 net_hbuf[60000]; static const u8 *net_body; static int net_blen, net_hstatus, net_htrunc, net_hs_tls; static char net_hloc[200];
#ifdef NET_HAVE_TLS
static int net_tls_cut;
static int net_https_fetch(const u8 *ip, u16 port, const char *host, const u8 *req, int reqlen, u8 *buf, int bufmax, int *outlen, int (*done)(const u8 *, int));
#endif
static char hlc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int h_find(const u8 *b, int n, const char *pat) { int pl = 0; while (pat[pl]) pl++; for (int i = 0; i + pl <= n; i++) { int k = 0; while (k < pl && hlc((char)b[i + k]) == hlc(pat[k])) k++; if (k == pl) return i; } return -1; }
static int h_hdr_end(const u8 *b, int n) { for (int i = 0; i + 3 < n; i++) if (b[i] == '\r' && b[i + 1] == '\n' && b[i + 2] == '\r' && b[i + 3] == '\n') return i + 4; return -1; }
static int h_done(const u8 *b, int n) {
    int he = h_hdr_end(b, n); if (he < 0) return 0;
    int c = h_find(b, he, "\r\ncontent-length:"); if (c >= 0) { int p = c + 17, v = 0; while (p < he && b[p] == ' ') p++; while (p < he && b[p] >= '0' && b[p] <= '9') v = v * 10 + (b[p++] - '0'); return n - he >= v; }
    if (h_find(b, he, "\r\ntransfer-encoding: chunked") >= 0) return n >= 5 && b[n - 5] == '0' && b[n - 4] == '\r' && b[n - 3] == '\n' && b[n - 2] == '\r' && b[n - 1] == '\n';
    return 0;
}
static int net_url(const char *u, char *host, int *port, char *path, int *tls) {
    int i = 0; *tls = 0; *port = 80;
    if (hlc(u[0]) == 'h' && hlc(u[1]) == 't' && hlc(u[2]) == 't' && hlc(u[3]) == 'p' && u[4] == ':' && u[5] == '/' && u[6] == '/') i = 7;
    else if (hlc(u[0]) == 'h' && hlc(u[1]) == 't' && hlc(u[2]) == 't' && hlc(u[3]) == 'p' && hlc(u[4]) == 's' && u[5] == ':' && u[6] == '/' && u[7] == '/') { i = 8; *tls = 1; *port = 443; }
    int h = 0; while (u[i] && u[i] != '/' && u[i] != ':' && h < 120) host[h++] = u[i++]; host[h] = 0;
    if (u[i] == ':') { i++; int p = 0; while (u[i] >= '0' && u[i] <= '9') p = p * 10 + (u[i++] - '0'); if (p < 1 || p > 65535) return -1; *port = p; }
    int p = 0; if (!u[i]) path[p++] = '/'; while (u[i] && p < 190) path[p++] = u[i++]; path[p] = 0; return h ? 0 : -1;
}
static int h_dechunk(u8 *b, int n) {
    int rp = 0, wp = 0; while (rp < n) {
        int sz = 0, any = 0; while (rp < n && ((b[rp] >= '0' && b[rp] <= '9') || (hlc((char)b[rp]) >= 'a' && hlc((char)b[rp]) <= 'f'))) { char c = hlc((char)b[rp++]); sz = sz * 16 + (c <= '9' ? c - '0' : c - 'a' + 10); any = 1; if (sz > 1000000) return wp; }
        if (!any) break; while (rp < n && b[rp] != '\n') rp++; rp++; if (sz == 0) break; if (rp + sz > n) sz = n - rp;
        memmove(b + wp, b + rp, (unsigned)sz); wp += sz; rp += sz; if (rp < n && b[rp] == '\r') rp++; if (rp < n && b[rp] == '\n') rp++;
    }
    return wp;
}
/* Returns 0 on success with net_body/net_blen/net_hstatus set; negative on failure
 * (-1 bad url, -2 https, -3 no network/DNS, -4 timeout, -5 refused, -6 bad reply). */
static int net_http_get(const char *url0) {
    static char url[200], host[128], path[200], req[512]; int port, redirects = 0, tls = 0; u8 ip[4];
    int u = 0; for (; url0[u] && u < 199; u++) url[u] = url0[u]; url[u] = 0; net_htrunc = 0;
    for (;;) {
        int r = net_url(url, host, &port, path, &tls); if (r) return r; net_hs_tls = tls;
        if (!net_cfg) return -3; if (net_resolve(host, ip)) return -3;
        int q = 0; const char *parts[] = {"GET ", path, " HTTP/1.1\r\nHost: ", host, "\r\nUser-Agent: PixelBeeOS/3.1\r\nAccept: text/html, text/plain, */*\r\nConnection: close\r\n\r\n"};
        for (int a = 0; a < 5; a++) for (const char *s = parts[a]; *s && q < 500; s++) req[q++] = *s; req[q] = 0;
        int n;
        if (tls) {
#ifdef NET_HAVE_TLS
            r = net_https_fetch(ip, (u16)port, host, (const u8 *)req, q, net_hbuf, (int)sizeof net_hbuf - 1, &n, h_done); if (r == -7) return -7;
#else
            return -2;
#endif
        } else r = tcp_get(ip, (u16)port, (const u8 *)req, q, net_hbuf, (int)sizeof net_hbuf - 1, &n, h_done);
        if (r == -2) return -5; if (r) return r == -3 ? -3 : -4;
        net_hbuf[n] = 0; int he = h_hdr_end(net_hbuf, n); if (he < 0 || n < 12 || net_hbuf[0] != 'H') return -6;
        int st = 0; for (int i = 9; i < 12; i++) { if (net_hbuf[i] < '0' || net_hbuf[i] > '9') return -6; st = st * 10 + (net_hbuf[i] - '0'); }
        net_hstatus = st; net_hloc[0] = 0; int l = h_find(net_hbuf, he, "\r\nlocation:");
        if (l >= 0) { int p = l + 11, k = 0; while (p < he && net_hbuf[p] == ' ') p++; while (p < he && net_hbuf[p] != '\r' && k < 199) net_hloc[k++] = (char)net_hbuf[p++]; net_hloc[k] = 0; }
        if ((st == 301 || st == 302 || st == 303 || st == 307 || st == 308) && net_hloc[0] && redirects < 4) {
            redirects++;
            if (net_hloc[0] == '/') { int k = 0; const char *pre = tls ? "https://" : "http://"; for (; pre[k]; k++) url[k] = pre[k]; for (int a = 0; host[a] && k < 120; a++) url[k++] = host[a]; if (port != (tls ? 443 : 80)) { url[k++] = ':'; char d[6]; int dn = 0, pv = port; while (pv) { d[dn++] = (char)('0' + pv % 10); pv /= 10; } while (dn) url[k++] = d[--dn]; } for (int a = 0; net_hloc[a] && k < 198; a++) url[k++] = net_hloc[a]; url[k] = 0; }
            else { int a = 0; for (; net_hloc[a] && a < 199; a++) url[a] = net_hloc[a]; url[a] = 0; }
            continue;
        }
        int chunked = h_find(net_hbuf, he, "\r\ntransfer-encoding: chunked") >= 0;
        net_body = net_hbuf + he; net_blen = n - he; if (chunked) net_blen = h_dechunk(net_hbuf + he, net_blen);
        
#ifdef NET_HAVE_TLS
        net_htrunc = tls ? net_tls_cut : tcp.full;
#else
        net_htrunc = tcp.full;
#endif
 net_hbuf[he + net_blen] = 0; return 0;
    }
}
#endif
