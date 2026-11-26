/* PixelBeeOS 3.1 - glue between the kernel TCP layer (net.h) and the TLS 1.3 client (tls.h) -> https:// fetches. */
#ifndef PIXELBEE_NETTLS_H
#define PIXELBEE_NETTLS_H
static u8 net_traw[48000];
static int ntls_write(const u8 *d, int n) { return tcp_write(d, n); }
static const char *net_tls_errstr(void) { return tls.err[0] ? tls.err : "secure connection failed"; }
static int net_https_fetch(const u8 *ip, u16 port, const char *host, const u8 *req, int reqlen, u8 *buf, int bufmax, int *outlen, int (*done)(const u8 *, int)) {
    *outlen = 0; net_tls_cut = 0; int r = tcp_open(ip, port, net_traw, (int)sizeof net_traw); if (r) return r == -2 ? -2 : (r == -3 ? -3 : -4);
    tls_io.write = ntls_write; tls_io.need = tcp_need; tls_io.data = tcp_data; tls_io.eat = tcp_eat; rng_add(net_nic.mac, 6); rng_event(now_ms());
    int dotted = 1; for (int i = 0; host[i]; i++) if (!((host[i] >= '0' && host[i] <= '9') || host[i] == '.')) dotted = 0;
    r = tls_connect(dotted ? 0 : host); if (r) { tcp_close(); return r == -2 ? -4 : -7; }
    if (tls_write(req, reqlen)) { tcp_close(); return -4; }
    int total = 0; while (total < bufmax) { int k = tls_read(buf + total, bufmax - total, &net_tls_cut, 6000); if (k > 0) { total += k; if (done && done(buf, total)) break; } else { if (k == -1) { tcp_close(); return -7; } break; } }
    if (total >= bufmax) net_tls_cut = 1;
    tls_close(); tcp_close(); *outlen = total; return 0;
}
#endif
