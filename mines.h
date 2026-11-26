/* PixelBeeOS 3.2 - Minesweeper (app 16). Mouse: left = reveal, right = flag. Keys: arrows move, Space/Enter reveal, F flag, R restart, Esc close.
 * The first reveal is always safe. 9x9 board, 10 mines. */
#define MS_N 9
#define MS_MINES 10
static u8 ms_mine[MS_N * MS_N], ms_open[MS_N * MS_N], ms_flag[MS_N * MS_N];
static int ms_state, ms_placed, ms_sel = 40, ms_left, ms_x0, ms_y0, ms_cs; static u32 ms_t0, ms_tend, ms_seed;
static u32 ms_rand(void) { ms_seed ^= ms_seed << 13; ms_seed ^= ms_seed >> 17; ms_seed ^= ms_seed << 5; return ms_seed; }
static void ms_reset(void) {
    memset(ms_mine, 0, sizeof ms_mine); memset(ms_open, 0, sizeof ms_open); memset(ms_flag, 0, sizeof ms_flag);
    ms_state = 0; ms_placed = 0; ms_left = MS_MINES; ms_t0 = ms_tend = 0; ms_sel = (MS_N * MS_N) / 2; if (!ms_seed) ms_seed = 2463534242u;
}
static void app_open_mines(void) { app_on = 16; ms_reset(); dirty = 1; }
static int ms_adj(int i) {
    int x = i % MS_N, y = i / MS_N, n = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int xx = x + dx, yy = y + dy; if ((dx || dy) && xx >= 0 && yy >= 0 && xx < MS_N && yy < MS_N && ms_mine[yy * MS_N + xx]) n++; }
    return n;
}
static void ms_check_win(void) {
    int closed = 0; for (int i = 0; i < MS_N * MS_N; i++) if (!ms_open[i]) closed++;
    if (closed == MS_MINES && !ms_state) { ms_state = 2; ms_tend = cur_ms; ms_left = 0; for (int i = 0; i < MS_N * MS_N; i++) if (ms_mine[i]) ms_flag[i] = 1; notify("Minesweeper: you win!", 1); }
}
static void ms_reveal(int i) {
    if (ms_state || ms_open[i] || ms_flag[i]) return;
    if (!ms_placed) {                                         /* place mines now so the first click is safe */
        ms_placed = 1; ms_t0 = cur_ms; ms_seed ^= cur_ms * 2654435761u + 1; if (!ms_seed) ms_seed = 1;
        for (int n = 0; n < MS_MINES; ) { int c = (int)(ms_rand() % (MS_N * MS_N)); if (c != i && !ms_mine[c]) { ms_mine[c] = 1; n++; } }
    }
    if (ms_mine[i]) { ms_state = 1; ms_tend = cur_ms; ms_open[i] = 1; for (int k = 0; k < MS_N * MS_N; k++) if (ms_mine[k]) ms_open[k] = 1; notify("Boom! Press R to retry", 2); return; }
    int stack[MS_N * MS_N], sp = 0; stack[sp++] = i; ms_open[i] = 1;
    while (sp) {
        int c = stack[--sp]; if (ms_adj(c)) continue;
        int x = c % MS_N, y = c / MS_N;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy; if (xx < 0 || yy < 0 || xx >= MS_N || yy >= MS_N) continue;
            int j = yy * MS_N + xx; if (ms_open[j] || ms_flag[j]) continue; ms_open[j] = 1; stack[sp++] = j;
        }
    }
    ms_check_win();
}
static void ms_toggle_flag(int i) {
    if (ms_state || ms_open[i]) return;
    ms_flag[i] = !ms_flag[i]; ms_left += ms_flag[i] ? -1 : 1;
}
static void ms_key(int k) {
    if (k == 27) { app_on = 0; return; }
    if (k == 'r' || k == 'R') { ms_reset(); return; }
    if (k == KEY_LEFT && ms_sel % MS_N) ms_sel--;
    else if (k == KEY_RIGHT && ms_sel % MS_N < MS_N - 1) ms_sel++;
    else if (k == KEY_UP && ms_sel >= MS_N) ms_sel -= MS_N;
    else if (k == KEY_DOWN && ms_sel < MS_N * (MS_N - 1)) ms_sel += MS_N;
    else if (k == ' ' || k == '\n') ms_reveal(ms_sel);
    else if (k == 'f' || k == 'F') ms_toggle_flag(ms_sel);
}
static void ms_click(int id) { if (id >= 5000 && id < 5000 + MS_N * MS_N) { ms_sel = id - 5000; ms_reveal(ms_sel); } else if (id == 5100) ms_reset(); }
static void ms_mouse(int btn, int x, int y) {                  /* right button: flag the cell under the pointer */
    static int prev;
    if ((btn & 2) && !(prev & 2) && ms_cs > 0 && x >= ms_x0 && y >= ms_y0 && x < ms_x0 + ms_cs * MS_N && y < ms_y0 + ms_cs * MS_N) {
        ms_sel = (y - ms_y0) / ms_cs * MS_N + (x - ms_x0) / ms_cs; ms_toggle_flag(ms_sel); dirty = 1;
    }
    prev = btn;
}
static void ms_num(char *o, int *p, u32 v) { char t[12]; fmt_base(v, 10, t); for (char *q = t; *q; q++) o[(*p)++] = *q; }
static void ms_draw(u32 *b, u32 ms) {
    (void)ms; static const u32 nc[9] = {0, 0x5AB4FF, 0x4CD964, 0xFF6B6B, 0x7D8CFF, 0xFF9F43, 0x3CD7C8, 0xE6E6E6, 0x9A9AA0};
    int cs = (ww - 40) / MS_N, cs2 = (wh - 34 - 90) / MS_N; if (cs2 < cs) cs = cs2; if (cs < 12) cs = 12;
    int bw = cs * MS_N, bx = wx + (ww - bw) / 2, by = wy + 34 + 62; ms_x0 = bx; ms_y0 = by; ms_cs = cs;
    u32 t = ms_state ? ms_tend - ms_t0 : (ms_placed ? cur_ms - ms_t0 : 0);
    char s[48]; int p = 0; { const char *h = "Mines "; while (*h) s[p++] = *h++; } ms_num(s, &p, (u32)(ms_left < 0 ? 0 : ms_left));
    { const char *h = "    Time "; while (*h) s[p++] = *h++; } ms_num(s, &p, t / 1000); s[p++] = 's'; s[p] = 0;
    text(b, bx, wy + 34 + 14, s, pal[A_ACC]);
    int rx = bx + bw - 74; draw_rr(b, rx, wy + 34 + 8, 74, 28, 9, pal[A_ACC], hmix(80, 140, hov(rx, wy + 34 + 8, 74, 28))); text(b, rx + 11, wy + 34 + 15, "Restart", pal[A_TXT]); hit_add(rx, wy + 34 + 8, 74, 28, 5100);
    text(b, bx, wy + 34 + 40, ms_state == 1 ? "Boom! Restart or press R" : ms_state == 2 ? "Cleared! Press R to play again" : "Click = reveal   Right-click / F = flag", ms_state == 1 ? pal[A_ERR] : ms_state == 2 ? pal[A_OK] : pal[A_DIM]);
    draw_rr(b, bx - 6, by - 6, bw + 12, bw + 12, 12, 0x000000, 110);
    for (int i = 0; i < MS_N * MS_N; i++) {
        int cx = bx + (i % MS_N) * cs, cy = by + (i / MS_N) * cs;
        if (ms_open[i]) {
            draw_rr(b, cx + 1, cy + 1, cs - 2, cs - 2, 4, ms_mine[i] ? 0x7A1F2B : 0x2A2F3A, 255);
            if (ms_mine[i]) { draw_rr(b, cx + cs / 2 - cs / 5, cy + cs / 2 - cs / 5, cs * 2 / 5, cs * 2 / 5, cs / 5, 0x101010, 255); }
            else { int a = ms_adj(i); if (a) { char d[2] = {(char)('0' + a), 0}; text(b, cx + cs / 2 - 4, cy + cs / 2 - 8, d, nc[a]); } }
        } else {
            draw_rr(b, cx + 1, cy + 1, cs - 2, cs - 2, 4, hot(cx, cy, cs, cs) ? 0x5A6A86 : 0x44526B, 255);
            draw_line(b, cx + 3, cy + 2, cx + cs - 4, cy + 2, 0xFFFFFF, 50);
            if (ms_flag[i]) { draw_line(b, cx + cs / 2, cy + cs / 4, cx + cs / 2, cy + cs - cs / 4, 0xEEEEEE, 255); draw_rr(b, cx + cs / 2, cy + cs / 4, cs / 3, cs / 4, 2, 0xFF4D5E, 255); }
            hit_add(cx, cy, cs, cs, 5000 + i);
        }
        if (i == ms_sel && !ms_state) draw_rr(b, cx, cy, cs, cs, 5, pal[A_ACC], 60);
    }
}
static void icon_mines(u32 *b, int ix, int iy) {
    draw_rr(b, ix, iy, 34, 34, 10, 0x2B3550, 250);
    draw_rr(b, ix + 8, iy + 8, 18, 18, 9, 0x0E0E12, 255);
    for (int k = 0; k < 4; k++) { int dx = k < 2 ? 0 : 1, dy = k & 1; draw_line(b, ix + 17 - 11 + dx * 22 * 0, iy + 17, ix + 17 + (k == 0 ? -12 : k == 1 ? 12 : 0), iy + 17 + (k == 2 ? -12 : k == 3 ? 12 : 0), 0x0E0E12, 255); (void)dy; }
    draw_rr(b, ix + 13, iy + 12, 5, 5, 2, 0xFFFFFF, 160);
}
