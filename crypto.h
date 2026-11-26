/* PixelBeeOS 3.1 - cryptographic primitives for TLS 1.3 (freestanding, no libc beyond memcpy/memset).
 *   SHA-256, HMAC-SHA256, HKDF-Expand-Label, AES-128, GCM, X25519, and a hash-based random generator.
 * Written for clarity, not speed. AES uses a computed S-box (table lookups are data-dependent: not constant-time
 * against a local attacker; irrelevant for a single-user hobby OS). X25519 and the tag comparison are constant-time.
 * Verified in tests/crypto_test.c against FIPS-180/197, RFC 4231, RFC 5869, RFC 7748 and NIST GCM vectors, and
 * cross-checked against Python's `cryptography` library on random inputs. */
#ifndef PIXELBEE_CRYPTO_H
#define PIXELBEE_CRYPTO_H

/* ------------------------------------------------------------ SHA-256 */
typedef struct { u32 h[8]; u8 buf[64]; u32 fill; unsigned long long bytes; } Sha256;
static const u32 sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_block(Sha256 *s, const u8 *p) {
    u32 w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (u32)p[i * 4] << 24 | (u32)p[i * 4 + 1] << 16 | (u32)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) { u32 s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10); w[i] = w[i - 16] + s0 + w[i - 7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        u32 t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha_k[i] + w[i], t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha256_init(Sha256 *s) {
    static const u32 iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, iv, sizeof iv); s->fill = 0; s->bytes = 0;
}
static void sha256_update(Sha256 *s, const u8 *d, u32 n) {
    s->bytes += n;
    while (n) { u32 t = 64 - s->fill; if (t > n) t = n; memcpy(s->buf + s->fill, d, t); s->fill += t; d += t; n -= t; if (s->fill == 64) { sha256_block(s, s->buf); s->fill = 0; } }
}
static void sha256_final(Sha256 *s, u8 out[32]) {            /* destroys s: copy it first to keep a running hash */
    unsigned long long bits = s->bytes * 8; u8 pad = 0x80; sha256_update(s, &pad, 1); pad = 0; while (s->fill != 56) sha256_update(s, &pad, 1);
    u8 L[8]; for (int i = 0; i < 8; i++) L[i] = (u8)(bits >> (56 - 8 * i)); sha256_update(s, L, 8);
    for (int i = 0; i < 8; i++) { out[i * 4] = (u8)(s->h[i] >> 24); out[i * 4 + 1] = (u8)(s->h[i] >> 16); out[i * 4 + 2] = (u8)(s->h[i] >> 8); out[i * 4 + 3] = (u8)s->h[i]; }
}
static void sha256(const u8 *d, u32 n, u8 out[32]) { Sha256 s; sha256_init(&s); sha256_update(&s, d, n); sha256_final(&s, out); }
static void sha256_peek(const Sha256 *s, u8 out[32]) { Sha256 c = *s; sha256_final(&c, out); }

/* ------------------------------------------------------------ HMAC + HKDF */
static void hmac_sha256_2(const u8 *key, u32 kl, const u8 *d1, u32 l1, const u8 *d2, u32 l2, u8 out[32]) {
    u8 k[64], pad[64], in[32]; Sha256 s; memset(k, 0, 64); if (kl > 64) sha256(key, kl, k); else memcpy(k, key, kl);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36; sha256_init(&s); sha256_update(&s, pad, 64); sha256_update(&s, d1, l1); if (l2) sha256_update(&s, d2, l2); sha256_final(&s, in);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c; sha256_init(&s); sha256_update(&s, pad, 64); sha256_update(&s, in, 32); sha256_final(&s, out);
}
static void hmac_sha256(const u8 *key, u32 kl, const u8 *d, u32 n, u8 out[32]) { hmac_sha256_2(key, kl, d, n, 0, 0, out); }
static void hkdf_extract(const u8 *salt, u32 sl, const u8 *ikm, u32 il, u8 out[32]) { u8 z[32]; if (!sl) { memset(z, 0, 32); salt = z; sl = 32; } hmac_sha256(salt, sl, ikm, il, out); }
/* HKDF-Expand for output <= 32 bytes */
static void hkdf_expand32(const u8 *prk, const u8 *info, u32 il, u32 outlen, u8 *out) { u8 t[32]; u8 one = 1; hmac_sha256_2(prk, 32, info, il, &one, 1, t); memcpy(out, t, outlen); }
/* TLS 1.3 HKDF-Expand-Label(secret, label, context, length) */
static void hkdf_label(const u8 secret[32], const char *label, const u8 *ctx, u32 cl, u32 outlen, u8 *out) {
    u8 info[2 + 1 + 6 + 12 + 1 + 32]; int p = 0, ll = 0; while (label[ll]) ll++;
    info[p++] = (u8)(outlen >> 8); info[p++] = (u8)outlen; info[p++] = (u8)(6 + ll); memcpy(info + p, "tls13 ", 6); p += 6; memcpy(info + p, label, (u32)ll); p += ll; info[p++] = (u8)cl; if (cl) { memcpy(info + p, ctx, cl); p += (int)cl; }
    hkdf_expand32(secret, info, (u32)p, outlen, out);
}

/* ------------------------------------------------------------ AES-128 (encrypt only) + GCM */
static u8 aes_sb[256]; static int aes_ready;
static void aes_tables(void) {
    if (aes_ready) return; u8 p = 1, q = 1;
    do { p = (u8)(p ^ (u8)(p << 1) ^ ((p & 0x80) ? 0x1B : 0)); q ^= (u8)(q << 1); q ^= (u8)(q << 2); q ^= (u8)(q << 4); if (q & 0x80) q ^= 0x09;
         u8 x = (u8)(q ^ (u8)((q << 1) | (q >> 7)) ^ (u8)((q << 2) | (q >> 6)) ^ (u8)((q << 3) | (q >> 5)) ^ (u8)((q << 4) | (q >> 4))); aes_sb[p] = (u8)(x ^ 0x63); } while (p != 1);
    aes_sb[0] = 0x63; aes_ready = 1;
}
typedef struct { u8 rk[176]; u8 h[16]; } Gcm;                  /* AES-128 key schedule + GHASH key */
static u8 xt(u8 x) { return (u8)((x << 1) ^ ((x & 0x80) ? 0x1B : 0)); }
static void aes_enc(const u8 *rk, const u8 in[16], u8 out[16]) {
    u8 s[16]; for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (int r = 1; r <= 10; r++) {
        u8 t[16]; for (int i = 0; i < 16; i++) t[i] = aes_sb[s[i]];
        u8 u[16]; for (int c = 0; c < 4; c++) for (int row = 0; row < 4; row++) u[c * 4 + row] = t[((c + row) & 3) * 4 + row];     /* ShiftRows */
        if (r < 10) for (int c = 0; c < 4; c++) { u8 *a = u + c * 4, a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
            a[0] = (u8)(xt(a0) ^ xt(a1) ^ a1 ^ a2 ^ a3); a[1] = (u8)(a0 ^ xt(a1) ^ xt(a2) ^ a2 ^ a3); a[2] = (u8)(a0 ^ a1 ^ xt(a2) ^ xt(a3) ^ a3); a[3] = (u8)(xt(a0) ^ a0 ^ a1 ^ a2 ^ xt(a3)); }
        for (int i = 0; i < 16; i++) s[i] = u[i] ^ rk[r * 16 + i];
    }
    memcpy(out, s, 16);
}
static void gcm_key(Gcm *g, const u8 key[16]) {
    aes_tables(); memcpy(g->rk, key, 16); u8 rc = 1;
    for (int i = 4; i < 44; i++) { u8 t[4]; memcpy(t, g->rk + (i - 1) * 4, 4);
        if (i % 4 == 0) { u8 x = t[0]; t[0] = (u8)(aes_sb[t[1]] ^ rc); t[1] = aes_sb[t[2]]; t[2] = aes_sb[t[3]]; t[3] = aes_sb[x]; rc = xt(rc); }
        for (int k = 0; k < 4; k++) g->rk[i * 4 + k] = (u8)(g->rk[(i - 4) * 4 + k] ^ t[k]); }
    u8 z[16]; memset(z, 0, 16); aes_enc(g->rk, z, g->h);
}
static void gf_mul(u8 x[16], const u8 h[16]) {
    u8 z[16], v[16]; memset(z, 0, 16); memcpy(v, h, 16);
    for (int i = 0; i < 128; i++) {
        if ((x[i >> 3] >> (7 - (i & 7))) & 1) for (int k = 0; k < 16; k++) z[k] ^= v[k];
        u8 lsb = v[15] & 1; for (int k = 15; k > 0; k--) v[k] = (u8)((v[k] >> 1) | (v[k - 1] << 7)); v[0] >>= 1; if (lsb) v[0] ^= 0xE1;
    }
    memcpy(x, z, 16);
}
static void ghash_upd(const Gcm *g, u8 y[16], const u8 *d, u32 n) { while (n) { u32 t = n < 16 ? n : 16; for (u32 i = 0; i < t; i++) y[i] ^= d[i]; gf_mul(y, g->h); d += t; n -= t; } }
static void gcm_ctr(const Gcm *g, const u8 iv[12], u32 ctr0, const u8 *in, u32 n, u8 *out) {
    u8 cb[16], ks[16]; memcpy(cb, iv, 12); u32 c = ctr0;
    while (n) { cb[12] = (u8)(c >> 24); cb[13] = (u8)(c >> 16); cb[14] = (u8)(c >> 8); cb[15] = (u8)c; aes_enc(g->rk, cb, ks); u32 t = n < 16 ? n : 16; for (u32 i = 0; i < t; i++) out[i] = in[i] ^ ks[i]; in += t; out += t; n -= t; c++; }
}
static void gcm_tag(const Gcm *g, const u8 iv[12], const u8 *aad, u32 al, const u8 *ct, u32 cl, u8 tag[16]) {
    u8 y[16], L[16], j0[16], ek[16]; memset(y, 0, 16); ghash_upd(g, y, aad, al); ghash_upd(g, y, ct, cl);
    unsigned long long ab = (unsigned long long)al * 8, cb = (unsigned long long)cl * 8; for (int i = 0; i < 8; i++) { L[i] = (u8)(ab >> (56 - 8 * i)); L[8 + i] = (u8)(cb >> (56 - 8 * i)); }
    ghash_upd(g, y, L, 16); memcpy(j0, iv, 12); j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1; aes_enc(g->rk, j0, ek); for (int i = 0; i < 16; i++) tag[i] = y[i] ^ ek[i];
}
/* out = ciphertext (pl bytes) followed by the 16-byte tag */
static void gcm_seal(const Gcm *g, const u8 iv[12], const u8 *aad, u32 al, const u8 *pt, u32 pl, u8 *out) { gcm_ctr(g, iv, 2, pt, pl, out); gcm_tag(g, iv, aad, al, out, pl, out + pl); }
/* in = ciphertext followed by tag (cl bytes of ciphertext). Returns 0 on success; on failure nothing is written to out. */
static int gcm_open(const Gcm *g, const u8 iv[12], const u8 *aad, u32 al, const u8 *in, u32 cl, u8 *out) {
    u8 tag[16], diff = 0; gcm_tag(g, iv, aad, al, in, cl, tag); for (int i = 0; i < 16; i++) diff |= (u8)(tag[i] ^ in[cl + (u32)i]);
    if (diff) return -1; gcm_ctr(g, iv, 2, in, cl, out); return 0;
}

/* ------------------------------------------------------------ X25519 (TweetNaCl field arithmetic, public domain design) */
typedef long long i64; typedef i64 gf[16];
static const gf gf_121665 = {0xDB41, 1};
static void gf_car(gf o) { i64 c; for (int i = 0; i < 16; i++) { o[i] += (1LL << 16); c = o[i] >> 16; o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15); o[i] -= c * 65536; } }
static void gf_sel(gf p, gf q, int b) { i64 c = ~((i64)b - 1), t; for (int i = 0; i < 16; i++) { t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; } }
static void gf_pack(u8 *o, const gf n) {
    int b; gf m, t; for (int i = 0; i < 16; i++) t[i] = n[i]; gf_car(t); gf_car(t); gf_car(t);
    for (int j = 0; j < 2; j++) { m[0] = t[0] - 0xffed; for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; } m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1); b = (int)((m[15] >> 16) & 1); m[14] &= 0xffff; gf_sel(t, m, 1 - b); }
    for (int i = 0; i < 16; i++) { o[2 * i] = (u8)(t[i] & 0xff); o[2 * i + 1] = (u8)(t[i] >> 8); }
}
static void gf_unpack(gf o, const u8 *n) { for (int i = 0; i < 16; i++) o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8); o[15] &= 0x7fff; }
static void gf_add(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void gf_sub(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void gf_mul_(gf o, const gf a, const gf b) { i64 t[31]; for (int i = 0; i < 31; i++) t[i] = 0; for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j]; for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16]; for (int i = 0; i < 16; i++) o[i] = t[i]; gf_car(o); gf_car(o); }
static void gf_inv(gf o, const gf in) { gf c; for (int a = 0; a < 16; a++) c[a] = in[a]; for (int a = 253; a >= 0; a--) { gf_mul_(c, c, c); if (a != 2 && a != 4) gf_mul_(c, c, in); } for (int a = 0; a < 16; a++) o[a] = c[a]; }
static void x25519(u8 q[32], const u8 n[32], const u8 p[32]) {
    u8 z[32]; i64 x[80], r; gf a, b, c, d, e, f; for (int i = 0; i < 31; i++) z[i] = n[i]; z[31] = (u8)((n[31] & 127) | 64); z[0] &= 248; gf_unpack(x, p);
    for (int i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; } a[0] = d[0] = 1;
    for (int i = 254; i >= 0; --i) {
        r = (z[i >> 3] >> (i & 7)) & 1; gf_sel(a, b, (int)r); gf_sel(c, d, (int)r);
        gf_add(e, a, c); gf_sub(a, a, c); gf_add(c, b, d); gf_sub(b, b, d); gf_mul_(d, e, e); gf_mul_(f, a, a); gf_mul_(a, c, a); gf_mul_(c, b, e);
        gf_add(e, a, c); gf_sub(a, a, c); gf_mul_(b, a, a); gf_sub(c, d, f); gf_mul_(a, c, gf_121665); gf_add(a, a, d); gf_mul_(c, c, a); gf_mul_(a, d, f); gf_mul_(d, b, x); gf_mul_(b, e, e);
        gf_sel(a, b, (int)r); gf_sel(c, d, (int)r);
    }
    for (int i = 0; i < 16; i++) { x[i + 16] = a[i]; x[i + 32] = c[i]; x[i + 48] = b[i]; x[i + 64] = d[i]; }
    gf_inv(x + 32, x + 32); gf_mul_(x + 16, x + 16, x + 32); gf_pack(q, x + 16);
}
static void x25519_base(u8 q[32], const u8 n[32]) { u8 nine[32]; memset(nine, 0, 32); nine[0] = 9; x25519(q, n, nine); }

/* ------------------------------------------------------------ random numbers */
static u8 rng_state[32]; static u32 rng_ctr; static int rng_seeded; static u32 rng_events;
#if defined(__i386__) || defined(__x86_64__)
static inline unsigned long long rng_tsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return ((unsigned long long)hi << 32) | lo; }
static int rng_rdrand(u32 *out) {
    u32 a, b, c, d; __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(1), "2"(0)); if (!(c & (1u << 30))) return 0;
    for (int t = 0; t < 10; t++) { u32 v; unsigned char ok; __asm__ volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok)); if (ok) { *out = v; return 1; } } return 0;
}
#else
static inline unsigned long long rng_tsc(void) { return 0; } static int rng_rdrand(u32 *o) { (void)o; return 0; }
#endif
static void rng_add(const u8 *d, u32 n) { Sha256 s; sha256_init(&s); sha256_update(&s, rng_state, 32); sha256_update(&s, d, n); sha256_final(&s, rng_state); }
/* call with anything unpredictable and cheap: packet arrival times, key presses, mouse moves */
static void rng_event(u32 extra) { unsigned long long t = rng_tsc(); u8 b[12]; for (int i = 0; i < 8; i++) b[i] = (u8)(t >> (8 * i)); for (int i = 0; i < 4; i++) b[8 + i] = (u8)(extra >> (8 * i)); rng_add(b, 12); rng_events++; }
static void rng_bytes(u8 *out, u32 n) {
    if (!rng_seeded) { for (int i = 0; i < 8; i++) { u32 v; if (rng_rdrand(&v)) rng_add((u8 *)&v, 4); rng_event((u32)i); } rng_seeded = 1; }
    rng_event(rng_ctr);                                                           /* fresh timing jitter on every call */
    while (n) { u8 blk[32], c[4]; c[0] = (u8)rng_ctr; c[1] = (u8)(rng_ctr >> 8); c[2] = (u8)(rng_ctr >> 16); c[3] = (u8)(rng_ctr >> 24); rng_ctr++;
        Sha256 s; sha256_init(&s); sha256_update(&s, rng_state, 32); sha256_update(&s, c, 4); sha256_final(&s, blk); u32 t = n < 32 ? n : 32; memcpy(out, blk, t); out += t; n -= t; }
    u8 z = 0; rng_add(&z, 1);                                                      /* ratchet: past outputs cannot be recomputed from the new state */
}
#endif
