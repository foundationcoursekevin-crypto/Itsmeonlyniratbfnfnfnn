/* ===================================================================
 * PixelBeeOS apps: Calendar (events + holidays) and BeeBrowser (offline pages,
 * BeeSearch, mini HTML renderer). Drawn directly onto the glass window.
 * =================================================================== */

static void glyph_s(u32 *buf, int x, int y, int c, u32 col, int sc) {
    if (sc <= 1) { glyph(buf, x, y, c, col); return; }
    if (c < 32 || c > 126) c = '?';
    const u8 *g = font[c - 32];
    for (int j = 0; j < 16; j++) for (int i = 0; i < 8; i++) {
        u8 a = g[j * 8 + i]; if (!a) continue;
        for (int sy = 0; sy < sc; sy++) for (int sx2 = 0; sx2 < sc; sx2++) {
            int xx = x + i * sc + sx2, yy = y + j * sc + sy; if (xx < 0 || yy < 0 || xx >= (int)W || yy >= (int)H) continue;
            u32 *p = &buf[yy * W + xx]; *p = mix(*p, col, a);
        }
    }
}
static void text_s(u32 *buf, int x, int y, const char *s, u32 col, int sc) { for (; *s; s++, x += 8 * sc) glyph_s(buf, x, y, *s, col, sc); }

/* ------------------------------ CALENDAR ------------------------------ */
typedef struct { u16 y; u8 m, d, used; char t[40]; } Ev;
static Ev evs[40];
static int ca_y, ca_m, ca_d, ca_in, ca_bl; static char ca_buf[40];
static int cal_cy[42], cal_cm[42], cal_cd[42];
static void exec_line(const char *s);
static const struct { u8 m, d; const char *n; } hol_[] = {{1, 1, "New Year's Day"}, {2, 14, "Valentine's Day"}, {3, 14, "Pi Day"}, {4, 1, "April Fools' Day"},
    {5, 1, "Labour Day"}, {7, 4, "Independence Day (US)"}, {10, 31, "Halloween"}, {12, 24, "Christmas Eve"}, {12, 25, "Christmas Day"}, {12, 31, "New Year's Eve"}};
static const char *hol_name(int m, int d) { for (u32 i = 0; i < sizeof hol_ / sizeof hol_[0]; i++) if (hol_[i].m == m && hol_[i].d == d) return hol_[i].n; return 0; }
static int ev_count(int y, int m, int d) { int c = 0; for (int i = 0; i < 40; i++) if (evs[i].used && evs[i].y == y && evs[i].m == m && evs[i].d == d) c++; return c; }
static void cal_norm(void) {
    while (ca_d < 1) { ca_m--; if (ca_m < 1) { ca_m = 12; ca_y--; } ca_d += dim_(ca_y, ca_m); }
    while (ca_d > dim_(ca_y, ca_m)) { ca_d -= dim_(ca_y, ca_m); ca_m++; if (ca_m > 12) { ca_m = 1; ca_y++; } }
    if (ca_y < 1970) { ca_y = 1970; ca_m = 1; ca_d = 1; } if (ca_y > 2999) { ca_y = 2999; ca_m = 12; ca_d = 31; }
}
static void app_open_cal(int m, int y) {
    int hh, mm, ss, dd, mo, yy; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy);
    ca_y = y ? y : yy; ca_m = m ? m : mo; ca_d = (ca_m == mo && ca_y == yy) ? dd : 1; ca_in = 0; app_on = 1; dirty = 1; notify("Calendar", 0);
}
static void cal_draw(u32 *b, u32 ms) {
    int hh, mm, ss, td, tm, ty; rtc_read(&hh, &mm, &ss, &td, &tm, &ty);
    int ax = wx + 16, ay = wy + 34 + 8, aw = ww - 32, ah = wh - 34 - 20;
    int cw = (aw * 64 / 100) / 7, ch = (ah - 118) / 6; if (ch > 64) ch = 64; if (ch < 22) ch = 22;
    char t[40]; int p = 0; for (const char *s = mon_[ca_m - 1]; *s; s++) t[p++] = *s; t[p++] = ' '; { char yb[8]; fmt_base((u32)ca_y, 10, yb); for (char *q = yb; *q; q++) t[p++] = *q; } t[p] = 0;
    text_s(b, ax, ay, t, pal[A_ACC], 2);
    { int bx = ax + cw * 7 - 76;
      draw_rr(b, bx, ay + 4, 30, 30, 15, 0xFFFFFF, hmix(26, 70, hov(bx, ay + 4, 30, 30))); text(b, bx + 11, ay + 11, "<", pal[A_TXT]); hit_add(bx, ay + 4, 30, 30, 20);
      draw_rr(b, bx + 38, ay + 4, 30, 30, 15, 0xFFFFFF, hmix(26, 70, hov(bx + 38, ay + 4, 30, 30))); text(b, bx + 49, ay + 11, ">", pal[A_TXT]); hit_add(bx + 38, ay + 4, 30, 30, 21); }
    static const char *wk[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for (int i = 0; i < 7; i++) text(b, ax + i * cw + cw / 2 - 8, ay + 44, wk[i], i == 0 ? 0xFF9FB0 : pal[A_DIM]);
    int y0 = ay + 66, first = dow_(ca_y, ca_m, 1), pm = ca_m == 1 ? 12 : ca_m - 1, py = ca_m == 1 ? ca_y - 1 : ca_y, pd = dim_(py, pm), nd_ = dim_(ca_y, ca_m);
    for (int cell = 0; cell < 42; cell++) {
        int d = cell - first + 1, cy2 = ca_y, cm2 = ca_m, in = 1;
        if (d < 1) { d += pd; cm2 = pm; cy2 = py; in = 0; } else if (d > nd_) { d -= nd_; cm2 = ca_m == 12 ? 1 : ca_m + 1; cy2 = ca_m == 12 ? ca_y + 1 : ca_y; in = 0; }
        int gx = ax + (cell % 7) * cw, gy = y0 + (cell / 7) * ch;
        int istd = (cy2 == ty && cm2 == tm && d == td), sel = in && d == ca_d;
        draw_rr(b, gx + 2, gy + 2, cw - 4, ch - 4, 9, 0xFFFFFF, in ? 16 : 5);
        cal_cy[cell] = cy2; cal_cm[cell] = cm2; cal_cd[cell] = d; hit_add(gx, gy, cw, ch, 100 + cell);
        if (hot(gx, gy, cw, ch)) draw_rr(b, gx + 2, gy + 2, cw - 4, ch - 4, 9, pal[A_ACC], 38);
        if (istd) draw_rr(b, gx + 2, gy + 2, cw - 4, ch - 4, 9, 0xFFB454, 60);
        if (sel) { draw_rr(b, gx + 1, gy + 1, cw - 2, ch - 2, 10, pal[A_ACC], 120); draw_rr(b, gx + 3, gy + 3, cw - 6, ch - 6, 8, 0x0A1E36, 150); }
        char nb[3] = {d >= 10 ? (char)('0' + d / 10) : ' ', (char)('0' + d % 10), 0};
        u32 col = !in ? 0x5E7790 : (cell % 7 == 0 ? 0xFF9FB0 : pal[A_TXT]); if (istd) col = 0xFFD58A;
        text(b, gx + 9, gy + 6, nb, col);
        int ne = ev_count(cy2, cm2, d), hl = hol_name(cm2, d) != 0, dx = gx + 10;
        for (int k = 0; k < ne && k < 3; k++, dx += 10) draw_rr(b, dx, gy + ch - 14, 7, 7, 3, pal[A_ACC], 255);
        if (hl) draw_rr(b, dx, gy + ch - 14, 7, 7, 3, 0xFFB454, 255);
    }
    /* side panel */
    int sx = ax + cw * 7 + 22, sw = aw - (cw * 7 + 22), sy = y0;
    draw_rr(b, sx, sy, sw, ch * 6 - 4, 12, 0xFFFFFF, 12);
    char hd[48]; int hp = 0; for (const char *s = wd_[dow_(ca_y, ca_m, ca_d)]; *s; s++) hd[hp++] = *s; hd[hp] = 0;
    text(b, sx + 14, sy + 12, hd, pal[A_ACC]);
    { char ln[40]; int q = 0; char nb[4]; fmt_base((u32)ca_d, 10, nb); for (char *s = nb; *s; s++) ln[q++] = *s; ln[q++] = ' '; for (const char *s = mon_[ca_m - 1]; *s; s++) ln[q++] = *s; ln[q++] = ' '; { char yb[8]; fmt_base((u32)ca_y, 10, yb); for (char *s = yb; *s; s++) ln[q++] = *s; } ln[q] = 0; text(b, sx + 14, sy + 32, ln, pal[A_TXT]); }
    int diff = (int)(days_(ca_y, ca_m, ca_d) - days_(ty, tm, td)); char rel[32]; int rp = 0;
    if (diff == 0) { for (const char *s = "Today"; *s; s++) rel[rp++] = *s; }
    else { u32 ad = (u32)(diff < 0 ? -diff : diff); char nb[12]; fmt_base(ad, 10, nb); if (diff > 0) { for (const char *s = "In "; *s; s++) rel[rp++] = *s; } for (char *s = nb; *s; s++) rel[rp++] = *s; for (const char *s = ad == 1 ? " day" : " days"; *s; s++) rel[rp++] = *s; if (diff < 0) for (const char *s = " ago"; *s; s++) rel[rp++] = *s; }
    rel[rp] = 0; text(b, sx + 14, sy + 52, rel, pal[A_DIM]);
    int ly = sy + 84; const char *hn = hol_name(ca_m, ca_d);
    if (hn) { draw_rr(b, sx + 14, ly + 5, 7, 7, 3, 0xFFB454, 255); text(b, sx + 28, ly, hn, 0xFFD58A); ly += 22; }
    int shown = 0;
    for (int i = 0; i < 40; i++) if (evs[i].used && evs[i].y == ca_y && evs[i].m == ca_m && evs[i].d == ca_d && ly < sy + ch * 6 - 24) {
        draw_rr(b, sx + 14, ly + 5, 7, 7, 3, pal[A_ACC], 255); char tx[28]; scpy(tx, evs[i].t, (sw - 44) / 8 < 27 ? (sw - 44) / 8 : 27); text(b, sx + 28, ly, tx, pal[A_TXT]); ly += 22; shown++;
    }
    if (!shown && !hn) text(b, sx + 14, ly, "No events", pal[A_DIM]);
    if (!shown) text(b, sx + 14, ly + (hn ? 0 : 22), "Press E to add one", pal[A_DIM]);
    int fy = ay + ah - 18;
    if (ca_in) {
        draw_rr(b, ax, fy - 30, aw, 30, 15, 0xFFFFFF, 30); text(b, ax + 14, fy - 24, "New event:", pal[A_ACC]); text(b, ax + 14 + 88, fy - 24, ca_buf, pal[A_TXT]);
        if ((ms / 500) % 2 == 0) draw_rr(b, ax + 14 + 88 + ca_bl * 8, fy - 23, 8, 14, 2, pal[A_ACC], 210);
    }
}
static void cal_key(int k) {
    if (ca_in) {
        if (k == 27) ca_in = 0;
        else if (k == '\n') {
            if (ca_bl) { int s = -1; for (int i = 0; i < 40; i++) if (!evs[i].used) { s = i; break; }
                if (s < 0) notify("Event list full", 2); else { evs[s].used = 1; evs[s].y = (u16)ca_y; evs[s].m = (u8)ca_m; evs[s].d = (u8)ca_d; scpy(evs[s].t, ca_buf, 40); notify("Event added", 1); } }
            ca_in = 0;
        } else if (k == '\b') { if (ca_bl) ca_buf[--ca_bl] = 0; }
        else if (k >= 32 && k < 127 && ca_bl < 38) { ca_buf[ca_bl++] = (char)k; ca_buf[ca_bl] = 0; }
        return;
    }
    int hh, mm, ss, dd, mo, yy;
    if (k == 27) { app_on = 0; return; }
    else if (k == KEY_LEFT) ca_d--; else if (k == KEY_RIGHT) ca_d++; else if (k == KEY_UP) ca_d -= 7; else if (k == KEY_DOWN) ca_d += 7;
    else if (k == '[' || k == ',') { ca_m--; if (ca_m < 1) { ca_m = 12; ca_y--; } }
    else if (k == ']' || k == '.') { ca_m++; if (ca_m > 12) { ca_m = 1; ca_y++; } }
    else if (k == '{') ca_y--; else if (k == '}') ca_y++;
    else if (k == 't' || k == 'T') { rtc_read(&hh, &mm, &ss, &dd, &mo, &yy); ca_y = yy; ca_m = mo; ca_d = dd; }
    else if (k == 'e' || k == 'E') { ca_in = 1; ca_bl = 0; ca_buf[0] = 0; return; }
    else if (k == 'x' || k == 'X') { int c = 0; for (int i = 0; i < 40; i++) if (evs[i].used && evs[i].y == ca_y && evs[i].m == ca_m && evs[i].d == ca_d) { evs[i].used = 0; c++; } notify(c ? "Events removed" : "Nothing to remove", c ? 1 : 0); return; }
    else return;
    int mx = dim_(ca_y, ca_m); if (ca_d > mx && (k == '[' || k == ']' || k == ',' || k == '.' || k == '{' || k == '}')) ca_d = mx; cal_norm();
}

/* ------------------------------ BROWSER ------------------------------ */
#define BL_N 160
typedef struct { char t[112]; u8 kind, link; } BLine;     /* kinds: 0 text 1 h1 2 h2 3 link 4 bullet 5 rule 6 dim 7 logo 8 searchbox 9 indent */
static BLine bl[BL_N]; static int bn, bscroll, bsel, bnl, bfocus, bcols;
static char blu[24][96], burl[96], bedit[96]; static int bedl;
static char bh[16][96]; static int bhn, bhi = -1;
static void br_go(const char *url, int push);

static void bl_add(u8 kind, const char *s, int link) { if (bn >= BL_N) return; scpy(bl[bn].t, s, 112); bl[bn].kind = kind; bl[bn].link = (u8)(link < 0 ? 255 : link); bn++; }
static int bl_link(const char *url) { if (bnl >= 24) return -1; scpy(blu[bnl], url, 96); return bnl++; }
static void bl_wrap(u8 kind, const char *s) {
    int w = bcols - (kind == 4 ? 3 : 0), first = 1;
    while (*s) {
        while (*s == ' ') s++; if (!*s) break;
        int n = slen(s), cut = n;
        if (n > w) { cut = w; while (cut > 0 && s[cut] != ' ') cut--; if (cut == 0) cut = w; }
        char tmp[112]; int c = cut > 111 ? 111 : cut; memcpy(tmp, s, (unsigned)c); tmp[c] = 0;
        bl_add(first ? kind : (kind == 4 ? 9 : kind), tmp, -1); first = 0; s += cut;
    }
}
static void bl_linkline(const char *title, const char *url) { bl_add(3, title, bl_link(url)); }
static int starts_ci(const char *s, const char *p) { while (*p) { if (lw(*s) != lw(*p)) return 0; s++; p++; } return 1; }
static int buf_has(const char *h, int n, const char *nee) { int nl = slen(nee); if (!nl) return 1; for (int s = 0; s + nl <= n; s++) { int m = 1; for (int j = 0; j < nl; j++) if (lw(h[s + j]) != lw(nee[j])) { m = 0; break; } if (m) return 1; } return 0; }

static void pg_home(void) {
    bl_add(7, "", -1); bl_add(8, "", -1); bl_add(6, "Offline search: PixelBeeOS commands, your files and built-in pages.", -1); bl_add(6, "Type in the box above and press Enter.", -1); bl_add(0, "", -1);
    bl_linkline("Glacier facts", "vin://glacier"); bl_linkline("Every PixelBeeOS command", "vin://commands"); bl_linkline("Open the Calendar app", "vin://app/calendar");
    bl_linkline("How to use BeeBrowser", "vin://help"); bl_linkline("About BeeBrowser", "vin://about");
}
static void pg_about(void) {
    bl_add(1, "BeeBrowser 0.1", -1);
    bl_wrap(0, "A tiny web-style browser built into PixelBeeOS. It shows built-in pages, your own .htm and .txt files from the RAM disk, and BeeSearch results.");
    bl_wrap(0, "It cannot load real websites yet, because PixelBeeOS has no network stack. See the page that opens when you type a site like google.com.");
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void pg_help(void) {
    bl_add(1, "Using BeeBrowser", -1); bl_add(2, "Address bar", -1);
    bl_wrap(4, "Type words to search, or a page like vin://glacier, or the name of an .htm or .txt file, then press Enter.");
    bl_add(2, "Page keys", -1); bl_wrap(4, "Up and Down scroll. N and P select the next or previous link. Enter opens it. Numbers 1 to 9 open a link directly.");
    bl_wrap(4, "B goes back, F forward, H home, Tab or / jumps to the address bar, Esc leaves the browser.");
    bl_add(2, "Make your own pages", -1); bl_wrap(4, "In the terminal run: make file hello.htm  then type  <h1>Hello</h1><p>My page</p>  and press Esc to save. Open it with: browser hello.htm");
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void pg_glacier(void) {
    bl_add(1, "Glacier facts", -1);
    bl_wrap(4, "Glaciers hold about 69 percent of the world's fresh water."); bl_wrap(4, "Glacier ice looks blue because it absorbs red light and scatters blue light.");
    bl_wrap(4, "A glacier flows slowly under its own weight, from centimeters to meters per day."); bl_wrap(4, "Crevasses are deep cracks that open where the ice bends or stretches.");
    bl_wrap(4, "Antarctica's ice sheet holds roughly 90 percent of all the ice on Earth."); bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void pg_commands(void) {
    static const char *grp[] = {"Core", "Math", "Convert", "Units", "Time", "Text", "Fun", "System", "Files"}; static const u8 gc[] = {C_LEG, C_M1, C_BASE, C_UNIT, C_TIME, C_TEXT, C_FUN, C_SYS, C_FILE};
    bl_add(1, "PixelBeeOS commands", -1);
    for (int g = 0; g < 9; g++) {
        char buf[700]; int p = 0; bl_add(2, grp[g], -1);
        for (int i = 0; i < NCMDS; i++) if (cmds[i].cat == gc[g] || (g == 1 && cmds[i].cat == C_M2)) { int L = slen(cmds[i].name); if (p + L + 2 < 690) { memcpy(buf + p, cmds[i].name, (unsigned)L); p += L; buf[p++] = ' '; } }
        buf[p] = 0; bl_wrap(0, buf);
    }
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void pg_offline(const char *url) {
    bl_add(1, "No network connection", -1); char l[112] = "You asked for: "; int p = slen(l); for (int i = 0; url[i] && p < 108; i++) l[p++] = url[i]; l[p] = 0; bl_add(6, l, -1);
    bl_wrap(0, "PixelBeeOS could not find a supported network card, or could not get an address from your router.");
    bl_wrap(4, "Supported cards: Intel e1000 and Realtek RTL8139 (QEMU: -nic user,model=e1000)"); bl_wrap(4, "Open the Terminal and run: net up"); bl_wrap(4, "https:// works too (TLS 1.3 sites), but the site identity is not verified yet.");
    bl_add(0, "", -1); bl_wrap(0, "BeeSearch still works offline on your commands, files and PixelBeeOS pages.");
    bl_linkline("Go to BeeSearch", "vin://home");
}
static void pg_missing(const char *url) { bl_add(1, "Page not found", -1); char l[112] = "Nothing at: "; int p = slen(l); for (int i = 0; url[i] && p < 108; i++) l[p++] = url[i]; l[p] = 0; bl_add(6, l, -1); bl_add(0, "", -1); bl_linkline("Go to BeeSearch", "vin://home"); }
static void pg_search(const char *q) {
    char h[112] = "Results for: "; int p = slen(h); for (int i = 0; q[i] && p < 108; i++) h[p++] = q[i]; h[p] = 0; bl_add(1, h, -1);
    bl_add(6, "Offline search: commands, files and pages (not the internet).", -1);
    int c = 0; bl_add(2, "Commands", -1);
    for (int i = 0; i < NCMDS && c < 10; i++) if (cmds[i].cat != C_ALI && (ci_has(cmds[i].name, q) || ci_has(cmds[i].desc, q))) { bl_add(0, cmds[i].desc, -1); c++; }
    if (!c) bl_add(6, "none", -1);
    int f = 0; bl_add(2, "Files in the RAM disk", -1);
    for (int k = 1; k < NODES && f < 8; k++) if (nd[k].used && !nd[k].isdir && (ci_has(nd[k].name, q) || buf_has(fdata[k], nd[k].size, q))) { char path[200]; node_path(k, path); char url[100] = "file:"; scpy(url + 5, path, 90); bl_linkline(path, url); f++; }
    if (!f) bl_add(6, "none", -1);
    static const struct { const char *t, *u; } pgs[] = {{"Glacier facts", "vin://glacier"}, {"Every PixelBeeOS command", "vin://commands"}, {"How to use BeeBrowser", "vin://help"}, {"About BeeBrowser", "vin://about"}};
    int g = 0; bl_add(2, "Pages", -1); for (int i = 0; i < 4; i++) if (ci_has(pgs[i].t, q)) { bl_linkline(pgs[i].t, pgs[i].u); g++; } if (!g) bl_add(6, "none", -1);
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void html_render(const char *s, int n) {
    char txt[400]; int tl_ = 0; u8 kind = 0; int skip = 0, ina = 0; char href[96] = "";
#define FLUSH() do { if (tl_) { txt[tl_] = 0; bl_wrap(kind, txt); tl_ = 0; } } while (0)
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '<') {
            int j = i + 1; char tag[16]; int tp = 0; int closing = 0; if (j < n && s[j] == '/') { closing = 1; j++; }
            while (j < n && s[j] != '>' && s[j] != ' ' && tp < 15) tag[tp++] = lw(s[j++]); tag[tp] = 0;
            int e = j; while (e < n && s[e] != '>') e++;
            if (!closing && ci_eq(tag, "a")) { FLUSH(); ina = 1; href[0] = 0; for (int k = j; k + 5 < e; k++) if (lw(s[k]) == 'h' && lw(s[k + 1]) == 'r' && lw(s[k + 2]) == 'e' && lw(s[k + 3]) == 'f' && s[k + 4] == '=') { int q = k + 5; char qc = s[q] == '"' || s[q] == '\'' ? s[q++] : ' '; int hp = 0; while (q < e && s[q] != qc && hp < 95) href[hp++] = s[q++]; href[hp] = 0; break; } }
            else if (closing && ci_eq(tag, "a")) { txt[tl_] = 0; if (tl_) { bl_add(3, txt, bl_link(href)); tl_ = 0; } ina = 0; }
            else if (ci_eq(tag, "title") || ci_eq(tag, "script") || ci_eq(tag, "style")) skip = !closing;
            else if (ci_eq(tag, "h1")) { FLUSH(); kind = closing ? 0 : 1; } else if (ci_eq(tag, "h2") || ci_eq(tag, "h3")) { FLUSH(); kind = closing ? 0 : 2; }
            else if (ci_eq(tag, "li")) { FLUSH(); kind = closing ? 0 : 4; } else if (ci_eq(tag, "hr")) { FLUSH(); bl_add(5, "", -1); }
            else if (ci_eq(tag, "p") || ci_eq(tag, "br") || ci_eq(tag, "div") || ci_eq(tag, "ul") || ci_eq(tag, "ol")) { FLUSH(); if (closing && ci_eq(tag, "p")) bl_add(0, "", -1); }
            i = e; continue;
        }
        if (skip) continue;
        if (c == '&') { if (i + 3 < n && s[i + 1] == 'l' && s[i + 2] == 't') { c = '<'; i += 3; } else if (i + 3 < n && s[i + 1] == 'g' && s[i + 2] == 't') { c = '>'; i += 3; } else if (i + 4 < n && s[i + 1] == 'a' && s[i + 2] == 'm') { c = '&'; i += 4; } else if (i + 5 < n && s[i + 1] == 'n') { c = ' '; i += 5; } }
        if (c == '\n' || c == '\t' || c == '\r') c = ' ';
        if (c == ' ' && (tl_ == 0 || txt[tl_ - 1] == ' ')) continue;
        if (tl_ < 380) txt[tl_++] = c;
        if (!ina && tl_ >= 380) FLUSH();
    }
    (void)ina; FLUSH();
#undef FLUSH
}
static void pg_file(int idx) {
    bl_add(1, nd[idx].name, -1); int L = slen(nd[idx].name);
    int html = L > 4 && (ci_eq(nd[idx].name + L - 4, ".htm") || (L > 5 && ci_eq(nd[idx].name + L - 5, ".html")));
    if (!nd[idx].size) { bl_add(6, "(empty file)", -1); }
    else if (html) html_render(fdata[idx], nd[idx].size);
    else { int st = 0; for (int i = 0; i <= nd[idx].size; i++) if (i == nd[idx].size || fdata[idx][i] == '\n') { char tmp[200]; int c = i - st > 199 ? 199 : i - st; memcpy(tmp, fdata[idx] + st, (unsigned)c); tmp[c] = 0; if (c) bl_wrap(0, tmp); else bl_add(0, "", -1); st = i + 1; } }
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
static void pg_http(const char *url);
static int nt_status(void); static const char *nt_card_name(void);
static void br_go(const char *url0, int push) {
    char url[96]; scpy(url, url0, 96); int s = 0; while (url[s] == ' ') s++; if (s) { int q = 0; while (url[s]) url[q++] = url[s++]; url[q] = 0; }
    int e = slen(url); while (e > 0 && url[e - 1] == ' ') url[--e] = 0;
    if (!url[0]) scpy(url, "vin://home", 96);
    if (starts_ci(url, "vin://app/calendar")) { app_open_cal(0, 0); return; }
    bn = 0; bnl = 0; bsel = 0; bscroll = 0; bcols = (ww - 32) / 8 - 3; if (bcols > 110) bcols = 110;
    if (starts_ci(url, "vin://home") || ci_eq(url, "home")) { scpy(url, "vin://home", 96); pg_home(); }
    else if (starts_ci(url, "vin://about")) pg_about(); else if (starts_ci(url, "vin://help")) pg_help();
    else if (starts_ci(url, "vin://glacier")) pg_glacier(); else if (starts_ci(url, "vin://commands")) pg_commands();
    else if (starts_ci(url, "vin://search?q=")) pg_search(url + 15);
    else if (starts_ci(url, "vin://")) pg_missing(url);
    else {
        const char *nm = starts_ci(url, "file:") ? url + 5 : url; int idx = resolve(nm);
        if (idx < 0) { char leaf[24]; int par; if (resolve_parent(nm, &par, leaf)) { add_txt(leaf); idx = fs_find(par, leaf); } }
        if (idx > 0 && !nd[idx].isdir) pg_file(idx);
        else if (starts_ci(url, "http://") || starts_ci(url, "https://") || starts_ci(url, "www.") || (ci_has(url, ".") && !ci_has(url, " "))) pg_http(url);
        else { char q[96] = "vin://search?q="; scpy(q + 15, url, 80); scpy(url, q, 96); pg_search(url + 15); }
    }
    scpy(burl, url, 96); dirty = 1;
    if (push) { bhn = bhi + 1; if (bhn >= 16) { for (int i = 1; i < 16; i++) scpy(bh[i - 1], bh[i], 96); bhn = 15; } scpy(bh[bhn], url, 96); bhi = bhn; bhn++; }
}
static void app_open_browser(const char *url) {
    app_on = 2; bfocus = 0; bedl = 0; bedit[0] = 0; bhn = 0; bhi = -1; br_go(url, 1);
    if (ci_eq(burl, "vin://home")) bfocus = 1;
    notify("BeeBrowser", 0);
}
static void br_draw(u32 *b, u32 ms) {
    int ax = wx + 16, ay = wy + 34 + 8, aw = ww - 32, ah = wh - 34 - 20;
    draw_rr(b, ax, ay, aw, 34, 17, 0xFFFFFF, bfocus ? 38 : 22); if (bfocus) draw_rr(b, ax - 1, ay - 1, aw + 2, 36, 18, pal[A_ACC], 40);
    text(b, ax + 14, ay + 9, "<", bhi > 0 ? pal[A_TXT] : 0x8AA6BE); text(b, ax + 38, ay + 9, ">", bhi < bhn - 1 ? pal[A_TXT] : 0x8AA6BE); text(b, ax + 62, ay + 9, "H", pal[A_ACC]);
    hit_add(ax + 10, ay + 4, 22, 26, 30); hit_add(ax + 34, ay + 4, 22, 26, 31); hit_add(ax + 58, ay + 4, 22, 26, 32); hit_add(ax + 88, ay, aw - 88 - 90, 34, 33);
    if (hot(ax + 10, ay + 4, 22, 26) || hot(ax + 34, ay + 4, 22, 26) || hot(ax + 58, ay + 4, 22, 26)) draw_rr(b, ax + 8, ay + 4, 76, 26, 13, 0xFFFFFF, 18);
    const char *ut = bfocus ? bedit : burl; int ul = slen(ut), maxc = (aw - 92 - 90) / 8; const char *us = ul > maxc ? ut + (ul - maxc) : ut;
    text(b, ax + 92, ay + 9, us, bfocus ? pal[A_TXT] : pal[A_DIM]);
    if (bfocus && (ms / 500) % 2 == 0) draw_rr(b, ax + 92 + slen(us) * 8, ay + 10, 8, 14, 2, pal[A_ACC], 210);
    draw_rr(b, ax + aw - 78, ay + 8, 68, 18, 9, 0xFFB454, 70); text(b, ax + aw - 70, ay + 9, "OFFLINE", 0xFFD58A);
    int py = ay + 48, lim = ay + ah - 26, y = py;
    for (int i = bscroll; i < bn && y < lim; i++) {
        const BLine *L = &bl[i]; int x = ax + 8;
        switch (L->kind) {
        case 0: text(b, x, y, L->t, pal[A_TXT]); y += 20; break;
        case 9: text(b, x + 24, y, L->t, pal[A_TXT]); y += 20; break;
        case 1: text_s(b, x, y, L->t, pal[A_ACC], 2); y += 40; break;
        case 2: text(b, x, y + 4, L->t, 0xFFE08A); draw_rr(b, x, y + 22, 8 * slen(L->t), 1, 0, 0xFFE08A, 90); y += 30; break;
        case 3: {
            int sel = (!bfocus && L->link == bsel); char nb[8]; int ln = (int)L->link + 1; nb[0] = '['; nb[1] = ln >= 10 ? (char)('0' + ln / 10) : (char)('0' + ln); nb[2] = ln >= 10 ? (char)('0' + ln % 10) : ']'; nb[3] = ln >= 10 ? ']' : 0; nb[4] = 0;
            int lw_ = 8 * (slen(L->t) + 5) + 8; hit_add(x - 4, y - 2, lw_, 22, 200 + L->link);
            if (sel || hot(x - 4, y - 2, lw_, 22)) draw_rr(b, x - 4, y - 2, lw_, 22, 8, pal[A_ACC], 50);
            text(b, x, y, nb, pal[A_DIM]); text(b, x + 8 * 4, y, L->t, 0x7FD0FF); draw_rr(b, x + 32, y + 17, 8 * slen(L->t), 1, 0, 0x7FD0FF, 120); y += 24; break; }
        case 4: draw_rr(b, x + 4, y + 6, 6, 6, 3, pal[A_ACC], 255); text(b, x + 22, y, L->t, pal[A_TXT]); y += 20; break;
        case 5: draw_rr(b, x, y + 5, aw - 24, 1, 0, 0xFFFFFF, 60); y += 14; break;
        case 6: text(b, x, y, L->t, pal[A_DIM]); y += 20; break;
        case 7: { static const u32 lc[9] = {0x6FE3FF, 0xFF8FA3, 0xFFE08A, 0x8FFFC4, 0x7FB2FF, 0xFFB454, 0xC9A0FF, 0x6FE3FF, 0xFF8FA3}; const char *nm = "BeeSearch"; int tw2 = 9 * 8 * 3, lx = ax + (aw - tw2) / 2;
            for (int k = 0; k < 9; k++) glyph_s(b, lx + k * 24, y, nm[k], lc[k], 3); y += 62; break; }
        case 8: { int bw = aw - 120; if (bw > 540) bw = 540; int bx = ax + (aw - bw) / 2; draw_rr(b, bx, y, bw, 40, 20, 0xFFFFFF, bfocus ? 46 : 26); hit_add(bx, y, bw, 40, 34); if (bfocus) draw_rr(b, bx - 1, y - 1, bw + 2, 42, 21, pal[A_ACC], 50);
            if (bfocus || bedit[0]) { text(b, bx + 20, y + 12, bedit, pal[A_TXT]); if (bfocus && (ms / 500) % 2 == 0) draw_rr(b, bx + 20 + slen(bedit) * 8, y + 13, 8, 14, 2, pal[A_ACC], 210); }
            else text(b, bx + 20, y + 12, "Search PixelBeeOS...", pal[A_DIM]); y += 56; break; }
        }
    }
}
static void br_vis_fix(void) {                         /* keep the selected link on screen */
    for (int i = 0; i < bn; i++) if (bl[i].kind == 3 && bl[i].link == bsel) { int vis = (wh - 34 - 20 - 80) / 22; if (i < bscroll) bscroll = i; else if (i > bscroll + vis - 2) bscroll = i - vis + 2; if (bscroll < 0) bscroll = 0; return; }
}
static void br_key(int k) {
    if (bfocus) {
        if (k == 27 || k == '\t') { bfocus = 0; if (!bedit[0]) bedl = 0; }
        else if (k == '\n') { char q[96]; scpy(q, bedit, 96); bedit[0] = 0; bedl = 0; bfocus = 0; br_go(q, 1); }
        else if (k == '\b') { if (bedl) bedit[--bedl] = 0; }
        else if (k >= 32 && k < 127 && bedl < 90) { bedit[bedl++] = (char)k; bedit[bedl] = 0; }
        return;
    }
    if (k == 27 || k == 'q' || k == 'Q') { app_on = 0; return; }
    else if (k == '\t' || k == '/') { bfocus = 1; bedl = 0; bedit[0] = 0; }
    else if (k == KEY_DOWN || k == 'j') { if (bscroll < bn - 1) bscroll++; }
    else if (k == KEY_UP || k == 'k') { if (bscroll > 0) bscroll--; }
    else if (k == ' ') { bscroll += 8; if (bscroll > bn - 1) bscroll = bn - 1; }
    else if (k == 'n' || k == 'N') { if (bnl) { bsel = (bsel + 1) % bnl; br_vis_fix(); } }
    else if (k == 'p' || k == 'P') { if (bnl) { bsel = (bsel + bnl - 1) % bnl; br_vis_fix(); } }
    else if (k == '\n') { if (bnl && bsel < bnl) { char u[96]; scpy(u, blu[bsel], 96); br_go(u, 1); } }
    else if (k >= '1' && k <= '9') { int li = k - '1'; if (li < bnl) { char u[96]; scpy(u, blu[li], 96); br_go(u, 1); } }
    else if (k == 'b' || k == 'B') { if (bhi > 0) { bhi--; char u[96]; scpy(u, bh[bhi], 96); br_go(u, 0); } }
    else if (k == 'f' || k == 'F') { if (bhi < bhn - 1) { bhi++; char u[96]; scpy(u, bh[bhi], 96); br_go(u, 0); } }
    else if (k == 'h' || k == 'H') br_go("vin://home", 1);
}
/* ------------------------------ PAINT ------------------------------ */
static u8 canvas[900 * 520]; static int pt_col = 1, pt_size = 1, pt_erase, pt_lx = -1, pt_ly, pt_x0, pt_y0, pt_w, pt_h;
static const u32 pt_pal[11] = {0xF7FBFF, 0x1B2A41, 0xE63946, 0xFF9F1C, 0xFFD60A, 0x2EC4B6, 0x2D7FD3, 0x8A4FFF, 0xFF70A6, 0x3BB273, 0x8D6E63};
static const int pt_r[3] = {2, 5, 10};
static void app_open_paint(void) { app_on = 3; pt_lx = -1; dirty = 1; notify("Paint  -  draw with the mouse", 0); }
static void pt_stamp(int x, int y, int r, u8 v) {
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx * dx + dy * dy <= r * r) {
        int px = x + dx, py = y + dy; if (px >= 0 && py >= 0 && px < pt_w && py < pt_h) canvas[py * 900 + px] = v;
    }
}
static void pt_line(int x0, int y0, int x1, int y1, int r, u8 v) {
    int dx = x1 - x0, dy = y1 - y0, ax_ = dx < 0 ? -dx : dx, ay_ = dy < 0 ? -dy : dy, n = ax_ > ay_ ? ax_ : ay_; if (!n) n = 1;
    for (int i = 0; i <= n; i++) pt_stamp(x0 + dx * i / n, y0 + dy * i / n, r, v);
}
static void pt_draw(u32 *b, u32 ms) {
    (void)ms; int ax = wx + 16, ay = wy + 34 + 8, aw = ww - 32, ah = wh - 34 - 20;
    for (int i = 0; i < 10; i++) {
        int x = ax + 8 + i * 34, y = ay + 4; hit_add(x, y, 26, 26, 300 + i);
        if ((pt_col == i + 1 && !pt_erase) || hot(x, y, 26, 26)) draw_rr(b, x - 3, y - 3, 32, 32, 16, 0xFFFFFF, (pt_col == i + 1 && !pt_erase) ? 230 : 90);
        draw_rr(b, x, y, 26, 26, 13, pt_pal[i + 1], 255);
    }
    for (int i = 0; i < 3; i++) {
        int x = ax + 366 + i * 44, y = ay + 2; hit_add(x, y, 38, 30, 320 + i);
        draw_rr(b, x, y, 38, 30, 10, 0xFFFFFF, pt_size == i ? 60 : (hmix(18, 36, hov(x, y, 38, 30))));
        int r = pt_r[i] > 8 ? 8 : pt_r[i]; draw_rr(b, x + 19 - r, y + 15 - r, 2 * r + 1, 2 * r + 1, r, 0xFFFFFF, 255);
    }
    hit_add(ax + 506, ay + 2, 72, 30, 331); draw_rr(b, ax + 506, ay + 2, 72, 30, 10, 0xFFFFFF, pt_erase ? 60 : (hmix(18, 36, hov(ax + 506, ay + 2, 72, 30)))); text(b, ax + 518, ay + 9, "Eraser", pal[A_TXT]);
    hit_add(ax + 588, ay + 2, 64, 30, 330); draw_rr(b, ax + 588, ay + 2, 64, 30, 10, 0xFF8FA3, hmix(40, 90, hov(ax + 588, ay + 2, 64, 30))); text(b, ax + 604, ay + 9, "Clear", pal[A_TXT]);
    pt_x0 = ax; pt_y0 = ay + 44; pt_w = aw > 900 ? 900 : aw; pt_h = ah - 44 - 26; if (pt_h > 520) pt_h = 520;
    hit_add(pt_x0, pt_y0, pt_w, pt_h, 340);
    draw_rr(b, pt_x0, pt_y0, pt_w, pt_h, 10, pt_pal[0], 255);
    for (int y = 0; y < pt_h; y++) { int sy = pt_y0 + y; if (sy >= (int)H) break; for (int x = 0; x < pt_w; x++) { u8 v = canvas[y * 900 + x]; if (v && pt_x0 + x < (int)W) b[sy * W + pt_x0 + x] = pt_pal[v]; } }
    if (hot(pt_x0, pt_y0, pt_w, pt_h)) { int r = pt_r[pt_size] * (pt_erase ? 3 : 2) / 2; draw_rr(b, mx - r, my - r, 2 * r + 1, 2 * r + 1, r, 0x000000, 55); }
}
static void pt_key(int k) {
    if (k == 27) { app_on = 0; return; }
    if (k >= '1' && k <= '9') { pt_col = k - '0'; pt_erase = 0; } else if (k == '0') { pt_col = 10; pt_erase = 0; }
    else if (k == '+' || k == '=') { if (pt_size < 2) pt_size++; } else if (k == '-') { if (pt_size > 0) pt_size--; }
    else if (k == 'e' || k == 'E') pt_erase = !pt_erase;
    else if (k == 'c' || k == 'C') { memset(canvas, 0, sizeof canvas); notify("Canvas cleared", 0); }
}

/* ------------------------------ mouse ------------------------------ */
static void run_in_terminal(const char *cmd) {
    if (game_on) game_exit(); if (ed_on) return;
    focus_terminal(); ll = 0; t_putc('\n', A_TXT); t_puts(cmd, A_ACC); t_putc('\n', A_TXT); exec_line(cmd); if (!ed_on && !app_on) prompt(); dirty = 1;
}
static void ui_click(int id) {
    if (id >= 4000 && id < 4100) { ux_mc_click(id); return; }
    if (folder_prompt || gallery_open) { if (id == 2203) gallery_open = 0; return; }
    if (id == 2200) { desk_menu_open = 0; folder_parent = desktop_node; folder_prompt = 1; new_folder_len = 0; new_folder_name[0] = 0; dirty = 1; return; }
    if (id == 2201) { desk_menu_open = 0; gallery_open = 1; app_on = 0; terminal_visible = 0; dirty = 1; return; }
    if (id == 2202) { desk_menu_open = 0; gallery_open = 0; app_at(8)->open(); dirty = 1; return; }
    if (id >= 3100 && id < 3100 + NODES) { int node = id - 3100; desk_menu_open = 0; desktop_open_node(node); dirty = 1; return; }
    if (id >= 3000 && id <= 3005) {
        launchpad_open = 0; launcher_open = 0; control_open = 0; terminal_visible = 0;
        if (id == 3000) desktop_open_this_pc();
        else if (id == 3005) desktop_open_drive();
        else desktop_open_folder(id - 3001);
        dirty = 1; return;
    }
    if (id == 2000) {
        launchpad_open = 0; launcher_open = 0; control_open = 0; terminal_visible = 0;
        terminal_visible = 0; term_focus = 1; app_on = 0; for (int i = 0; i < nwin; i++) win_[i].min = 1; build_scene(); dirty = 1; return;
    }
    if (id == 2001) { control_open = 0; launcher_open = 0; launchpad_open = !launchpad_open; dirty = 1; return; }
    if (id == 2002) { launchpad_open = 0; control_open = 0; launcher_open = 1; launcher_query[0] = 0; launcher_len = 0; dirty = 1; return; }
    if (id >= 2301 && id < 2300 + NODES) { int node = id - 2300; if (node > 0 && nd[node].used) { launchpad_open = 0; launchpad_query[0] = 0; launchpad_len = 0; if (nd[node].isdir) desktop_open_node(node); else app_open_notes_node(node); dirty = 1; } return; }
    if (id >= 2400 && id < 2400 + app_count()) { int n = id - 2400; launchpad_open = 0; launchpad_query[0] = 0; launchpad_len = 0; app_at(n)->open(); dirty = 1; return; }
    if (id == 2003 || id == 2004) { launchpad_open = 0; launcher_open = 0; control_open = !control_open; dirty = 1; return; }
    if (id == 2100) { if (display_level > 30) display_level -= 10; dirty = 1; return; }
    if (id == 2101) { if (display_level < 100) display_level += 10; dirty = 1; return; }
    if (id == 2102) { alerts_enabled = !alerts_enabled; if (!alerts_enabled) { speaker_off(); spk_active = 0; } dirty = 1; return; }
    if (id == 2104) { control_open = 0; dirty = 1; return; }
    if (id == 2010) { control_open = 0; launchpad_open = 0; launcher_open = 0; app_launch("network"); dirty = 1; return; }
    if (id >= 1700 && id <= 1700 + app_count()) { int n = id - 1700; launchpad_open = 0; control_open = 0; launcher_open = 0; if (n == 0) focus_terminal(); else { int rebuild = terminal_visible; terminal_visible = 0; if (rebuild) build_scene(); app_at(n - 1)->open(); } dirty = 1; return; }
    if (id == PWR_OFF_ID || id == PWR_RST_ID) {            /* dock power buttons: first click arms, second click within 3 s does it */
        if (pwr_arm == id && now_ms() < pwr_until) { if (id == PWR_OFF_ID) power_off(); else power_restart(); return; }
        pwr_arm = id; pwr_until = now_ms() + 3000; dirty = 1;
        notify(id == PWR_OFF_ID ? "Click again to shut down" : "Click again to restart", 2); return;
    }
    if (id >= 1000 && id < 1500) {                          /* window chrome: close / focus / drag / minimize / maximize */
        int kind = (id - 1000) / 100, app = (id - 1000) % 100;
        if (kind == 0) win_close(app);
        else if (kind == 3) win_minimize(app);
        else if (kind == 4) win_toggle_max(app);
        else if (kind == 2) { int i = win_find(app); if (i >= 0 && !win_[i].max) { drag_app = app; drag_ox = mx - win_[i].x; drag_oy = my - win_[i].y; } }
        dirty = 1; return;
    }
    if (id >= 1 && id <= 3) {
        if (ed_on) return; if (game_on) game_exit();
        switch (id) { case 1: launchpad_open = 1; launcher_open = 0; control_open = 0; dirty = 1; break; case 2: focus_terminal(); break; case 3: { int rebuild = terminal_visible; terminal_visible = 0; if (rebuild) build_scene(); desktop_open_this_pc(); dirty = 1; break; } }
        return;
    }
    if (id >= 500 && id < 500 + app_count()) {
        if (ed_on) return; if (game_on) game_exit();
        int n = id - 500 + 1, i = win_find(n);
        if (i >= 0) { if (win_[i].min) win_[i].t0 = cur_ms; win_[i].min = 0; term_focus = 0; win_raise(i); app_on = n; } else app_at(id - 500)->open();
        dirty = 1; return;
    }
    if (id == 10 || id == 11) {                              /* Terminal red / yellow light: hide the Terminal back to the desktop */
        if (nwin > 0 && !term_focus && !win_[nwin - 1].min) { win_close(win_[nwin - 1].app); return; }
        terminal_visible = 0; term_focus = 0; app_on = 0; build_scene(); dirty = 1; return;
    }
    if (app_on && app_at(app_on - 1)->click) app_at(app_on - 1)->click(id);
}
/* per-app click handlers (ids are private to each app) */
static void cal_click(int id) {
    if (ca_in) ca_in = 0;
    if (id == 20) cal_key('['); else if (id == 21) cal_key(']');
    else if (id >= 100 && id < 142) { ca_y = cal_cy[id - 100]; ca_m = cal_cm[id - 100]; ca_d = cal_cd[id - 100]; }
}
static void br_click(int id) {
    if (id == 30) { bfocus = 0; br_key('b'); } else if (id == 31) { bfocus = 0; br_key('f'); } else if (id == 32) { bfocus = 0; br_key('h'); }
    else if (id == 33 || id == 34) { bfocus = 1; bedl = 0; bedit[0] = 0; }
    else if (id >= 200 && id < 224) { int li = id - 200; if (li < bnl) { char u[96]; scpy(u, blu[li], 96); bfocus = 0; br_go(u, 1); } }
}
static void pt_click(int id) {
    if (id >= 300 && id < 310) { pt_col = id - 299; pt_erase = 0; } else if (id >= 320 && id < 323) pt_size = id - 320;
    else if (id == 330) pt_key('c'); else if (id == 331) pt_erase = !pt_erase;
}
static void pt_mouse(int btn, int mx_, int my_) {
    if (btn & 3) {
        int cx2 = mx_ - pt_x0, cy2 = my_ - pt_y0, inside = cx2 >= 0 && cy2 >= 0 && cx2 < pt_w && cy2 < pt_h;
        if (inside || pt_lx >= 0) {
            u8 v = ((btn & 2) || pt_erase) ? 0 : (u8)pt_col; int r = pt_r[pt_size] * ((btn & 2) || pt_erase ? 3 : 2) / 2;
            if (pt_lx < 0) pt_stamp(cx2, cy2, r, v); else pt_line(pt_lx, pt_ly, cx2, cy2, r, v);
            pt_lx = cx2; pt_ly = cy2;
        }
    } else pt_lx = -1;
}
/* The pointer moves the instant a packet is decoded (mouse_packet, may run from inside the drawing code).
 * Clicks, hover and drags are queued with the position they happened at and handled in mouse_drain() from the main loop. */
typedef struct { short x, y; u8 btn; } MEv;
static MEv mev[64]; static int mevh, mevt;
static void mouse_packet(int btn, int dx, int dy) {
    {   /* pointer acceleration with sub-pixel carry: slow = 1:1 for precision, fast flicks cover the screen; no jitter from rounding */
        static int ax, ay; int s = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy), g = 16;
        if (s > 3) g += ((s > 22 ? 22 : s) - 3) * 3 / 2;                  /* 1.0x up to ~2.8x (16..44 sixteenths) */
        ax += dx * g; ay += dy * g; dx = ax >> 4; dy = ay >> 4; ax &= 15; ay &= 15;
    }
    pmx += dx; pmy += dy; if (pmx < 0) pmx = 0; if (pmy < 0) pmy = 0; if (pmx >= (int)W) pmx = (int)W - 1; if (pmy >= (int)H) pmy = (int)H - 1;
    mouse_seen = 1;
    int last = (mevt + 63) & 63;
    if (mevh != mevt && mev[last].btn == (u8)btn) { mev[last].x = (short)pmx; mev[last].y = (short)pmy; return; }   /* same buttons: just a newer position */
    int n = (mevt + 1) & 63; if (n == mevh) { mev[last].x = (short)pmx; mev[last].y = (short)pmy; mev[last].btn = (u8)btn; return; }
    mev[mevt].x = (short)pmx; mev[mevt].y = (short)pmy; mev[mevt].btn = (u8)btn; mevt = n;
}
static void mouse_logic(int btn) {
    input_seen = 1;
    if (locked || pw_ui) { mbtn = btn; dirty = 1; return; }          /* screen locked: pointer moves, nothing is clickable */
    ux_hot_corners(btn);
    if ((btn & 2) && !(mbtn & 2) && !terminal_visible && !app_on && !launcher_open && !launchpad_open && !control_open) {
        desk_menu_open = 1; desk_menu_x = mx; desk_menu_y = my; dirty = 1;
    }
    if ((btn & 1) && !(mbtn & 1)) { if (!mc_open) win_focus_at(mx, my); for (int i = nhits - 1; i >= 0; i--) if (mx >= hits[i].x && mx < hits[i].x + hits[i].w && my >= hits[i].y && my < hits[i].y + hits[i].h) { ui_click(hits[i].id); break; } }
    if (drag_app) { if (btn & 1) { int i = win_find(drag_app); if (i >= 0) { win_[i].x = mx - drag_ox; win_[i].y = my - drag_oy; win_clamp(&win_[i]); } } else { ux_snap_release(drag_app); drag_app = 0; } }
    if (app_on && app_at(app_on - 1)->mouse) app_at(app_on - 1)->mouse(btn, mx, my);
    { int od = dock_peek; if (!dock_hidden()) dock_peek = 0; else if (my >= (int)H - 6) dock_peek = 1; else if (my < dky - 12) dock_peek = 0; if (od != dock_peek) dirty = 1; }
    { static int last_hid = -1; int hid = 0;      /* only redraw the screen when something under the pointer changed */
      for (int i = nhits - 1; i >= 0; i--) if (mx >= hits[i].x && mx < hits[i].x + hits[i].w && my >= hits[i].y && my < hits[i].y + hits[i].h) { hid = hits[i].id; break; }
      if (btn != mbtn || btn || drag_app || hid != last_hid) dirty = 1; last_hid = hid; }
    mbtn = btn;
}
static void mouse_drain(void) {
    if (mevh == mevt) return;
    while (mevh != mevt) { MEv e = mev[mevh]; mevh = (mevh + 1) & 63; mx = e.x; my = e.y; mouse_logic(e.btn); }
    mx = pmx; my = pmy;
}
static void app_key(int k) { dirty = 1; if (app_on && app_at(app_on - 1)->key) app_at(app_on - 1)->key(k); }

/* ------------------------------ CALCULATOR ------------------------------ */
static int kc_v, kc_acc, kc_op, kc_new = 1, kc_err;
static const struct { const char *l; char k; u8 c, r, s; } kc_btn[] = {
    {"C", 'c', 0, 0, 1}, {"+/-", 'n', 1, 0, 1}, {"<", '\b', 2, 0, 1}, {"/", '/', 3, 0, 1},
    {"7", '7', 0, 1, 1}, {"8", '8', 1, 1, 1}, {"9", '9', 2, 1, 1}, {"*", '*', 3, 1, 1},
    {"4", '4', 0, 2, 1}, {"5", '5', 1, 2, 1}, {"6", '6', 2, 2, 1}, {"-", '-', 3, 2, 1},
    {"1", '1', 0, 3, 1}, {"2", '2', 1, 3, 1}, {"3", '3', 2, 3, 1}, {"+", '+', 3, 3, 1},
    {"0", '0', 0, 4, 2}, {"=", '=', 2, 4, 2}};
#define KC_NB ((int)(sizeof kc_btn / sizeof kc_btn[0]))
static void app_open_calc(void) { app_on = 4; kc_v = kc_acc = kc_op = kc_err = 0; kc_new = 1; dirty = 1; notify("Calculator  -  keys or mouse", 0); }
static void kc_eval(void) {
    int r = kc_v;
    if (kc_op == '+') { if (__builtin_add_overflow(kc_acc, kc_v, &r)) kc_err = 1; }
    else if (kc_op == '-') { if (__builtin_sub_overflow(kc_acc, kc_v, &r)) kc_err = 1; }
    else if (kc_op == '*') { if (__builtin_mul_overflow(kc_acc, kc_v, &r)) kc_err = 1; }
    else if (kc_op == '/') { if (!kc_v) kc_err = 1; else r = kc_acc / kc_v; }
    kc_acc = kc_v = kc_err ? 0 : r;
}
static void kc_key(int k) {
    if (k == 27) { app_on = 0; return; }
    if (k == 'c' || k == 'C') { kc_v = kc_acc = kc_op = kc_err = 0; kc_new = 1; return; }
    if (kc_err) return;
    if (k >= '0' && k <= '9') { if (kc_new) { kc_v = 0; kc_new = 0; } if (kc_v < 99999999 && kc_v > -99999999) kc_v = kc_v * 10 + (kc_v < 0 ? -(k - '0') : (k - '0')); }
    else if (k == '+' || k == '-' || k == '*' || k == '/') { if (kc_op && !kc_new) kc_eval(); else if (!kc_op) kc_acc = kc_v; kc_op = k; kc_new = 1; }
    else if (k == '=' || k == '\n') { if (kc_op) { kc_eval(); kc_op = 0; } kc_new = 1; }
    else if (k == '\b') { if (!kc_new) kc_v /= 10; }
    else if (k == 'n' || k == 'N') kc_v = -kc_v;
}
static void kc_click(int id) { if (id >= 600 && id < 600 + KC_NB) kc_key(kc_btn[id - 600].k); }
static void kc_draw(u32 *b, u32 ms) {
    (void)ms; int pw = 320, ax = wx + (ww - pw) / 2, ay = wy + 34 + 8;
    draw_rr(b, ax, ay, pw, 74, 14, 0x000000, 90);
    char t[16]; if (kc_err) scpy(t, "Error", 16); else { int p = 0; u32 m = kc_v < 0 ? 0u - (u32)kc_v : (u32)kc_v; if (kc_v < 0) t[p++] = '-'; char nb[12]; fmt_base(m, 10, nb); for (char *q = nb; *q; q++) t[p++] = *q; t[p] = 0; }
    int tw_ = slen(t) * 24; text_s(b, ax + pw - 14 - tw_, ay + 22, t, kc_err ? 0xFF9FB0 : pal[A_TXT], 3);
    if (kc_op) { char o[2] = {(char)kc_op, 0}; text(b, ax + 12, ay + 8, o, pal[A_ACC]); }
    int bw = pw / 4, bh_ = (wh - 34 - 20 - 74 - 24) / 5; if (bh_ > 56) bh_ = 56; if (bh_ < 30) bh_ = 30;
    for (int i = 0; i < KC_NB; i++) {
        int x = ax + kc_btn[i].c * bw, y = ay + 84 + kc_btn[i].r * bh_, w = bw * kc_btn[i].s;
        int isop = kc_btn[i].k == '+' || kc_btn[i].k == '-' || kc_btn[i].k == '*' || kc_btn[i].k == '/' || kc_btn[i].k == '=';
        draw_rr(b, x + 3, y + 3, w - 6, bh_ - 6, 12, isop ? pal[A_ACC] : 0xFFFFFF, hot(x, y, w, bh_) ? 90 : (isop ? 60 : 28));
        text(b, x + w / 2 - 4 * slen(kc_btn[i].l), y + bh_ / 2 - 8, kc_btn[i].l, pal[A_TXT]); hit_add(x, y, w, bh_, 600 + i);
    }
}

/* ------------------------------ CLOCK ------------------------------ */
static const u8 seg_[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static void app_open_clock(void) { app_on = 5; dirty = 1; notify("Clock", 0); }
static void clk_key(int k) { if (k == 27) app_on = 0; }
static void seg_digit(u32 *b, int x, int y, int w, int h, int d, u32 col) {
    int t = w / 6 + 1, s = seg_[d], hh = h / 2;
#define SG(bit, X, Y, WW, HH) draw_rr(b, X, Y, WW, HH, t / 2, col, (s >> (bit)) & 1 ? 255 : 22)
    SG(0, x + t, y, w - 2 * t, t); SG(1, x + w - t, y + t, t, hh - t); SG(2, x + w - t, y + hh, t, hh - t); SG(3, x + t, y + h - t, w - 2 * t, t);
    SG(4, x, y + hh, t, hh - t); SG(5, x, y + t, t, hh - t); SG(6, x + t, y + hh - t / 2, w - 2 * t, t);
#undef SG
}
static void clk_draw(u32 *b, u32 ms) {
    (void)ms; int hh, mm, ss, dd, mo, yy; rtc_read(&hh, &mm, &ss, &dd, &mo, &yy);
    int w = (ww - 80) / 9; if (w > 70) w = 70; if (w < 24) w = 24; int h = w * 9 / 5, gap = w / 5, cw_ = w / 3;
    int total = 6 * w + 5 * gap + 2 * cw_, x = wx + (ww - total) / 2, y = wy + (wh - h) / 2 - 20;
    int dg[6] = {hh / 10, hh % 10, mm / 10, mm % 10, ss / 10, ss % 10};
    for (int i = 0; i < 6; i++) {
        seg_digit(b, x, y, w, h, dg[i], pal[A_ACC]); x += w + gap;
        if (i == 1 || i == 3) { draw_rr(b, x + cw_ / 2 - 3, y + h / 3, 6, 6, 3, pal[A_ACC], 255); draw_rr(b, x + cw_ / 2 - 3, y + 2 * h / 3, 6, 6, 3, pal[A_ACC], 255); x += cw_; }
    }
    char t[40]; int p = 0; { char nb[12]; fmt_base((u32)dd, 10, nb); for (char *q = nb; *q; q++) t[p++] = *q; } t[p++] = ' ';
    for (const char *s = mon_[(mo < 1 || mo > 12 ? 1 : mo) - 1]; *s; s++) t[p++] = *s; t[p++] = ' '; { char nb[12]; fmt_base((u32)yy, 10, nb); for (char *q = nb; *q; q++) t[p++] = *q; } t[p] = 0;
    text_s(b, wx + (ww - slen(t) * 16) / 2, y + h + 34, t, pal[A_TXT], 2);
}

#include "apps2.h"

/* ------------------------------ DOCK ICONS ------------------------------ */
static void icon_cal(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 10, 0xF4FAFF, 250); draw_rr(b, ix, iy, 34, 12, 7, 0xFF6B81, 255); draw_rr(b, ix, iy + 6, 34, 6, 0, 0xFF6B81, 255);
    for (int k = 0; k < 6; k++) draw_rr(b, ix + 6 + (k % 3) * 8, iy + 16 + (k / 3) * 8, 5, 5, 2, 0x2A5D86, 255);
}
static void icon_browser(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 17, 0x2D7FD3, 255);
    draw_rr(b, ix + 9, iy + 3, 16, 28, 8, 0xFFFFFF, 230); draw_rr(b, ix + 11, iy + 5, 12, 24, 6, 0x2D7FD3, 255);
    draw_line(b, ix + 17, iy + 3, ix + 17, iy + 31, 0xFFFFFF, 200); draw_line(b, ix + 3, iy + 17, ix + 31, iy + 17, 0xFFFFFF, 200);
    draw_line(b, ix + 6, iy + 10, ix + 28, iy + 10, 0xFFFFFF, 140); draw_line(b, ix + 6, iy + 24, ix + 28, iy + 24, 0xFFFFFF, 140);
}
static void icon_paint(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 10, 0xFFE9C7, 255);
    draw_rr(b, ix + 6, iy + 7, 9, 9, 4, 0xE63946, 255); draw_rr(b, ix + 18, iy + 6, 9, 9, 4, 0x2D7FD3, 255);
    draw_rr(b, ix + 7, iy + 19, 9, 9, 4, 0x3BB273, 255); draw_rr(b, ix + 19, iy + 19, 9, 9, 4, 0xFFB454, 255);
}
static void icon_calc(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 10, 0x2B3A55, 250); draw_rr(b, ix + 5, iy + 5, 24, 8, 3, 0x9FE8FF, 255);
    for (int k = 0; k < 6; k++) draw_rr(b, ix + 5 + (k % 3) * 8, iy + 16 + (k / 3) * 7, 6, 5, 2, k == 5 ? 0xFFB454 : 0xDDE8F5, 255);
}
static void icon_clock(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 17, 0x0D2238, 250); draw_rr(b, ix + 3, iy + 3, 28, 28, 14, 0xF4FAFF, 255);
    draw_line(b, ix + 17, iy + 17, ix + 17, iy + 8, 0x1F4F78, 255); draw_line(b, ix + 17, iy + 17, ix + 24, iy + 20, 0xFF6B81, 255);
}

/* ===================================================================
 * APP REGISTRY - to add an app:
 *   1. write  app_open_x()  (set app_on = <its row number, 1-based>),
 *      x_key(int k), x_draw(u32 *b, u32 ms), icon_x(u32 *b, int ix, int iy)
 *      (optional: x_click(int id) for hit_add ids, x_mouse(btn, mx, my))
 *   2. add ONE row to app_tab[] below
 *   3. add  R("name", "", "description", C_SYS)  to cmds.h
 * The dock icon, window title, keyboard, clicks, mouse and the
 * `apps` / launch-by-name CLI commands then work automatically.
 * =================================================================== */
static void open_cal_default(void) { app_open_cal(0, 0); }
static void open_browser_home(void) { app_open_browser("vin://home"); }
/* ------------------------------ SETTINGS (app 9) ------------------------------ */
static void app_open_settings(void) { app_on = 9; dirty = 1; }
static void st_key(int k) { if (k == 27) app_on = 0; }
static void st_click(int id) {
    if (id >= 910 && id < 910 + THEME_COUNT) { theme_i = id - 910; build_scene(); dirty = 1; notify("Appearance updated", 1); }
    else if (id == 920) { flat_ui = !flat_ui; if (!flat_ui && !blur_r) blur_r = 8; build_scene(); dirty = 1; notify(flat_ui ? "Desktop glass off (all solid)" : "Desktop glass on", 1); }
    else if (id == 921) {
        performance_mode = !performance_mode;
        if (performance_mode) { flat_ui = 1; blur_r = 0; win_glass_on = 0; app_fast = 0; }
        else { flat_ui = 0; blur_r = low_ram_mode ? 2 : 8; win_alpha = low_ram_mode ? 174 : 214; }
        build_scene(); dirty = 1;
        notify(performance_mode ? "Performance mode enabled" : "Balanced visuals restored", 1);
    }
}
static void st_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 28, y = wy + 54;
    heading(b, x, y, "Appearance", pal[A_ACC]);
    text(b, x, y + 30, "Choose a desktop color theme", pal[A_TXT]);
    for (int i = 0; i < THEME_COUNT; i++) {
        int tw = (ww - 56 - (THEME_COUNT - 1) * 8) / THEME_COUNT, bx = x + i * (tw + 8); draw_rr(b, bx, y + 62, tw, 74, 12, themes[i].tint, 220);
        draw_rr(b, bx + 8, y + 72, tw - 16, 12, 6, themes[i].sky[1], 230);
        text(b, bx + 10, y + 110, themes[i].name, pal[A_TXT]);
        if (theme_i == i) draw_rr(b, bx + 2, y + 64, tw - 4, 70, 11, 0xFFFFFF, 35);
        hit_add(bx, y + 62, tw, 74, 910 + i);
    }
    draw_rr(b, x, y + 164, 270, 38, 11, 0xFFFFFF, 25);
    text(b, x + 14, y + 177, flat_ui ? "Desktop glass: Off - all solid  (click to turn on)" : "Desktop glass: On - windows stay solid  (click to turn off)", pal[A_TXT]);
    hit_add(x, y + 164, 270, 38, 920);
    draw_rr(b, x, y + 218, 330, 40, 11, 0xFFFFFF, 30);
    text(b, x + 12, y + 231, performance_mode ? "Performance mode: ON  -  low effects" : "Performance mode: OFF  -  glass effects", pal[A_TXT]);
    hit_add(x, y + 218, 330, 40, 921);
    text(b, x, y + 274, low_ram_mode ? "Auto profile: 2 GB Lite (30 FPS cap, light glass)" : "Auto profile: Balanced (60 FPS target)", pal[A_DIM]);
    text(b, x, y + 296, "Lower blur keeps the frosted-glass look with less CPU work", pal[A_DIM]);
}
static void icon_settings(u32 *b, int x, int y) {
    draw_rr(b, x + 3, y + 3, 28, 28, 10, 0x72869A, 255);
    draw_rr(b, x + 10, y + 10, 14, 14, 7, 0xDDE8F2, 255);
    draw_rr(b, x + 14, y + 14, 6, 6, 3, 0x72869A, 255);
}

/* ------------------------------ SYSTEM MONITOR (app 10) ------------------------------ */
static void app_open_monitor(void) { app_on = 10; dirty = 1; }
static void sm_key(int k) { if (k == 27) app_on = 0; }
static void sm_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 28, y = wy + 58; char n[16];
    heading(b, x, y, "System overview", pal[A_ACC]);
    row_card(b, x - 12, y + 42 - 7, ww - 32); text(b, x, y + 42, "Display", pal[A_DIM]); fmt_base(W, 10, n); text(b, x + 200, y + 42, n, pal[A_TXT]); text(b, x + 130 + 8 * slen(n), y + 42, " x ", pal[A_DIM]); fmt_base(H, 10, n); text(b, x + 130 + 8 * (slen(n) + 3), y + 42, n, pal[A_TXT]);
    row_card(b, x - 12, y + 78 - 7, ww - 32); text(b, x, y + 78, "Memory reported by BIOS", pal[A_DIM]); fmt_base(mem_kb / 1024, 10, n); text(b, x + 190, y + 78, n, pal[A_TXT]); text(b, x + 190 + 8 * slen(n), y + 78, " MB", pal[A_TXT]);
    row_card(b, x - 12, y + 114 - 7, ww - 32); text(b, x, y + 114, "Desktop", pal[A_DIM]); text(b, x + 190, y + 114, "PixelBeeOS graphical shell", pal[A_TXT]);
    row_card(b, x - 12, y + 150 - 7, ww - 32); text(b, x, y + 150, "Input", pal[A_DIM]); text(b, x + 190, y + 150, "PS/2 keyboard and mouse", pal[A_TXT]);
}
static void icon_monitor(u32 *b, int x, int y) {
    draw_rr(b, x + 2, y + 3, 30, 23, 6, 0x274B70, 255);
    draw_rr(b, x + 5, y + 6, 24, 17, 3, 0x83D8FF, 255);
    draw_rr(b, x + 14, y + 27, 6, 4, 1, 0xDDE8F2, 255);
    draw_rr(b, x + 9, y + 31, 16, 3, 1, 0xDDE8F2, 255);
}

/* ------------------------------ PERFORMANCE CENTER (app 11) ------------------------------ */
static void app_open_performance(void) { app_on = 11; dirty = 1; }
static void pc_key(int k) { if (k == 27) app_on = 0; }
static void pc_draw(u32 *b, u32 ms) {
    char n[20]; int x = wx + 28, y = wy + 56;
    heading(b, x, y, "Performance center", pal[A_ACC]);
    row_card(b, x - 12, y + 34 - 7, ww - 32); text(b, x, y + 34, "Desktop render rate", pal[A_DIM]);
    fmt_base(perf_fps, 10, n); text(b, x + 210, y + 34, n, pal[A_TXT]);
    text(b, x + 210 + 8 * slen(n), y + 34, " FPS (measured)", pal[A_DIM]);
    draw_rr(b, x, y + 62, 470, 9, 5, 0x17284E, 220);
    { int q = (int)(perf_fps > 60 ? 60 : perf_fps); draw_rr(b, x, y + 62, 470 * q / 60, 9, 5, 0x4DCFFF, 245); }
    row_card(b, x - 12, y + 96 - 7, ww - 32); text(b, x, y + 96, "Memory reported by bootloader", pal[A_DIM]);
    fmt_base(mem_kb / 1024, 10, n); text(b, x + 250, y + 96, n, pal[A_TXT]); text(b, x + 250 + 8 * slen(n), y + 96, " MB", pal[A_TXT]);
    row_card(b, x - 12, y + 130 - 7, ww - 32); text(b, x, y + 130, "File storage", pal[A_DIM]); text(b, x + 250, y + 130, "Temporary RAM disk", pal[A_TXT]);
    row_card(b, x - 12, y + 164 - 7, ww - 32); text(b, x, y + 164, "Power profile", pal[A_DIM]); text(b, x + 250, y + 164, performance_mode ? "Maximum speed / flat UI" : (low_ram_mode ? "2 GB Lite / frosted glass" : "Balanced / frosted glass"), pal[A_TXT]);
    row_card(b, x - 12, y + 198 - 7, ww - 32); text(b, x, y + 198, "Network", pal[A_DIM]); text(b, x + 250, y + 198, "Driver not loaded", pal[A_TXT]);
    row_card(b, x - 12, y + 232 - 7, ww - 32); text(b, x, y + 232, "Bluetooth", pal[A_DIM]); text(b, x + 250, y + 232, "Driver not loaded", pal[A_TXT]);
    row_card(b, x - 12, y + 266 - 7, ww - 32); text(b, x, y + 266, "Battery", pal[A_DIM]); text(b, x + 250, y + 266, acpi_present ? "ACPI detected; gauge pending" : "Not available", pal[A_TXT]);
    (void)ms;
}
static void icon_performance(u32 *b, int x, int y) {
    draw_rr(b, x + 2, y + 3, 30, 28, 8, 0x102E50, 255);
    draw_rr(b, x + 7, y + 17, 4, 9, 2, 0x54D8FF, 255);
    draw_rr(b, x + 14, y + 11, 4, 15, 2, 0xFFFFFF, 255);
    draw_rr(b, x + 21, y + 6, 4, 20, 2, 0xF4C542, 255);
}

/* ------------------------------ TASK MANAGER (app 12) ------------------------------ */
static void app_open_tasks(void) { app_on = 12; dirty = 1; }
static void tm_key(int k) { if (k == 27) app_on = 0; }
static void tm_click(int id) {
    if (id >= 8801 && id < 8810) {
        int app = id - 8800;
        if (app == 12) return; /* Task Manager cannot terminate itself from its own list. */
        if (win_find(app) >= 0) { win_close(app); notify("Application task ended", 1); }
        dirty = 1;
    }
}
static void tm_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 26, y = wy + 54, shown = 0;
    heading(b, x, y, "Task manager", pal[A_ACC]);
    text(b, x, y + 28, "Interactive application windows", pal[A_DIM]);
    text(b, x, y + 56, "APP / TASK", pal[A_DIM]);
    text(b, x + 330, y + 56, "STATE", pal[A_DIM]);
    text(b, x + 470, y + 56, "ACTION", pal[A_DIM]);
    for (int i = nwin - 1; i >= 0 && shown < 6; i--) {
        int app = win_[i].app; if (app == 12) continue;
        const App *ap = app_at(app - 1); int ry = y + 78 + shown * 42;
        draw_rr(b, x, ry, 570, 34, 9, 0xFFFFFF, 13);
        text(b, x + 10, ry + 11, ap->name, pal[A_TXT]);
        text(b, x + 330, ry + 11, win_[i].min ? "Minimized" : "Running", win_[i].min ? pal[A_WARN] : pal[A_OK]);
        draw_rr(b, x + 468, ry + 4, 88, 26, 8, 0x8D3446, 210);
        text(b, x + 480, ry + 12, "End task", pal[A_TXT]); hit_add(x + 468, ry + 4, 88, 26, 8800 + app);
        shown++;
    }
    if (!shown) text(b, x, y + 92, "No other application windows are open.", pal[A_DIM]);
}
static void icon_tasks(u32 *b, int x, int y) {
    draw_rr(b, x + 3, y + 4, 28, 26, 7, 0x123B63, 255);
    for (int i = 0; i < 3; i++) { draw_rr(b, x + 8, y + 9 + i * 7, 18, 4, 2, i == 1 ? 0xF4C542 : 0x65D5FF, 255); }
}

/* ------------------------------ DEVICE MANAGER (app 13) ------------------------------ */
static void app_open_devices(void) { app_on = 13; dirty = 1; }
static void dv_key(int k) { if (k == 27) app_on = 0; }
static void dv_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 28, y = wy + 56; char n[20];
    heading(b, x, y, "Device manager", pal[A_ACC]);
    text(b, x, y + 30, "Detected hardware is listed separately from loaded drivers.", pal[A_DIM]);
    fmt_base((u32)pci_device_count, 10, n); row_card(b, x - 12, y + 70 - 7, ww - 32); text(b, x, y + 70, "PCI functions enumerated", pal[A_TXT]); text(b, x + 290, y + 70, n, pal[A_OK]);
    row_card(b, x - 12, y + 108 - 7, ww - 32); text(b, x, y + 108, "PS/2 keyboard + mouse input", pal[A_TXT]); text(b, x + 290, y + 108, "Initialized", pal[A_OK]);
    row_card(b, x - 12, y + 142 - 7, ww - 32); text(b, x, y + 142, "VBE framebuffer", pal[A_TXT]); text(b, x + 290, y + 142, "Active", pal[A_OK]);
    row_card(b, x - 12, y + 176 - 7, ww - 32); text(b, x, y + 176, "PIT system clock", pal[A_TXT]); text(b, x + 290, y + 176, "Active", pal[A_OK]);
    row_card(b, x - 12, y + 210 - 7, ww - 32); text(b, x, y + 210, "PCI network controller", pal[A_TXT]); text(b, x + 290, y + 210, nt_status() == 2 ? "Connected" : (nt_status() == 1 ? "Driver loaded, not connected" : (pci_net_present ? "Detected / not supported" : "Not detected")), nt_status() == 2 ? pal[A_OK] : pal[A_WARN]);
    row_card(b, x - 12, y + 244 - 7, ww - 32); text(b, x, y + 244, "PCI storage controller", pal[A_TXT]); text(b, x + 290, y + 244, pci_storage_present ? "Detected / driver missing" : "Not detected", pal[A_WARN]);
    row_card(b, x - 12, y + 278 - 7, ww - 32); text(b, x, y + 278, "USB controller", pal[A_TXT]); text(b, x + 290, y + 278, pci_usb_present ? "Detected / driver missing" : "Not detected", pal[A_WARN]);
    row_card(b, x - 12, y + 312 - 7, ww - 32); text(b, x, y + 312, "PCI audio controller", pal[A_TXT]); text(b, x + 290, y + 312, audio_device_present ? "Detected / driver pending" : "Not detected", pal[A_WARN]);
}
static void icon_devices(u32 *b, int x, int y) {
    draw_rr(b, x + 3, y + 3, 28, 28, 7, 0x163B61, 255);
    draw_rr(b, x + 10, y + 9, 14, 16, 3, 0x58D3FF, 255);
    for (int i = 0; i < 4; i++) { draw_line(b, x + 7 + i * 6, y + 5, x + 7 + i * 6, y + 9, 0xDDF7FF, 255); draw_line(b, x + 7 + i * 6, y + 25, x + 7 + i * 6, y + 29, 0xDDF7FF, 255); }
}

/* ------------------------------ SECURITY CENTER (app 14) ------------------------------ */
static void app_open_security(void) { app_on = 14; dirty = 1; }
static void sc_key(int k) { if (k == 27) app_on = 0; }
static void sc_click(int id) { if (id == 8900) ux_lock_now(); }
static void sc_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 28, y = wy + 56;
    heading(b, x, y, "Security center", pal[A_ACC]);
    row_card(b, x - 12, y + 38 - 7, ww - 32); text(b, x, y + 38, "Screen lock", pal[A_DIM]);
    text(b, x + 210, y + 38, pw_set ? "Password configured" : "Password not configured", pw_set ? pal[A_OK] : pal[A_WARN]);
    draw_rr(b, x, y + 62, 190, 34, 9, 0x205A88, 210); text(b, x + 20, y + 74, "Lock this session", pal[A_TXT]); hit_add(x, y + 62, 190, 34, 8900);
    row_card(b, x - 12, y + 126 - 7, ww - 32); text(b, x, y + 126, "User accounts", pal[A_DIM]); text(b, x + 210, y + 126, "Single local session", pal[A_TXT]);
    row_card(b, x - 12, y + 160 - 7, ww - 32); text(b, x, y + 160, "Per-user file permissions", pal[A_DIM]); text(b, x + 210, y + 160, "Not implemented", pal[A_WARN]);
    row_card(b, x - 12, y + 194 - 7, ww - 32); text(b, x, y + 194, "Network firewall", pal[A_DIM]); text(b, x + 210, y + 194, "None - client only (answers ping)", pal[A_WARN]);
    text(b, x, y + 238, "The lock screen protects the local session when a password is set.", pal[A_TXT]);
    text(b, x, y + 266, "This build does not claim multi-user isolation or firewall protection.", pal[A_WARN]);
    text(b, x, y + 304, "Use the Terminal command 'passwd' to configure a lock password.", pal[A_DIM]);
}
static void icon_security(u32 *b, int x, int y) {
    draw_rr(b, x + 6, y + 3, 22, 28, 8, 0x1C4B72, 255);
    draw_rr(b, x + 11, y + 10, 12, 11, 5, 0x6BD7FF, 255);
    draw_line(b, x + 17, y + 20, x + 17, y + 26, 0xF4C542, 255);
}

static void app_open_mines(void); static void ms_key(int); static void ms_draw(u32 *, u32); static void ms_click(int); static void ms_mouse(int, int, int); static void icon_mines(u32 *, int, int);
static void app_open_net(void); static void nt_key(int); static void nt_draw(u32 *, u32); static void nt_click(int); static void icon_net(u32 *, int, int);
static const App app_tab[] = {
    /* name          cmd          open               key      draw      click     mouse     icon */
    {"Calendar",   "calendar",   open_cal_default,  cal_key, cal_draw, cal_click, 0,        icon_cal, 640, 470},      /* app_on = 1 */
    {"Browser",     "browser",    open_browser_home, br_key,  br_draw,  br_click,  0,        icon_browser, 800, 520},  /* 2 */
    {"Paint",      "paint",      app_open_paint,    pt_key,  pt_draw,  pt_click,  pt_mouse, icon_paint, 760, 520},    /* 3 */
    {"Calculator", "calculator", app_open_calc,     kc_key,  kc_draw,  kc_click,  0,        icon_calc, 360, 470},     /* 4 */
    {"Clock",      "clockapp",   app_open_clock,    clk_key, clk_draw, 0,         0,        icon_clock, 560, 300},    /* 5 */
    {"Notes",      "notepad",    app_open_notes,    nb_key,  nb_draw,  nb_click,  0,        icon_notes, 620, 420},    /* 6 */
    {"Files",      "filemanager", app_open_files,   fa_key,  fa_draw,  fa_click,  0,        icon_files, 620, 420},    /* 7 */
    {"Snake",      "snakewin",   app_open_snake,    gs_key,  gs_draw,  0,         0,        icon_snake, 620, 470},    /* 8 */
    {"Settings",   "settings",   app_open_settings, st_key,  st_draw,  st_click,  0,        icon_settings, 720, 390}, /* 9 */
    {"Monitor",    "monitor",    app_open_monitor,  sm_key,  sm_draw,  0,         0,        icon_monitor, 620, 340}, /* 10 */
    {"Performance", "performance", app_open_performance, pc_key, pc_draw, 0, 0, icon_performance, 720, 440}, /* 11 */
    {"Task Manager", "tasks", app_open_tasks, tm_key, tm_draw, tm_click, 0, icon_tasks, 700, 470}, /* 12 */
    {"Device Manager", "devices", app_open_devices, dv_key, dv_draw, 0, 0, icon_devices, 680, 460}, /* 13 */
    {"Security Center", "security", app_open_security, sc_key, sc_draw, sc_click, 0, icon_security, 680, 430}, /* 14 */
    {"Network", "network", app_open_net, nt_key, nt_draw, nt_click, 0, icon_net, 700, 470}, /* 15 */
    {"Minesweeper", "mines", app_open_mines, ms_key, ms_draw, ms_click, ms_mouse, icon_mines, 520, 560}, /* 16 */
};
static int app_count(void) { return (int)(sizeof app_tab / sizeof app_tab[0]); }
static const App *app_at(int i) { return &app_tab[i]; }
static int app_launch(const char *name) {
    for (int i = 0; i < app_count(); i++) if (ci_eq(app_tab[i].cmd, name) || ci_eq(app_tab[i].name, name)) { app_tab[i].open(); return 1; }
    return 0;
}
static void app_list(void) {
    t_say(" Apps (type the name after 'apps' to open one):", A_DIM);
    for (int i = 0; i < app_count(); i++) { t_puts("  ", A_DIM); t_puts(app_tab[i].name, A_ACC); t_puts("  -  ", A_DIM); t_say(app_tab[i].cmd, A_TXT); }
}
