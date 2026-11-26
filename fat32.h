/* PixelBeeOS 3.1 - block-device layer + FAT32 filesystem (read, write, long file names, format).
 *
 * Freestanding: needs only u8/u16/u32 and memcpy/memset/memmove from the including file.
 *
 * Safety rules baked into the driver:
 *  - A volume is WRITABLE only when its boot-sector label is exactly F32_DATA_LABEL ("PIXELBEEDAT").
 *    Any other FAT32 volume mounts READ-ONLY, so an unknown disk is never modified.
 *  - Every sector access is bounds-checked against the block device and the volume.
 *  - Write ordering: new data clusters are written first, then the FAT chain, then the directory entry,
 *    and only then are old clusters released. A power cut can leak space but never points a file at garbage.
 *  - The FAT "clean shutdown" bit is cleared while a volume is mounted writable and set again by f32_unmount().
 *  - f32_format() only touches the sector range it is given (one partition), never the whole disk.
 */
#ifndef PIXELBEE_FAT32_H
#define PIXELBEE_FAT32_H

#define F32_DATA_LABEL "PIXELBEEDAT"
#define F32_OK      0
#define F32_EIO    -1
#define F32_ENOENT -2
#define F32_EEXIST -3
#define F32_ENOSPC -4
#define F32_ENAME  -5
#define F32_ENOTDIR -6
#define F32_EISDIR -7
#define F32_EROFS  -8
#define F32_ENOTEMPTY -9
#define F32_ECORRUPT -10

/* ------------------------------------------------------------ block layer */
typedef struct BlockDev {
    u32 sectors;                                                    /* total 512-byte sectors */
    int (*read)(void *ctx, u32 lba, u32 count, void *buf);          /* 0 = success */
    int (*write)(void *ctx, u32 lba, u32 count, const void *buf);   /* 0 = success */
    void *ctx; int readonly; char name[8];
} BlockDev;

static int blk_read(BlockDev *d, u32 lba, u32 n, void *b) {
    if (!d || !d->read || n == 0 || lba >= d->sectors || n > d->sectors - lba) return F32_EIO;
    return d->read(d->ctx, lba, n, b) ? F32_EIO : 0;
}
static int blk_write(BlockDev *d, u32 lba, u32 n, const void *b) {
    if (!d || !d->write || d->readonly) return F32_EROFS;
    if (n == 0 || lba >= d->sectors || n > d->sectors - lba) return F32_EIO;
    return d->write(d->ctx, lba, n, b) ? F32_EIO : 0;
}

/* MBR partition table: idx 0..3 -> start LBA, sector count, type byte. Returns 1 if the slot is used. */
static int mbr_part(BlockDev *d, int idx, u32 *lba, u32 *n, u8 *type) {
    u8 s[512]; if (idx < 0 || idx > 3 || blk_read(d, 0, 1, s)) return 0;
    if (s[510] != 0x55 || s[511] != 0xAA) return 0;
    const u8 *p = s + 446 + idx * 16; u32 st = (u32)(p[8] | p[9] << 8 | p[10] << 16 | (u32)p[11] << 24), ct = (u32)(p[12] | p[13] << 8 | p[14] << 16 | (u32)p[15] << 24);
    if (!p[4] || !ct || st >= d->sectors || ct > d->sectors - st) return 0;
    *lba = st; *n = ct; *type = p[4]; return 1;
}

/* ------------------------------------------------------------ little-endian helpers */
static u16 f_r16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 f_r32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static void f_w16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void f_w32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

/* ------------------------------------------------------------ volume */
typedef struct {
    BlockDev *dev; u32 base, total;
    u32 spc, rsvd, nfats, fatsz, fat0, data0, root, nclus, hint, fsinfo;
    int ro, mounted, was_dirty; char label[12];
    u8 fb[512]; u32 fb_lba;          /* cached FAT sector */
    u8 db[512]; u32 db_lba;          /* cached directory sector */
    u32 dc_dir, dc_idx, dc_cl;       /* directory chain-walk cache */
    u32 free_cnt;                    /* 0xFFFFFFFF = unknown */
} F32;
typedef struct { char name[256]; u8 attr; u32 cl, size; u32 first, ent, dircl; } F32Ent;

static u16 f32_date = (46 << 9) | (1 << 5) | 1, f32_time;      /* 2026-01-01 until the kernel sets the clock */
static void f32_set_clock(int y, int mo, int d, int h, int mi, int s) {
    if (y < 1980) y = 1980; if (y > 2107) y = 2107;
    f32_date = (u16)(((y - 1980) << 9) | (mo << 5) | d); f32_time = (u16)((h << 11) | (mi << 5) | (s / 2));
}

static int f_ok_cl(const F32 *v, u32 cl) { return cl >= 2 && cl < v->nclus + 2; }
static u32 f_lba(const F32 *v, u32 cl) { return v->data0 + (cl - 2) * v->spc; }
static void f_invalidate(F32 *v) { v->db_lba = 0xFFFFFFFFu; v->dc_dir = 0xFFFFFFFFu; }

static int f_fatget(F32 *v, u32 cl, u32 *out) {
    if (cl >= v->nclus + 2) return F32_ECORRUPT;
    u32 sec = v->fat0 + cl / 128;
    if (v->fb_lba != sec) { if (blk_read(v->dev, sec, 1, v->fb)) { v->fb_lba = 0xFFFFFFFFu; return F32_EIO; } v->fb_lba = sec; }
    *out = f_r32(v->fb + (cl % 128) * 4) & 0x0FFFFFFFu; return 0;
}
static int f_fatset(F32 *v, u32 cl, u32 val) {
    if (v->ro) return F32_EROFS;
    u32 old; int r = f_fatget(v, cl, &old); if (r) return r;
    u8 *p = v->fb + (cl % 128) * 4; f_w32(p, (f_r32(p) & 0xF0000000u) | (val & 0x0FFFFFFFu));
    for (u32 k = 0; k < v->nfats; k++) if ((r = blk_write(v->dev, v->fb_lba + k * v->fatsz, 1, v->fb))) return r;
    return 0;
}
static int f_alloc(F32 *v, u32 *out) {
    for (u32 n = 0; n < v->nclus; n++) {
        u32 cl = 2 + (v->hint - 2 + n) % v->nclus, val; int r = f_fatget(v, cl, &val); if (r) return r;
        if (val == 0) {
            if ((r = f_fatset(v, cl, 0x0FFFFFFFu))) return r;
            v->hint = cl + 1; if (v->hint >= v->nclus + 2) v->hint = 2;
            if (v->free_cnt != 0xFFFFFFFFu && v->free_cnt) v->free_cnt--;
            *out = cl; return 0;
        }
    }
    return F32_ENOSPC;
}
static int f_freechain(F32 *v, u32 cl) {
    for (u32 guard = 0; f_ok_cl(v, cl) && guard < v->nclus; guard++) {
        u32 nx; int r = f_fatget(v, cl, &nx); if (r) return r;
        if ((r = f_fatset(v, cl, 0))) return r;
        if (v->free_cnt != 0xFFFFFFFFu) v->free_cnt++;
        if (nx >= 0x0FFFFFF8u) break; cl = nx;
    }
    f_invalidate(v); return 0;
}
static int f_zero_cluster(F32 *v, u32 cl) {
    u8 z[512]; memset(z, 0, 512); f_invalidate(v);
    for (u32 s = 0; s < v->spc; s++) { int r = blk_write(v->dev, f_lba(v, cl) + s, 1, z); if (r) return r; }
    return 0;
}

/* ------------------------------------------------------------ directory entry access */
/* returns 0 + pointer into v->db, 1 when the entry number lies past the end of the chain, <0 on error */
static int f_dget(F32 *v, u32 dircl, u32 no, u8 **ent) {
    u32 per = v->spc * 16, ci = no / per, rem = no % per, cl = dircl, start = 0;
    if (v->dc_dir == dircl && v->dc_idx <= ci) { cl = v->dc_cl; start = v->dc_idx; }
    for (u32 i = start; i < ci; i++) {
        u32 nx; int r = f_fatget(v, cl, &nx); if (r) return r;
        if (nx >= 0x0FFFFFF8u) return 1;
        if (!f_ok_cl(v, nx)) return F32_ECORRUPT;
        cl = nx;
    }
    if (!f_ok_cl(v, cl)) return F32_ECORRUPT;
    v->dc_dir = dircl; v->dc_idx = ci; v->dc_cl = cl;
    u32 lba = f_lba(v, cl) + rem / 16;
    if (v->db_lba != lba) { if (blk_read(v->dev, lba, 1, v->db)) { v->db_lba = 0xFFFFFFFFu; return F32_EIO; } v->db_lba = lba; }
    *ent = v->db + (rem % 16) * 32; return 0;
}
static int f_dput(F32 *v) { return blk_write(v->dev, v->db_lba, 1, v->db); }

static char f_lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int f_ci_eq(const char *a, const char *b) { while (*a && *b) { if (f_lc(*a) != f_lc(*b)) return 0; a++; b++; } return *a == *b; }
static u8 f_cksum(const u8 *n11) { u8 s = 0; for (int i = 0; i < 11; i++) s = (u8)(((s & 1) ? 0x80 : 0) + (s >> 1) + n11[i]); return s; }
static const u8 f_lfn_off[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

/* next real entry in a directory (skips ".", "..", deleted entries and the volume label). 1 = produced, 0 = end, <0 = error */
static int f32_readdir(F32 *v, u32 dircl, u32 *pos, F32Ent *e) {
    char lfn[272]; int have = 0; u8 ck = 0; u32 first = 0; memset(lfn, 0, sizeof lfn);
    for (;;) {
        u8 *p; int r = f_dget(v, dircl, *pos, &p); if (r < 0) return r; if (r == 1) return 0;
        u32 cur = (*pos)++;
        if (p[0] == 0x00) { *pos = cur; return 0; }
        if (p[0] == 0xE5) { have = 0; continue; }
        if (p[11] == 0x0F) {
            u8 ord = p[0];
            if (ord & 0x40) { have = 1; ck = p[13]; first = cur; memset(lfn, 0, sizeof lfn); }
            else if (!have || p[13] != ck) { have = 0; continue; }
            int idx = (ord & 0x3F) - 1; if (idx < 0 || idx > 19) { have = 0; continue; }
            for (int k = 0; k < 13; k++) {
                u16 ch = f_r16(p + f_lfn_off[k]); if (ch == 0 || ch == 0xFFFF) break;
                int at = idx * 13 + k; if (at < 255) lfn[at] = ch < 128 ? (char)ch : '?';
            }
            continue;
        }
        if (p[11] & 0x08) { have = 0; continue; }
        if (p[0] == '.' ) { have = 0; continue; }
        int n = 0;
        if (have && f_cksum(p) == ck && lfn[0]) { for (; lfn[n] && n < 255; n++) e->name[n] = lfn[n]; }
        else {
            for (int i = 0; i < 8 && p[i] != ' '; i++) { char c = (char)p[i]; if (i == 0 && p[0] == 0x05) c = (char)0xE5; e->name[n++] = (p[12] & 0x08) ? f_lc(c) : c; }
            if (p[8] != ' ') { e->name[n++] = '.'; for (int i = 8; i < 11 && p[i] != ' '; i++) e->name[n++] = (p[12] & 0x10) ? f_lc((char)p[i]) : (char)p[i]; }
        }
        e->name[n] = 0; e->attr = p[11]; e->cl = ((u32)f_r16(p + 20) << 16) | f_r16(p + 26); e->size = f_r32(p + 28);
        e->first = have ? first : cur; e->ent = cur; e->dircl = dircl; return 1;
    }
}
static int f32_find(F32 *v, u32 dircl, const char *name, F32Ent *e) {
    u32 pos = 0; int r;
    while ((r = f32_readdir(v, dircl, &pos, e)) == 1) if (f_ci_eq(e->name, name)) return 0;
    return r < 0 ? r : F32_ENOENT;
}

/* ------------------------------------------------------------ paths */
static int f_name_ok(const char *n) {
    int l = 0; for (; n[l]; l++) { unsigned char c = (unsigned char)n[l]; if (c < 32 || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0; }
    if (l < 1 || l > 255) return 0; if (n[l - 1] == ' ' || n[l - 1] == '.') return 0;
    if (f_ci_eq(n, ".") || f_ci_eq(n, "..")) return 0; return 1;
}
/* split "/a/b/leaf": walks every component but the last. Returns the directory cluster and fills leaf (empty for "/"). */
static int f_parent(F32 *v, const char *path, u32 *dircl, char *leaf) {
    u32 cur = v->root; F32Ent e; char seg[256];
    while (*path == '/' || *path == '\\') path++;
    leaf[0] = 0;
    while (*path) {
        int n = 0; while (*path && *path != '/' && *path != '\\') { if (n < 255) seg[n++] = *path; path++; } seg[n] = 0;
        while (*path == '/' || *path == '\\') path++;
        if (!*path) { for (int i = 0; i <= n; i++) leaf[i] = seg[i]; break; }
        int r = f32_find(v, cur, seg, &e); if (r) return r; if (!(e.attr & 0x10)) return F32_ENOTDIR; cur = e.cl ? e.cl : v->root;
    }
    *dircl = cur; return 0;
}
static int f32_resolve(F32 *v, const char *path, F32Ent *e) {
    u32 dc; char leaf[256]; int r = f_parent(v, path, &dc, leaf); if (r) return r;
    if (!leaf[0]) { memset(e, 0, sizeof *e); e->attr = 0x10; e->cl = v->root; e->dircl = v->root; return 0; }
    return f32_find(v, dc, leaf, e);
}

/* ------------------------------------------------------------ reading */
static int f32_read(F32 *v, const F32Ent *e, u32 off, void *buf, u32 len) {
    if (e->attr & 0x10) return F32_EISDIR; if (off >= e->size) return 0; if (len > e->size - off) len = e->size - off;
    u32 csz = v->spc * 512, cl = e->cl, got = 0; u8 *out = (u8 *)buf, tmp[512];
    for (u32 i = 0; i < off / csz; i++) { u32 nx; int r = f_fatget(v, cl, &nx); if (r) return r; if (!f_ok_cl(v, nx)) return F32_ECORRUPT; cl = nx; }
    u32 in = off % csz;
    while (got < len) {
        if (!f_ok_cl(v, cl)) return F32_ECORRUPT;
        for (u32 s = in / 512; s < v->spc && got < len; s++) {
            u32 so = (s == in / 512) ? in % 512 : 0, take = 512 - so; if (take > len - got) take = len - got;
            if (so == 0 && take == 512) { if (blk_read(v->dev, f_lba(v, cl) + s, 1, out + got)) return F32_EIO; }
            else { if (blk_read(v->dev, f_lba(v, cl) + s, 1, tmp)) return F32_EIO; memcpy(out + got, tmp + so, take); }
            got += take;
        }
        in = 0; if (got >= len) break;
        u32 nx; int r = f_fatget(v, cl, &nx); if (r) return r; if (nx >= 0x0FFFFFF8u) break; cl = nx;
    }
    return (int)got;
}

/* ------------------------------------------------------------ writing */
static int f_writechain(F32 *v, const u8 *data, u32 len, u32 *first) {
    *first = 0; if (len == 0) return 0;
    u32 csz = v->spc * 512, prev = 0, done = 0; u8 tmp[512];
    while (done < len) {
        u32 cl = 0; int r = f_alloc(v, &cl);
        if (!r && prev) r = f_fatset(v, prev, cl);
        if (r) { if (*first) f_freechain(v, *first); *first = 0; return r; }
        if (!prev) *first = cl;
        for (u32 s = 0; s < v->spc && done + s * 512 < len; s++) {
            u32 rem = len - (done + s * 512); const u8 *src = data + done + s * 512;
            if (rem < 512) { memset(tmp, 0, 512); memcpy(tmp, src, rem); src = tmp; }
            if ((r = blk_write(v->dev, f_lba(v, cl) + s, 1, src))) { f_freechain(v, *first); *first = 0; return r; }
        }
        prev = cl; done += csz;
    }
    return 0;
}
static int f_dirextend(F32 *v, u32 dircl) {
    u32 cl = dircl, nx, fresh; int r;
    for (u32 g = 0; g < v->nclus; g++) { if ((r = f_fatget(v, cl, &nx))) return r; if (nx >= 0x0FFFFFF8u) break; if (!f_ok_cl(v, nx)) return F32_ECORRUPT; cl = nx; }
    if ((r = f_alloc(v, &fresh))) return r;
    if ((r = f_zero_cluster(v, fresh))) return r;
    return f_fatset(v, cl, fresh);
}
static int f_valid83_upper(const char *n, u8 *out11) {      /* true if the name fits 8.3 with no lowercase letters */
    int l = 0; while (n[l]) l++;
    int dot = -1; for (int i = 0; i < l; i++) if (n[i] == '.') { if (dot >= 0) return 0; dot = i; }
    int bl = dot < 0 ? l : dot, el = dot < 0 ? 0 : l - dot - 1; if (bl < 1 || bl > 8 || el > 3) return 0;
    memset(out11, ' ', 11);
    for (int i = 0; i < l; i++) {
        if (i == dot) continue; char c = n[i];
        if (c >= 'a' && c <= 'z') return 0;
        if (c == ' ' || c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']' || (unsigned char)c > 126) return 0;
        if (dot < 0 || i < dot) out11[i] = (u8)c; else out11[8 + i - dot - 1] = (u8)c;
    }
    return 1;
}
static int f_short_taken(F32 *v, u32 dircl, const u8 *s11) {
    u32 no = 0;
    for (;; no++) { u8 *p; int r = f_dget(v, dircl, no, &p); if (r < 0) return r; if (r == 1 || p[0] == 0x00) return 0; if (p[0] != 0xE5 && p[11] != 0x0F) { int same = 1; for (int i = 0; i < 11; i++) if (p[i] != s11[i]) same = 0; if (same) return 1; } }
}
static int f_make_short(F32 *v, u32 dircl, const char *name, u8 *s11) {
    int l = 0; while (name[l]) l++; int dot = -1; for (int i = 0; i < l; i++) if (name[i] == '.') dot = i;
    u8 base[8]; int bn = 0, en = 0; u8 ext[3];
    for (int i = 0; i < (dot < 0 ? l : dot) && bn < 6; i++) { char c = name[i]; if (c == ' ' || c == '.') continue; if (c >= 'a' && c <= 'z') c = (char)(c - 32); if (c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']' || (unsigned char)c > 126) c = '_'; base[bn++] = (u8)c; }
    if (!bn) base[bn++] = '_';
    if (dot >= 0) for (int i = dot + 1; i < l && en < 3; i++) { char c = name[i]; if (c == ' ') continue; if (c >= 'a' && c <= 'z') c = (char)(c - 32); if (c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']' || (unsigned char)c > 126) c = '_'; ext[en++] = (u8)c; }
    for (int n = 1; n < 1000000; n++) {
        char num[8]; int k = 0, t = n; char rev[8]; int rn = 0; while (t) { rev[rn++] = (char)('0' + t % 10); t /= 10; } num[k++] = '~'; while (rn) num[k++] = rev[--rn];
        memset(s11, ' ', 11); int bl = bn; if (bl + k > 8) bl = 8 - k;
        for (int i = 0; i < bl; i++) s11[i] = base[i]; for (int i = 0; i < k; i++) s11[bl + i] = (u8)num[i]; for (int i = 0; i < en; i++) s11[8 + i] = ext[i];
        int r = f_short_taken(v, dircl, s11); if (r < 0) return r; if (!r) return 0;
    }
    return F32_ENOSPC;
}
static void f_fill_short(u8 *p, const u8 *s11, u8 attr, u32 cl, u32 size) {
    memset(p, 0, 32); memcpy(p, s11, 11); p[11] = attr;
    f_w16(p + 14, f32_time); f_w16(p + 16, f32_date); f_w16(p + 18, f32_date); f_w16(p + 22, f32_time); f_w16(p + 24, f32_date);
    f_w16(p + 20, (u16)(cl >> 16)); f_w16(p + 26, (u16)cl); f_w32(p + 28, size);
}
static int f_mkentries(F32 *v, u32 dircl, const char *name, u8 attr, u32 cl, u32 size) {
    u8 s11[11]; int nlen = 0; while (name[nlen]) nlen++;
    int lfn = !f_valid83_upper(name, s11), nl = lfn ? (nlen + 12) / 13 : 0, need = nl + 1, r;
    if (lfn && (r = f_make_short(v, dircl, name, s11))) return r;
    u32 run_start = 0, run = 0, no = 0;
    for (;; no++) {
        u8 *p; r = f_dget(v, dircl, no, &p); if (r < 0) return r;
        if (r == 1) { if ((r = f_dirextend(v, dircl))) return r; no--; continue; }
        if (p[0] == 0x00 || p[0] == 0xE5) { if (!run) run_start = no; if (++run >= (u32)need) break; } else run = 0;
    }
    u8 ck = f_cksum(s11);
    for (int i = 0; i < need; i++) {
        u8 *p; if ((r = f_dget(v, dircl, run_start + (u32)i, &p)) != 0) return r < 0 ? r : F32_ECORRUPT;
        if (i < nl) {
            int ord = nl - i; memset(p, 0, 32); p[0] = (u8)(ord | (i == 0 ? 0x40 : 0)); p[11] = 0x0F; p[13] = ck;
            for (int k = 0; k < 13; k++) {
                int at = (ord - 1) * 13 + k; u16 ch = at < nlen ? (u16)(u8)name[at] : (at == nlen ? 0 : 0xFFFF);
                f_w16(p + f_lfn_off[k], ch);
            }
        } else f_fill_short(p, s11, attr, cl, size);
        if ((r = f_dput(v))) return r;
    }
    return 0;
}

static int f32_write_file(F32 *v, const char *path, const void *data, u32 len) {
    if (v->ro) return F32_EROFS;
    u32 dc; char leaf[256]; int r = f_parent(v, path, &dc, leaf); if (r) return r;
    if (!leaf[0] || !f_name_ok(leaf)) return F32_ENAME;
    F32Ent e; int ex = f32_find(v, dc, leaf, &e); if (ex < 0 && ex != F32_ENOENT) return ex;
    if (ex == 0 && (e.attr & 0x10)) return F32_EISDIR;
    u32 first; if ((r = f_writechain(v, (const u8 *)data, len, &first))) return r;
    if (ex == 0) {
        u8 *p; if ((r = f_dget(v, dc, e.ent, &p)) != 0) { if (first) f_freechain(v, first); return r < 0 ? r : F32_ECORRUPT; }
        f_w16(p + 20, (u16)(first >> 16)); f_w16(p + 26, (u16)first); f_w32(p + 28, len); f_w16(p + 22, f32_time); f_w16(p + 24, f32_date); f_w16(p + 18, f32_date);
        if ((r = f_dput(v))) return r;
        if (e.cl) f_freechain(v, e.cl);                    /* release the OLD data only after the entry points at the new data */
        return 0;
    }
    if ((r = f_mkentries(v, dc, leaf, 0x20, first, len))) { if (first) f_freechain(v, first); return r; }
    return 0;
}
static int f32_mkdir(F32 *v, const char *path) {
    if (v->ro) return F32_EROFS;
    u32 dc; char leaf[256]; int r = f_parent(v, path, &dc, leaf); if (r) return r;
    if (!leaf[0] || !f_name_ok(leaf)) return F32_ENAME;
    F32Ent e; r = f32_find(v, dc, leaf, &e); if (r == 0) return F32_EEXIST; if (r != F32_ENOENT) return r;
    u32 cl; if ((r = f_alloc(v, &cl))) return r;
    if ((r = f_zero_cluster(v, cl))) { f_freechain(v, cl); return r; }
    u8 sec[512]; memset(sec, 0, 512); u8 dot[11], dd[11]; memset(dot, ' ', 11); memset(dd, ' ', 11); dot[0] = '.'; dd[0] = '.'; dd[1] = '.';
    f_fill_short(sec, dot, 0x10, cl, 0); f_fill_short(sec + 32, dd, 0x10, dc == v->root ? 0 : dc, 0);
    if ((r = blk_write(v->dev, f_lba(v, cl), 1, sec))) { f_freechain(v, cl); return r; }
    f_invalidate(v);
    if ((r = f_mkentries(v, dc, leaf, 0x10, cl, 0))) { f_freechain(v, cl); return r; }
    return 0;
}
static int f32_remove(F32 *v, const char *path) {
    if (v->ro) return F32_EROFS;
    F32Ent e; int r = f32_resolve(v, path, &e); if (r) return r;
    if (e.cl == v->root && !e.name[0]) return F32_ENAME;
    if (e.attr & 0x10) { F32Ent t; u32 pos = 0; r = f32_readdir(v, e.cl, &pos, &t); if (r < 0) return r; if (r == 1) return F32_ENOTEMPTY; }
    for (u32 no = e.first; no <= e.ent; no++) { u8 *p; r = f_dget(v, e.dircl, no, &p); if (r != 0) return r < 0 ? r : F32_ECORRUPT; p[0] = 0xE5; if ((r = f_dput(v))) return r; }
    if (e.cl) f_freechain(v, e.cl);                           /* entries are gone first, so a power cut can only leak clusters */
    f_invalidate(v); return 0;
}
static u32 f32_free_clusters(F32 *v) {
    if (v->free_cnt == 0xFFFFFFFFu) { u32 c = 0; for (u32 cl = 2; cl < v->nclus + 2; cl++) { u32 x; if (f_fatget(v, cl, &x)) break; if (!x) c++; } v->free_cnt = c; }
    return v->free_cnt;
}

/* ------------------------------------------------------------ mount / unmount / format */
static int f32_mount(F32 *v, BlockDev *dev, u32 base, u32 total) {
    u8 b[512]; memset(v, 0, sizeof *v); v->fb_lba = v->db_lba = v->dc_dir = 0xFFFFFFFFu; v->free_cnt = 0xFFFFFFFFu;
    if (blk_read(dev, base, 1, b)) return F32_EIO;
    if (b[510] != 0x55 || b[511] != 0xAA || f_r16(b + 11) != 512) return F32_ECORRUPT;
    u32 spc = b[13], rsvd = f_r16(b + 14), nf = b[16], fsz = f_r32(b + 36), tot = f_r16(b + 19) ? f_r16(b + 19) : f_r32(b + 32);
    if (!spc || (spc & (spc - 1)) || spc > 128 || !rsvd || nf < 1 || nf > 2 || f_r16(b + 17) != 0 || f_r16(b + 22) != 0 || !fsz) return F32_ECORRUPT;
    if (total && tot > total) return F32_ECORRUPT; if (tot > dev->sectors - base) return F32_ECORRUPT;
    u32 meta = rsvd + nf * fsz; if (meta >= tot) return F32_ECORRUPT;
    u32 ncl = (tot - meta) / spc; if (ncl < 65525 || ncl > 0x0FFFFFF0u) return F32_ECORRUPT;   /* below 65525 clusters it is not FAT32 */
    if (fsz < (ncl + 2 + 127) / 128) return F32_ECORRUPT;
    v->dev = dev; v->base = base; v->total = tot; v->spc = spc; v->rsvd = rsvd; v->nfats = nf; v->fatsz = fsz;
    v->fat0 = base + rsvd; v->data0 = base + meta; v->nclus = ncl; v->root = f_r32(b + 44); v->hint = 2; v->fsinfo = f_r16(b + 48);
    if (!f_ok_cl(v, v->root)) return F32_ECORRUPT;
    memcpy(v->label, b + 71, 11); v->label[11] = 0; for (int i = 10; i >= 0 && v->label[i] == ' '; i--) v->label[i] = 0;
    int lab = 1; for (int i = 0; i < 11; i++) { char want = F32_DATA_LABEL[i]; char got = (char)b[71 + i]; if (got != want) lab = 0; }
    v->ro = dev->readonly || !lab;
    u32 f1; if (f_fatget(v, 1, &f1)) return F32_EIO;
    v->was_dirty = !(f1 & 0x08000000u);
    if (v->fsinfo && v->fsinfo < rsvd) { u8 fi[512]; if (!blk_read(dev, base + v->fsinfo, 1, fi) && f_r32(fi) == 0x41615252u && f_r32(fi + 484) == 0x61417272u) { u32 fc = f_r32(fi + 488); if (fc <= ncl) v->free_cnt = fc; u32 nx = f_r32(fi + 492); if (f_ok_cl(v, nx)) v->hint = nx; } }
    if (!v->ro) { u32 old; f_fatget(v, 1, &old); u8 *q = v->fb + (1 % 128) * 4; f_w32(q, f_r32(q) & ~0x08000000u); for (u32 k = 0; k < nf; k++) if (blk_write(dev, v->fb_lba + k * fsz, 1, v->fb)) return F32_EIO; v->free_cnt = 0xFFFFFFFFu; }
    v->mounted = 1; return 0;
}
static int f32_unmount(F32 *v) {
    if (!v->mounted) return 0; int r = 0;
    if (!v->ro) {
        u32 old; if ((r = f_fatget(v, 1, &old))) return r;
        u8 *q = v->fb + (1 % 128) * 4; f_w32(q, f_r32(q) | 0x08000000u);
        for (u32 k = 0; k < v->nfats; k++) if ((r = blk_write(v->dev, v->fb_lba + k * v->fatsz, 1, v->fb))) return r;
        if (v->fsinfo && v->fsinfo < v->rsvd) { u8 fi[512]; if (!blk_read(v->dev, v->base + v->fsinfo, 1, fi)) { f_w32(fi + 488, v->free_cnt); f_w32(fi + 492, v->hint); blk_write(v->dev, v->base + v->fsinfo, 1, fi); } }
    }
    v->mounted = 0; return r;
}

static u32 f_pick_spc(u32 total) { return total <= 532480 ? 1 : (total <= 16777216 ? 8 : (total <= 33554432 ? 16 : (total <= 67108864 ? 32 : 64))); }
/* Format ONE partition range as FAT32 with the given 11-character label. Only sectors [base, base+total) are written. */
static int f32_format(BlockDev *dev, u32 base, u32 total, const char *label, u32 volid) {
    if (!dev || dev->readonly) return F32_EROFS;
    if (total < 66600 || base >= dev->sectors || total > dev->sectors - base) return F32_EIO;   /* too small for FAT32 or out of range */
    u32 spc = f_pick_spc(total), rsvd = 32, nf = 2, t1 = total - rsvd, t2 = (256 * spc + nf) / 2, fsz = (t1 + t2 - 1) / t2;
    static u8 z[4096]; u8 b[512]; int r; memset(z, 0, sizeof z); memset(b, 0, 512);
    b[0] = 0xEB; b[1] = 0x58; b[2] = 0x90; memcpy(b + 3, "PIXELBEE", 8); f_w16(b + 11, 512); b[13] = (u8)spc; f_w16(b + 14, (u16)rsvd); b[16] = (u8)nf;
    b[21] = 0xF8; f_w16(b + 24, 63); f_w16(b + 26, 255); f_w32(b + 28, base); f_w32(b + 32, total); f_w32(b + 36, fsz); f_w32(b + 44, 2); f_w16(b + 48, 1); f_w16(b + 50, 6);
    b[64] = 0x80; b[66] = 0x29; f_w32(b + 67, volid); memset(b + 71, ' ', 11); for (int i = 0; i < 11 && label[i]; i++) b[71 + i] = (u8)label[i]; memcpy(b + 82, "FAT32   ", 8); b[510] = 0x55; b[511] = 0xAA;
    u32 ncl = (total - rsvd - nf * fsz) / spc;
    for (u32 s = 0; s < rsvd; s += 8) { u32 n = rsvd - s < 8 ? rsvd - s : 8; if ((r = blk_write(dev, base + s, n, z))) return r; }       /* wipe the reserved area */
    u8 fi[512]; memset(fi, 0, 512); f_w32(fi, 0x41615252u); f_w32(fi + 484, 0x61417272u); f_w32(fi + 488, ncl - 1); f_w32(fi + 492, 3); f_w32(fi + 508, 0xAA550000u);
    if ((r = blk_write(dev, base + 1, 1, fi)) || (r = blk_write(dev, base + 7, 1, fi))) return r;
    if ((r = blk_write(dev, base + 6, 1, b)) || (r = blk_write(dev, base, 1, b))) return r;
    for (u32 k = 0; k < nf; k++) for (u32 s = 0; s < fsz; s += 8) { u32 n = fsz - s < 8 ? fsz - s : 8; if ((r = blk_write(dev, base + rsvd + k * fsz + s, n, z))) return r; }
    u8 fat0[512]; memset(fat0, 0, 512); f_w32(fat0, 0x0FFFFFF8u); f_w32(fat0 + 4, 0x0FFFFFFFu | 0x08000000u); f_w32(fat0 + 8, 0x0FFFFFFFu);   /* media, clean bit, root cluster = end of chain */
    for (u32 k = 0; k < nf; k++) if ((r = blk_write(dev, base + rsvd + k * fsz, 1, fat0))) return r;
    u32 root = base + rsvd + nf * fsz;
    for (u32 s = 0; s < spc; s++) if ((r = blk_write(dev, root + s, 1, z))) return r;
    return 0;
}
#endif
