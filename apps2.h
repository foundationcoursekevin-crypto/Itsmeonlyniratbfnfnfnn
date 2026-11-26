/* apps2.h - Notepad, File Manager and Snake (windowed). Included from apps.h, before the registry. */

static void unique_name(int dir, const char *base, char *out) {
    for (int n = 1; n < 100; n++) {
        int p = 0; for (const char *s = base; *s && p < 12; s++) out[p++] = *s;
        if (n > 1) { if (n >= 10) out[p++] = (char)('0' + n / 10); out[p++] = (char)('0' + n % 10); }
        out[p++] = '.'; out[p++] = 't'; out[p++] = 'x'; out[p++] = 't'; out[p] = 0;
        if (fs_find(dir, out) < 0) return;
    }
    scpy(out, "note.txt", 24);
}

/* ------------------------------ NOTEPAD (app 6) ------------------------------ */
static char nb_[FSIZE]; static int nb_len, nb_node, nb_pos, nb_cols = 70, nb_scroll;
static void app_open_notes_node(int idx) {
    app_on = 6; nb_node = idx; nb_len = 0; nb_scroll = 0;
    if (idx > 0) { nb_len = nd[idx].size; if (nb_len > FSIZE - 1) nb_len = FSIZE - 1; memcpy(nb_, fdata[idx], (unsigned)nb_len); }
    nb_pos = nb_len; dirty = 1;
}
static void app_open_notes(void) { app_open_notes_node(0); }
static int nb_save(void) {
    if (nb_node <= 0) { char nm[24]; unique_name(cwd, "note", nm); int i = fs_new(cwd, nm, 0); if (i < 0) { notify("Disk full", 2); return 0; } nb_node = i; }
    memcpy(fdata[nb_node], nb_, (unsigned)nb_len); nd[nb_node].size = nb_len; notify("Saved", 1); return 1;
}
/* cursor row/col for a text position, wrapping at nb_cols (same rules as the drawing pass) */
static void nb_rc(int pos, int *row, int *col) {
    int r = 0, c = 0;
    for (int p = 0; p < pos; p++) { char ch = nb_[p]; if (ch == '\n') { r++; c = 0; continue; } if (c >= nb_cols) { r++; c = 0; } c++; }
    if (c >= nb_cols && !(pos < nb_len && nb_[pos] == '\n')) { r++; c = 0; }
    *row = r; *col = c;
}
static int nb_index(int row, int col) {                 /* text position closest to (row, col), never past the end of that row */
    int r = 0, c = 0, best = -1;
    for (int p = 0; p <= nb_len; p++) {
        int rr = r, cc = c; if (c >= nb_cols && !(p < nb_len && nb_[p] == '\n')) { rr = r + 1; cc = 0; }
        if (rr == row && cc <= col) best = p;
        if (rr > row || p == nb_len) break;
        char ch = nb_[p]; if (ch == '\n') { r++; c = 0; continue; } if (c >= nb_cols) { r++; c = 0; } c++;
    }
    return best < 0 ? nb_len : best;
}
static void nb_ins(char c) { if (nb_len >= FSIZE - 1) return; memmove(nb_ + nb_pos + 1, nb_ + nb_pos, (unsigned)(nb_len - nb_pos)); nb_[nb_pos++] = c; nb_len++; }
static void nb_key(int k) {
    if (k == 27) { if (nb_save()) app_on = 0; return; }
    if (k == '\b') { if (nb_pos > 0) { memmove(nb_ + nb_pos - 1, nb_ + nb_pos, (unsigned)(nb_len - nb_pos)); nb_pos--; nb_len--; } }
    else if (k == '\n') nb_ins('\n');
    else if (k == '\t') { for (int n = 0; n < 4; n++) nb_ins(' '); }
    else if (k == KEY_DEL) { if (nb_pos < nb_len) { memmove(nb_ + nb_pos, nb_ + nb_pos + 1, (unsigned)(nb_len - nb_pos - 1)); nb_len--; } }
    else if (k == KEY_HOME) { while (nb_pos > 0 && nb_[nb_pos - 1] != '\n') nb_pos--; }
    else if (k == KEY_END) { while (nb_pos < nb_len && nb_[nb_pos] != '\n') nb_pos++; }
    else if (k == KEY_PGUP || k == KEY_PGDN) { int r, c; nb_rc(nb_pos, &r, &c); int t = k == KEY_PGUP ? r - 10 : r + 10; if (t < 0) t = 0; nb_pos = nb_index(t, c); }
    else if (k == KEY_LEFT) { if (nb_pos > 0) nb_pos--; }
    else if (k == KEY_RIGHT) { if (nb_pos < nb_len) nb_pos++; }
    else if (k == KEY_UP || k == KEY_DOWN) { int r, c; nb_rc(nb_pos, &r, &c); int t = k == KEY_UP ? r - 1 : r + 1; if (t >= 0) nb_pos = nb_index(t, c); }
    else if (k >= 32 && k < 127) nb_ins((char)k);
}
static void nb_click(int id) { if (id == 700) nb_save(); }
static void nb_num(char *o, int *p, int v) { char t[12]; int n = 0; if (!v) t[n++] = '0'; while (v) { t[n++] = (char)('0' + v % 10); v /= 10; } while (n) o[(*p)++] = t[--n]; }
static void nb_draw(u32 *b, u32 ms) {
    int ax = wx + 20, ay = wy + 34 + 8, aw = ww - 40;
    char nm[24]; scpy(nm, nb_node > 0 ? nd[nb_node].name : "(new note)", 24);
    int crow, ccol; nb_rc(nb_pos, &crow, &ccol);
    char st[48]; int sp = 0; { const char *l = "Ln "; while (*l) st[sp++] = *l++; } { int ln = 1; for (int i = 0; i < nb_pos; i++) if (nb_[i] == '\n') ln++; nb_num(st, &sp, ln); }
    { const char *l = ", Col "; while (*l) st[sp++] = *l++; } { int lc = 0; for (int i = nb_pos; i > 0 && nb_[i - 1] != '\n'; i--) lc++; nb_num(st, &sp, lc + 1); }
    { const char *l = "   "; while (*l) st[sp++] = *l++; } nb_num(st, &sp, nb_len); { const char *l = " chars"; while (*l) st[sp++] = *l++; } st[sp] = 0;
    text(b, ax, ay + 8, nm, pal[A_ACC]); text(b, ax + 8 * 26, ay + 8, st, pal[A_DIM]);
    int bx = ax + aw - 80; draw_rr(b, bx, ay, 80, 30, 10, pal[A_ACC], hmix(80, 140, hov(bx, ay, 80, 30))); text(b, bx + 20, ay + 7, "Save", pal[A_TXT]); hit_add(bx, ay, 80, 30, 700);
    int ty = ay + 40, th = wh - 34 - 8 - 40 - 14; draw_rr(b, ax, ty, aw, th, 12, 0x000000, 80);
    int gut = 44, cols = (aw - 24 - gut) / 8, maxrows = (th - 16) / 16; if (cols < 4) cols = 4; if (maxrows < 1) maxrows = 1;
    nb_cols = cols; nb_rc(nb_pos, &crow, &ccol);
    if (crow < nb_scroll) nb_scroll = crow; if (crow >= nb_scroll + maxrows) nb_scroll = crow - maxrows + 1; if (nb_scroll < 0) nb_scroll = 0;
    int x0 = ax + 12, y0 = ty + 8, tx0 = x0 + gut;
    draw_rr(b, ax + 4, y0 + (crow - nb_scroll) * 16 - 1, aw - 8, 17, 4, 0xFFFFFF, 14);                     /* current line */
    draw_line(b, tx0 - 8, ty + 6, tx0 - 8, ty + th - 6, 0xFFFFFF, 24);                                    /* gutter divider */
    { int col = 0, row = 0, line = 1;
      if (row >= nb_scroll && row - nb_scroll < maxrows) { char n[8]; int q = 0; nb_num(n, &q, line); n[q] = 0; text(b, x0 + gut - 20 - 8 * q, y0, n, pal[A_DIM]); }
      for (int i = 0; i < nb_len; i++) {
          char c = nb_[i];
          if (c == '\n') { col = 0; row++; line++; if (row >= nb_scroll && row - nb_scroll < maxrows) { char n[8]; int q = 0; nb_num(n, &q, line); n[q] = 0; text(b, x0 + gut - 20 - 8 * q, y0 + (row - nb_scroll) * 16, n, pal[A_DIM]); } continue; }
          if (col >= cols) { col = 0; row++; }
          if (row >= nb_scroll && row - nb_scroll < maxrows) glyph_s(b, tx0 + col * 8, y0 + (row - nb_scroll) * 16, c, pal[A_TXT], 1);
          col++;
      } }
    if ((ms / 500) & 1) draw_rr(b, tx0 + ccol * 8, y0 + (crow - nb_scroll) * 16, 2, 15, 0, pal[A_ACC], 255);
}

/* ------------------------------ FILE MANAGER (app 7) ------------------------------ */
static int fa_dir, fa_n, fa_list[24];
static void app_open_files(void) { app_on = 7; fa_pc_view = 0; fa_dir = cwd; dirty = 1; }
static void desktop_open_this_pc(void) { app_on = 7; fa_pc_view = 1; fa_dir = 0; dirty = 1; }
static void desktop_open_drive(void) { app_on = 7; fa_pc_view = 0; fa_dir = 0; dirty = 1; }
static void desktop_open_node(int node) { app_on = 7; fa_pc_view = 0; fa_dir = (node >= 0 && node < NODES && nd[node].used && nd[node].isdir) ? node : desktop_node; dirty = 1; }
static void desktop_open_folder(int which) {
    app_on = 7; fa_pc_view = 0;
    if (which == 0) fa_dir = home_node;
    else if (which == 1) fa_dir = documents_node;
    else if (which == 2) fa_dir = downloads_node;
    else if (which == 3) fa_dir = pictures_node;
    else fa_dir = home_node;
    dirty = 1;
}
static void fa_key(int k) {
    if (k == 27) app_on = 0;
    else if (k == '\b') { if (!fa_pc_view && fa_dir > 0) fa_dir = nd[fa_dir].parent < 0 ? 0 : nd[fa_dir].parent; else fa_pc_view = 1; }
}
static void fa_click(int id) {
    if (id == 793) { folder_parent = fa_dir; folder_prompt = 1; new_folder_len = 0; new_folder_name[0] = 0; notify("Name your new folder", 0); dirty = 1; return; }
    if (id == 790) { char nm[24]; unique_name(fa_dir, "note", nm); int i = fs_new(fa_dir, nm, 0); if (i < 0) { notify("Disk full", 2); return; } app_open_notes_node(i); return; }
    if (id == 791) { if (fa_dir > 0) fa_dir = nd[fa_dir].parent < 0 ? 0 : nd[fa_dir].parent; else fa_pc_view = 1; return; }
    if (id == 792) { fa_pc_view = 0; fa_dir = 0; return; } /* open C: RAM disk */
    if (id >= 830 && id < 836) {
        static const int folders[6] = {0, 1, 2, 3, 4, 5};
        int k = id - 830;
        if (folders[k] == 0) fa_dir = desktop_node;
        else if (folders[k] == 1) fa_dir = documents_node;
        else if (folders[k] == 2) fa_dir = downloads_node;
        else if (folders[k] == 3) fa_dir = pictures_node;
        else if (folders[k] == 4) fa_dir = music_node;
        else fa_dir = videos_node;
        fa_pc_view = 0; return;
    }
    if (id >= 800 && id < 824) { int k = id - 800; if (k < fa_n) { int i = fa_list[k]; if (nd[i].isdir) fa_dir = i; else app_open_notes_node(i); } }
}
static void fa_draw(u32 *b, u32 ms) {
    (void)ms; int ax = wx + 20, ay = wy + 34 + 8, aw = ww - 40;
    if (fa_pc_view) {
        static const char *names[6] = {"Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos"};
        int cols2 = aw >= 500 ? 3 : 2, gap = 8, cw = (aw - gap * (cols2 - 1)) / cols2;
        text(b, ax, ay + 4, "This PC", pal[A_ACC]);
        text(b, ax, ay + 24, "Folders", pal[A_DIM]);
        for (int k = 0; k < 6; k++) {
            int col = k % cols2, row = k / cols2, bx = ax + col * (cw + gap), by = ay + 42 + row * 42;
            draw_rr(b, bx, by, cw, 36, 9, 0xFFFFFF, hmix(28, 74, hov(bx, by, cw, 36)));
            draw_rr(b, bx + 8, by + 11, 18, 13, 4, k == 2 ? 0x39B9A5 : 0x7D9DE8, 255);
            draw_rr(b, bx + 10, by + 8, 9, 6, 3, 0xFFE2A0, 255);
            text(b, bx + 34, by + 13, names[k], pal[A_TXT]);
            hit_add(bx, by, cw, 36, 830 + k);
        }
        int drive_y = ay + 132;
        text(b, ax, drive_y, "Devices and drives", pal[A_DIM]);
        draw_rr(b, ax, drive_y + 18, aw, 58, 11, 0xFFFFFF, hmix(30, 74, hov(ax, drive_y + 18, aw, 58)));
        draw_rr(b, ax + 14, drive_y + 31, 42, 28, 7, 0xAFC9E8, 245);
        draw_rr(b, ax + 18, drive_y + 34, 34, 20, 4, 0x314A70, 255);
        draw_rr(b, ax + 23, drive_y + 49, 20, 3, 1, 0xAFC9E8, 210);
        draw_rr(b, ax + 46, drive_y + 48, 4, 4, 2, 0x7CFFC5, 255);
        text(b, ax + 70, drive_y + 29, "PixelBeeOS RAM Disk (C:)", pal[A_TXT]);
        text(b, ax + 70, drive_y + 48, "Temporary storage  |  cleared at reboot", pal[A_DIM]);
        hit_add(ax, drive_y + 18, aw, 58, 792);
        int hw_y = drive_y + 88;
        draw_rr(b, ax, hw_y, aw, 34, 9, 0xFFFFFF, 20);
        text(b, ax + 10, hw_y + 7, "Physical drives: storage driver not loaded", pal[A_DIM]);
        text(b, ax, hw_y + 42, "This PC shows available PixelBeeOS storage. Physical laptop partitions", pal[A_DIM]);
        text(b, ax, hw_y + 58, "will appear after an ATA / AHCI / NVMe driver is added.", pal[A_DIM]);
        return;
    }
    char p[200]; node_path(fa_dir, p);
    text(b, ax, ay + 8, p, pal[A_ACC]);
    int bx = ax + aw - 100; draw_rr(b, bx, ay, 100, 30, 10, 0xFFFFFF, hmix(48, 92, hov(bx, ay, 100, 30))); text(b, bx + 12, ay + 7, "New file", pal[A_TXT]); hit_add(bx, ay, 100, 30, 790);
    bx -= 122; draw_rr(b, bx, ay, 112, 30, 10, 0xB8E8FF, hmix(78, 116, hov(bx, ay, 112, 30))); text(b, bx + 10, ay + 7, "+ New Folder", 0xFFFFFF); hit_add(bx, ay, 112, 30, 793);
    bx -= 68; draw_rr(b, bx, ay, 60, 30, 10, 0xFFFFFF, hmix(40, 90, hov(bx, ay, 60, 30))); text(b, bx + 18, ay + 7, "Up", pal[A_TXT]); hit_add(bx, ay, 60, 30, 791);
    fa_n = 0; for (int i = 1; i < NODES && fa_n < 24; i++) if (nd[i].used && nd[i].parent == fa_dir) fa_list[fa_n++] = i;
    int ry = ay + 44, rh = 26, maxr = (wh - 34 - 8 - 44 - 14) / rh; if (maxr > 24) maxr = 24;
    if (fa_n == 0) text(b, ax + 8, ry + 6, "(empty folder - use New file or New Folder)", pal[A_DIM]);
    for (int k = 0; k < fa_n && k < maxr; k++) {
        int i = fa_list[k], y = ry + k * rh;
        draw_rr(b, ax, y, aw, rh - 3, 8, 0xFFFFFF, hmix(24, 70, hov(ax, y, aw, rh - 3)));
        text(b, ax + 10, y + 4, nd[i].isdir ? "[D]" : "[F]", nd[i].isdir ? pal[A_ACC] : pal[A_DIM]); text(b, ax + 50, y + 4, nd[i].name, pal[A_TXT]);
        if (!nd[i].isdir) { char nb2[12]; fmt_base((u32)nd[i].size, 10, nb2); int l = slen(nb2); text(b, ax + aw - 90 - 8 * l, y + 4, nb2, pal[A_DIM]); text(b, ax + aw - 80, y + 4, "bytes", pal[A_DIM]); }
        hit_add(ax, y, aw, rh - 3, 800 + k);
    }
}

/* ------------------------------ SNAKE WINDOW (app 8) ------------------------------ */
#define GS_W 28
#define GS_H 18
static int gs_x[GS_W * GS_H], gs_y[GS_W * GS_H], gs_l, gs_dx, gs_dy, gs_ndx, gs_ndy, gs_fx, gs_fy, gs_sc, gs_over, gs_best; static u32 gs_last;
static void gs_food(void) {
    for (int t = 0; t < 2000; t++) { gs_fx = (int)(rnd() % GS_W); gs_fy = (int)(rnd() % GS_H); int ok = 1;
        for (int i = 0; i < gs_l; i++) if (gs_x[i] == gs_fx && gs_y[i] == gs_fy) ok = 0; if (ok) return; }
}
static void gs_start(u32 ms) {
    gs_l = 4; for (int i = 0; i < gs_l; i++) { gs_x[i] = GS_W / 2 - i; gs_y[i] = GS_H / 2; }
    gs_dx = gs_ndx = 1; gs_dy = gs_ndy = 0; gs_sc = 0; gs_over = 0; gs_last = ms; rng ^= ms; gs_food();
}
static void app_open_snake(void) { app_on = 8; gs_start(cur_ms); dirty = 1; }
static void gs_key(int k) {
    if (k == 27) { app_on = 0; return; }
    if (gs_over) { if (k == '\n' || k == ' ') gs_start(cur_ms); return; }
    if ((k == KEY_UP || k == 'w' || k == 'W') && gs_dy != 1) { gs_ndx = 0; gs_ndy = -1; }
    else if ((k == KEY_DOWN || k == 's' || k == 'S') && gs_dy != -1) { gs_ndx = 0; gs_ndy = 1; }
    else if ((k == KEY_LEFT || k == 'a' || k == 'A') && gs_dx != 1) { gs_ndx = -1; gs_ndy = 0; }
    else if ((k == KEY_RIGHT || k == 'd' || k == 'D') && gs_dx != -1) { gs_ndx = 1; gs_ndy = 0; }
}
static void gs_step(void) {
    gs_dx = gs_ndx; gs_dy = gs_ndy; int nx = gs_x[0] + gs_dx, ny = gs_y[0] + gs_dy;
    if (nx < 0 || ny < 0 || nx >= GS_W || ny >= GS_H) gs_over = 1;
    for (int i = 0; i < gs_l - 1 && !gs_over; i++) if (gs_x[i] == nx && gs_y[i] == ny) gs_over = 1;
    if (gs_over) { if (gs_sc > gs_best) gs_best = gs_sc; notify("Game over", 2); return; }
    int tx = gs_x[gs_l - 1], ty = gs_y[gs_l - 1];
    for (int i = gs_l - 1; i > 0; i--) { gs_x[i] = gs_x[i - 1]; gs_y[i] = gs_y[i - 1]; }
    gs_x[0] = nx; gs_y[0] = ny;
    if (nx == gs_fx && ny == gs_fy) {
        if (gs_l < GS_W * GS_H - 1) { gs_x[gs_l] = tx; gs_y[gs_l] = ty; gs_l++; gs_sc++; gs_food(); } else gs_over = 1;
    }
}
static void gs_draw(u32 *b, u32 ms) {
    app_fast = 1;
    if (!gs_over) { int iv = 140 - gs_sc * 3; if (iv < 70) iv = 70; if (ms - gs_last > 600) gs_last = ms; while (!gs_over && ms - gs_last >= (u32)iv) { gs_step(); gs_last += (u32)iv; } }
    int cs = (ww - 40) / GS_W, cs2 = (wh - 34 - 70) / GS_H; if (cs2 < cs) cs = cs2; if (cs < 6) cs = 6;
    int bw = cs * GS_W, bh = cs * GS_H, bx = wx + (ww - bw) / 2, by = wy + 34 + 52;
    char s[40]; int p = 0; const char *h = "Score "; while (*h) s[p++] = *h++; { char nb2[12]; fmt_base((u32)gs_sc, 10, nb2); for (char *q = nb2; *q; q++) s[p++] = *q; }
    h = "   Best "; while (*h) s[p++] = *h++; { char nb2[12]; fmt_base((u32)gs_best, 10, nb2); for (char *q = nb2; *q; q++) s[p++] = *q; } s[p] = 0;
    text(b, bx, wy + 34 + 14, s, pal[A_ACC]); text(b, bx + bw - 8 * 24, wy + 34 + 14, "arrows/WASD  Esc = close", pal[A_DIM]);
    draw_rr(b, bx - 4, by - 4, bw + 8, bh + 8, 10, 0x000000, 110);
    draw_rr(b, bx + gs_fx * cs + 2, by + gs_fy * cs + 2, cs - 4, cs - 4, cs / 2, 0xFF5470, 255);
    for (int i = gs_l - 1; i >= 0; i--) draw_rr(b, bx + gs_x[i] * cs + 1, by + gs_y[i] * cs + 1, cs - 2, cs - 2, cs / 4, i ? 0x3BB273 : 0x9FE8FF, 255);
    if (gs_over) { draw_rr(b, bx + bw / 2 - 150, by + bh / 2 - 30, 300, 60, 14, 0x000000, 170); text(b, bx + bw / 2 - 8 * 12, by + bh / 2 - 8, "GAME OVER - Enter = retry", 0xFFFFFF); }
}
static void icon_notes(u32 *b, int ix, int iy) {
    draw_rr(b, ix + 4, iy, 26, 34, 6, 0xFFF6C9, 255); for (int k = 0; k < 4; k++) draw_line(b, ix + 9, iy + 10 + k * 6, ix + 25, iy + 10 + k * 6, 0x8A7A3A, 255);
    draw_rr(b, ix + 4, iy, 26, 6, 3, 0xFFB454, 255);
}
static void icon_files(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy + 6, 34, 26, 6, 0xFFB454, 255); draw_rr(b, ix, iy + 2, 15, 8, 3, 0xFFB454, 255); draw_rr(b, ix, iy + 13, 34, 19, 6, 0xFFD27A, 255);
}
static void icon_snake(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 10, 0x12301F, 250);
    for (int k = 0; k < 5; k++) draw_rr(b, ix + 5 + k * 5, iy + (k % 2 ? 12 : 18), 6, 6, 3, k == 4 ? 0x9FE8FF : 0x3BB273, 255);
    draw_rr(b, ix + 25, iy + 6, 6, 6, 3, 0xFF5470, 255);
}

/* ------------------------------ PCI scan (lspci / netcards) ------------------------------ */
static u32 pci_rd(int bus, int dev, int fn, int off) { outl(0xCF8, 0x80000000u | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) | ((u32)off & 0xFC)); return inl(0xCFC); }
static void hex4(u32 v, char *o) { for (int i = 0; i < 4; i++) { u32 d = (v >> (12 - 4 * i)) & 15; o[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); } o[4] = 0; }
static const char *pci_class_name(int cl, int sc) {
    if (cl == 2) return sc == 0 ? "Ethernet (wired network)" : sc == 0x80 ? "Network - other (often WIFI)" : "Network";
    if (cl == 6) return "Bridge"; if (cl == 3) return "Display / graphics"; if (cl == 1) return sc == 1 ? "IDE disk controller" : sc == 6 ? "SATA disk controller" : "Storage";
    if (cl == 4) return "Multimedia / audio"; if (cl == 12) return sc == 3 ? "USB controller" : "Serial bus"; if (cl == 0x0D) return "Wireless controller"; return "Other device";
}
static void cmd_lspci(int net_only) {
    int found = 0, nets = 0;
    for (int bus = 0; bus < 8; bus++) for (int dev = 0; dev < 32; dev++) for (int fn = 0; fn < 8; fn++) {
        u32 id = pci_rd(bus, dev, fn, 0); if ((id & 0xFFFF) == 0xFFFF) { if (fn == 0) break; continue; }
        u32 cls = pci_rd(bus, dev, fn, 8); int cl = (int)(cls >> 24), sc = (int)((cls >> 16) & 255);
        int is_net = cl == 2 || cl == 0x0D; if (is_net) nets++;
        if (net_only && !is_net) { if (fn == 0 && !(pci_rd(bus, dev, 0, 0xC) & 0x800000)) break; continue; }
        char v[5], d[5]; hex4(id & 0xFFFF, v); hex4(id >> 16, d); found++;
        t_puts(" ", A_DIM); t_puts(v, A_ACC); t_puts(":", A_DIM); t_puts(d, A_ACC); t_puts("  ", A_DIM); t_say(pci_class_name(cl, sc), is_net ? A_OK : A_TXT);
        if (fn == 0 && !(pci_rd(bus, dev, 0, 0xC) & 0x800000)) break;
    }
    if (!found) t_say(net_only ? " No network cards found (wifi on USB or an unsupported bus would not show here)." : " No PCI devices found.", A_WARN);
    else if (net_only) t_say(" Send me these ID numbers (vendor:device) - that tells me which driver this chip needs.", A_DIM);
    (void)nets;
}
