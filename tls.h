/* PixelBeeOS 3.1 - TLS 1.3 client (RFC 8446): TLS_AES_128_GCM_SHA256, x25519, one connection at a time.
 *
 * WHAT IT PROTECTS: the connection is encrypted and integrity-protected, and the server proves it holds the handshake
 * keys (Finished is verified). That stops anyone who merely LISTENS to the network.
 * WHAT IT DOES NOT DO (yet): it does NOT validate the server's certificate chain or CertificateVerify signature.
 * An attacker who can actively intercept your connection (a hostile Wi-Fi, a compromised router) could impersonate
 * the site. Treat https:// pages as "encrypted, identity unverified" - do not type passwords or card numbers.
 * Not supported: TLS 1.2 or older servers, HelloRetryRequest, session resumption, client certificates, ALPN.
 *
 * Transport is abstracted (TlsIo) so the same code runs over the kernel's TCP layer and over a host socket in tests.
 * Needs crypto.h first. */
#ifndef PIXELBEE_TLS_H
#define PIXELBEE_TLS_H
typedef struct {
    int (*write)(const u8 *d, int n);            /* reliable send; 0 = ok */
    int (*need)(int n, u32 ms);                  /* wait until n bytes are buffered; 1 = yes, 0 = timeout or closed */
    const u8 *(*data)(void);                     /* the buffered bytes */
    void (*eat)(int n);                          /* drop n buffered bytes */
} TlsIo;
static TlsIo tls_io;
static struct { Sha256 th; Gcm cw, sw; u8 civ[12], siv[12], c_sec[32], s_sec[32]; unsigned long long cseq, sseq; int cenc, senc, open, verified; char err[64]; } tls;
static u8 tls_pt[16384 + 512], tls_hs[24000], tls_rec[1500]; static int tls_hslen;

static int tls_fail(const char *m) { int i = 0; for (; m[i] && i < 63; i++) tls.err[i] = m[i]; tls.err[i] = 0; tls.open = 0; return -1; }
static void tls_nonce(const u8 *iv, unsigned long long seq, u8 *out) { memcpy(out, iv, 12); for (int i = 0; i < 8; i++) out[11 - i] ^= (u8)(seq >> (8 * i)); }
static void tls_keys(const u8 secret[32], Gcm *g, u8 iv[12]) { u8 k[16]; hkdf_label(secret, "key", 0, 0, 16, k); hkdf_label(secret, "iv", 0, 0, 12, iv); gcm_key(g, k); }

/* send one record. Encrypted once the client keys are active. inner = real content type. */
static int tls_send(u8 type, const u8 *d, int n) {
    if (n > 1300) return -1; u8 *r = tls_rec;
    if (!tls.cenc) { r[0] = type; r[1] = 3; r[2] = type == 22 ? 1 : 3; r[3] = (u8)(n >> 8); r[4] = (u8)n; memcpy(r + 5, d, (unsigned)n); return tls_io.write(r, 5 + n); }
    u8 pt[1320], nonce[12]; memcpy(pt, d, (unsigned)n); pt[n] = type; int cl = n + 1 + 16;
    r[0] = 23; r[1] = 3; r[2] = 3; r[3] = (u8)(cl >> 8); r[4] = (u8)cl; tls_nonce(tls.civ, tls.cseq++, nonce); gcm_seal(&tls.cw, nonce, r, 5, pt, (u32)(n + 1), r + 5);
    return tls_io.write(r, 5 + cl);
}
/* read the next record. Returns inner content type (22 handshake, 23 data, 21 alert) with data in tls_pt, or <0 on error / timeout. */
static int tls_recv(int *len, u32 ms) {
    for (;;) {
        if (!tls_io.need(5, ms)) return -2; const u8 *h = tls_io.data(); u8 rt = h[0]; int rl = h[1] == 3 ? (h[3] << 8 | h[4]) : -1;
        if (rl < 0 || rl > 16384 + 256) return tls_fail("bad record header");
        if (!tls_io.need(5 + rl, ms)) return -2; h = tls_io.data();
        if (rt == 20) { tls_io.eat(5 + rl); continue; }                                       /* change_cipher_spec: ignored in TLS 1.3 */
        if (rt == 23 && tls.senc) {
            u8 nonce[12]; if (rl < 17) return tls_fail("short record"); tls_nonce(tls.siv, tls.sseq, nonce);
            if (gcm_open(&tls.sw, nonce, h, 5, h + 5, (u32)(rl - 16), tls_pt)) return tls_fail("bad record MAC (connection tampered?)");
            tls.sseq++; tls_io.eat(5 + rl); int j = rl - 16 - 1; while (j >= 0 && tls_pt[j] == 0) j--; if (j < 0) return tls_fail("empty record"); *len = j; return tls_pt[j];
        }
        if (rt == 23) return tls_fail("unexpected encrypted record");
        memcpy(tls_pt, h + 5, (unsigned)rl); tls_io.eat(5 + rl); *len = rl; return rt;
    }
}
static int tls_ct_eq(const u8 *a, const u8 *b, int n) { u8 d = 0; for (int i = 0; i < n; i++) d |= (u8)(a[i] ^ b[i]); return d == 0; }
static void tls_th(const u8 *d, int n) { sha256_update(&tls.th, d, (u32)n); }

/* Performs the handshake. host = SNI name (NULL for none). 0 = ok, -1 = protocol error (tls.err), -2 = timeout. */
static int tls_connect(const char *host) {
    static const u8 zero32[32] = {0}; u8 priv[32], pub[32], rnd[32], sid[32], empty[32]; static u8 ch[600]; int p = 0;
    memset(&tls, 0, sizeof tls); tls_hslen = 0; sha256((const u8 *)"", 0, empty);
    rng_bytes(priv, 32); x25519_base(pub, priv); rng_bytes(rnd, 32); rng_bytes(sid, 32);
    ch[p++] = 1; p += 3; ch[p++] = 3; ch[p++] = 3; memcpy(ch + p, rnd, 32); p += 32; ch[p++] = 32; memcpy(ch + p, sid, 32); p += 32;
    ch[p++] = 0; ch[p++] = 2; ch[p++] = 0x13; ch[p++] = 0x01; ch[p++] = 1; ch[p++] = 0;
    int ext0 = p; p += 2; int hl = 0; if (host) while (host[hl]) hl++;
    if (hl > 0 && hl < 200) { ch[p++] = 0; ch[p++] = 0; int el = hl + 5; ch[p++] = (u8)(el >> 8); ch[p++] = (u8)el; ch[p++] = (u8)((hl + 3) >> 8); ch[p++] = (u8)(hl + 3); ch[p++] = 0; ch[p++] = (u8)(hl >> 8); ch[p++] = (u8)hl; memcpy(ch + p, host, (unsigned)hl); p += hl; }
    ch[p++] = 0; ch[p++] = 43; ch[p++] = 0; ch[p++] = 3; ch[p++] = 2; ch[p++] = 3; ch[p++] = 4;                                  /* supported_versions: 1.3 */
    ch[p++] = 0; ch[p++] = 10; ch[p++] = 0; ch[p++] = 4; ch[p++] = 0; ch[p++] = 2; ch[p++] = 0; ch[p++] = 0x1d;               /* supported_groups: x25519 */
    { static const u8 sa[] = {0x08, 0x04, 0x04, 0x03, 0x04, 0x01, 0x08, 0x05, 0x05, 0x03, 0x05, 0x01, 0x08, 0x06, 0x08, 0x07}; ch[p++] = 0; ch[p++] = 13; ch[p++] = 0; ch[p++] = (u8)(2 + sizeof sa); ch[p++] = 0; ch[p++] = (u8)sizeof sa; memcpy(ch + p, sa, sizeof sa); p += (int)sizeof sa; }
    ch[p++] = 0; ch[p++] = 51; ch[p++] = 0; ch[p++] = 38; ch[p++] = 0; ch[p++] = 36; ch[p++] = 0; ch[p++] = 0x1d; ch[p++] = 0; ch[p++] = 32; memcpy(ch + p, pub, 32); p += 32;     /* key_share */
    ch[ext0] = (u8)((p - ext0 - 2) >> 8); ch[ext0 + 1] = (u8)(p - ext0 - 2); ch[1] = 0; ch[2] = (u8)((p - 4) >> 8); ch[3] = (u8)(p - 4);
    sha256_init(&tls.th); tls_th(ch, p); if (tls_send(22, ch, p)) return tls_fail("send failed");
    /* ---- ServerHello ---- */
    int len, t = tls_recv(&len, 8000); if (t == -2) return -2; if (t < 0) return -1; if (t == 21) return tls_fail("server rejected the handshake (alert)"); if (t != 22 || len < 42 || tls_pt[0] != 2) return tls_fail("expected ServerHello");
    static u8 sh[2048]; if (len > (int)sizeof sh) return tls_fail("ServerHello too large"); memcpy(sh, tls_pt, (unsigned)len); tls_th(sh, len);
    static const u8 hrr[32] = {0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91, 0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C};
    if (tls_ct_eq(sh + 6, hrr, 32)) return tls_fail("server asked for a different key type (HelloRetryRequest)");
    int q = 4 + 2 + 32, sl = sh[q]; q += 1 + sl; if (q + 3 > len) return tls_fail("bad ServerHello");
    if (sh[q] != 0x13 || sh[q + 1] != 0x01) return tls_fail("server chose an unsupported cipher"); q += 3; if (q + 2 > len) return tls_fail("bad ServerHello");
    int el = sh[q] << 8 | sh[q + 1]; q += 2; int ver = 0; const u8 *spub = 0; int end = q + el; if (end > len) return tls_fail("bad ServerHello extensions");
    while (q + 4 <= end) { int et = sh[q] << 8 | sh[q + 1], ell = sh[q + 2] << 8 | sh[q + 3]; q += 4; if (q + ell > end) return tls_fail("bad extension");
        if (et == 43 && ell == 2) ver = sh[q] << 8 | sh[q + 1]; else if (et == 51 && ell == 36 && sh[q] == 0 && sh[q + 1] == 0x1d && sh[q + 2] == 0 && sh[q + 3] == 32) spub = sh + q + 4; q += ell; }
    if (ver != 0x0304) return tls_fail("server does not speak TLS 1.3"); if (!spub) return tls_fail("server sent no x25519 key");
    u8 shared[32]; x25519(shared, priv, spub); { u8 any = 0; for (int i = 0; i < 32; i++) any |= shared[i]; if (!any) return tls_fail("bad server key"); }
    u8 early[32], derived[32], hs[32], th[32], c_hs[32], s_hs[32]; hkdf_extract(0, 0, zero32, 32, early); hkdf_label(early, "derived", empty, 32, 32, derived); hkdf_extract(derived, 32, shared, 32, hs);
    sha256_peek(&tls.th, th); hkdf_label(hs, "c hs traffic", th, 32, 32, c_hs); hkdf_label(hs, "s hs traffic", th, 32, 32, s_hs);
    tls_keys(s_hs, &tls.sw, tls.siv); tls.sseq = 0; tls.senc = 1;
    /* ---- encrypted server flight: EncryptedExtensions, Certificate, CertificateVerify, Finished ---- */
    int done = 0; u8 th_after[32];
    while (!done) {
        t = tls_recv(&len, 10000); if (t == -2) return -2; if (t < 0) return -1; if (t == 21) return tls_fail("server alert during handshake"); if (t != 22) return tls_fail("unexpected record in handshake");
        if (tls_hslen + len > (int)sizeof tls_hs) return tls_fail("server certificate chain too large"); memcpy(tls_hs + tls_hslen, tls_pt, (unsigned)len); tls_hslen += len;
        while (tls_hslen >= 4) {
            int mt = tls_hs[0], ml = tls_hs[1] << 16 | tls_hs[2] << 8 | tls_hs[3]; if (4 + ml > (int)sizeof tls_hs) return tls_fail("handshake message too large"); if (tls_hslen < 4 + ml) break;
            if (mt == 20) {
                u8 fk[32], want[32], before[32]; if (ml != 32) return tls_fail("bad Finished"); sha256_peek(&tls.th, before); hkdf_label(s_hs, "finished", 0, 0, 32, fk); hmac_sha256(fk, 32, before, 32, want);
                if (!tls_ct_eq(want, tls_hs + 4, 32)) return tls_fail("server Finished does not verify"); tls_th(tls_hs, 4 + ml); done = 1;
            } else if (mt == 8 || mt == 11 || mt == 15) tls_th(tls_hs, 4 + ml);                      /* EncryptedExtensions / Certificate / CertificateVerify: hashed, NOT validated */
            else return tls_fail("unexpected handshake message");
            memmove(tls_hs, tls_hs + 4 + ml, (unsigned)(tls_hslen - 4 - ml)); tls_hslen -= 4 + ml; if (done) break;
        }
    }
    sha256_peek(&tls.th, th_after);
    u8 derived2[32], master[32], cfk[32], verify[32], fin[36]; hkdf_label(hs, "derived", empty, 32, 32, derived2); hkdf_extract(derived2, 32, zero32, 32, master);
    hkdf_label(master, "c ap traffic", th_after, 32, 32, tls.c_sec); hkdf_label(master, "s ap traffic", th_after, 32, 32, tls.s_sec);
    /* ---- client Finished under the client handshake keys, then switch everything to application keys ---- */
    hkdf_label(c_hs, "finished", 0, 0, 32, cfk); hmac_sha256(cfk, 32, th_after, 32, verify); fin[0] = 20; fin[1] = 0; fin[2] = 0; fin[3] = 32; memcpy(fin + 4, verify, 32);
    static const u8 ccs[6] = {0x14, 3, 3, 0, 1, 1}; if (tls_io.write(ccs, 6)) return tls_fail("send failed");
    tls_keys(c_hs, &tls.cw, tls.civ); tls.cseq = 0; tls.cenc = 1; if (tls_send(22, fin, 36)) return tls_fail("send failed");
    tls_keys(tls.c_sec, &tls.cw, tls.civ); tls.cseq = 0; tls_keys(tls.s_sec, &tls.sw, tls.siv); tls.sseq = 0; tls.open = 1; tls.verified = 0; return 0;
}
static int tls_write(const u8 *d, int n) { while (n > 0) { int c = n > 1200 ? 1200 : n; if (tls_send(23, d, c)) return -1; d += c; n -= c; } return 0; }
/* Next chunk of application data into out (max bytes; extra is dropped and *cut set). >0 bytes, 0 = server closed cleanly, -2 = timeout, -1 = error */
static int tls_read(u8 *out, int max, int *cut, u32 ms) {
    for (;;) {
        int len, t = tls_recv(&len, ms); if (t < 0) return t;
        if (t == 23) { if (len > max) { len = max; *cut = 1; } memcpy(out, tls_pt, (unsigned)len); if (len > 0) return len; continue; }
        if (t == 21) { tls.open = 0; return (len >= 2 && tls_pt[1] == 0) ? 0 : tls_fail("server sent an error alert"); }
        if (t == 22) {                                                                                   /* NewSessionTicket is ignored; KeyUpdate re-keys */
            int p = 0; while (p + 4 <= len) { int mt = tls_pt[p], ml = tls_pt[p + 1] << 16 | tls_pt[p + 2] << 8 | tls_pt[p + 3]; if (p + 4 + ml > len) break;
                if (mt == 24 && ml == 1) { u8 ns[32]; hkdf_label(tls.s_sec, "traffic upd", 0, 0, 32, ns); memcpy(tls.s_sec, ns, 32); tls_keys(tls.s_sec, &tls.sw, tls.siv); tls.sseq = 0;
                    if (tls_pt[p + 4] == 1) { static const u8 ku[5] = {24, 0, 0, 1, 0}; tls_send(22, ku, 5); hkdf_label(tls.c_sec, "traffic upd", 0, 0, 32, ns); memcpy(tls.c_sec, ns, 32); tls_keys(tls.c_sec, &tls.cw, tls.civ); tls.cseq = 0; } }
                p += 4 + ml; }
        }
    }
}
static void tls_close(void) { static const u8 al[2] = {1, 0}; if (tls.open) tls_send(21, al, 2); tls.open = 0; }
#endif
