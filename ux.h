/* ux.h - PixelBeeOS 2.2 desktop features that fit a small kernel:
 *   lock screen + password (salted, stretched hash, wrong-try delay, auto-lock), Mission Control (window overview),
 *   window snapping (drag to an edge/corner, Super+Left/Right), hot corners, Spotlight calculator.
 * HONEST LIMITS: there is no disk driver, so the password and settings live in RAM and reset at reboot; the hash is a
 * home-made stretched hash (not a vetted password KDF); there is no memory isolation between programs. This is a
 * screen lock against casual access, NOT disk encryption or a hardened security boundary. */

/* ---------- password hashing ---------- */
static u32 ux_mix(u32 h, u32 v) { h ^= v; h *= 16777619u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
static void ux_hash(const char *pw, u32 salt, u32 out[2]) {
    u32 a = 2166136261u ^ salt, b = 0x9E3779B9u + salt;
    for (u32 r = 0; r < 30000; r++) for (const char *p = pw; *p; p++) { a = ux_mix(a, (u32)(u8)*p + r); b = ux_mix(b ^ a, (u32)(u8)*p * 31u + (r << 3)); }
    out[0] = a; out[1] = b;
}
static int ux_check(const char *pw) { u32 h[2]; ux_hash(pw, pw_salt, h); return ((h[0] ^ pw_h[0]) | (h[1] ^ pw_h[1])) == 0; }
static void ux_buf_clear(void) { lock_len = 0; lock_buf[0] = 0; }

/* ---------- lock screen / set password ---------- */
static void ux_lock_now(void) {
    if (!pw_set) { notify("Set a password first: type  passwd", 2); return; }
    locked = 1; ux_buf_clear(); lock_msg[0] = 0; launcher_open = launchpad_open = control_open = 0; mc_open = 0;
    desk_menu_open = 0; gallery_open = 0; folder_prompt = 0; drag_app = 0; dirty = 1;
}
static void ux_passwd_start(void) {
    pw_ui = 1; pw_stage = pw_set ? 3 : 1; ux_buf_clear(); lock_msg[0] = 0; launcher_open = launchpad_open = control_open = 0; mc_open = 0; dirty = 1;
}
static void ux_setmsg(const char *m) { int i = 0; while (m[i] && i < 46) { lock_msg[i] = m[i]; i++; } lock_msg[i] = 0; }
static void ux_lock_enter(void) {
    if (cur_ms < lock_until) { ux_setmsg("Too many tries - please wait"); ux_buf_clear(); return; }
    if (locked && !pw_ui) {
        if (ux_check(lock_buf)) { locked = 0; lock_fail = 0; lock_msg[0] = 0; last_input_ms = cur_ms; notify("Unlocked", 1); }
        else {
            lock_fail++; u32 d = lock_fail >= 3 ? (5000u << (lock_fail - 3 > 6 ? 6 : lock_fail - 3)) : 0; lock_until = cur_ms + d;
            ux_setmsg(d ? "Wrong password - delay added" : "Wrong password");
        }
        ux_buf_clear(); return;
    }
    if (pw_stage == 3) {                                           /* changing it: prove you know the current one */
        if (ux_check(lock_buf)) { pw_stage = 1; ux_setmsg("Now type a new password"); }
        else { lock_fail++; lock_until = cur_ms + (lock_fail >= 3 ? 5000u : 0); ux_setmsg("Wrong current password"); }
        ux_buf_clear(); return;
    }
    if (pw_stage == 1) {
        if (lock_len < 4) { ux_setmsg("Use at least 4 characters"); return; }
        for (int i = 0; i <= lock_len; i++) pw_tmp[i] = lock_buf[i];
        pw_stage = 2; ux_buf_clear(); ux_setmsg("Type it again to confirm"); return;
    }
    int same = 1; for (int i = 0; ; i++) { if (pw_tmp[i] != lock_buf[i]) { same = 0; break; } if (!pw_tmp[i]) break; }
    if (!same) { pw_stage = 1; ux_buf_clear(); ux_setmsg("Did not match - start again"); return; }
    pw_salt = rng ^ (cur_ms * 2654435761u) ^ 0xA5C3E1F7u; ux_hash(pw_tmp, pw_salt, pw_h);
    for (int i = 0; i < 32; i++) pw_tmp[i] = 0;                    /* do not keep the clear text around */
    pw_set = 1; pw_ui = 0; ux_buf_clear(); lock_msg[0] = 0; last_input_ms = cur_ms; dirty = 1;
    notify("Password set. Super+L locks the screen", 1);
}
static void ux_text_big(u32 *b, int x, int y, const char *s, u32 col, int sc) {
    for (; *s; s++, x += 8 * sc) {
        int c = *s; if (c < 32 || c > 126) c = '?'; const u8 *g = font[c - 32];
        for (int j = 0; j < 16; j++) for (int i = 0; i < 8; i++) {
            u8 a = g[j * 8 + i]; if (!a) continue;
            for (int sy = 0; sy < sc; sy++) { int yy = y + j * sc + sy; if (yy < 0 || yy >= (int)H) continue;
                for (int sx = 0; sx < sc; sx++) { int xx = x + i * sc + sx; if (xx < 0 || xx >= (int)W) continue; u32 *p = &b[yy * W + xx]; *p = mix(*p, col, a); } }
        }
    }
}
static void draw_lock_screen(u32 *b, u32 ms, int hh, int mm) {
    if (!(locked || pw_ui)) return;
    draw_rr(b, 0, 0, (int)W, (int)H, 0, 0x0E0E10, 255);
    char tm[6] = { (char)('0' + (hh / 10) % 10), (char)('0' + hh % 10), ':', (char)('0' + (mm / 10) % 10), (char)('0' + mm % 10), 0 };
    int sc = (int)W >= 1100 ? 8 : 5, cx = (int)W / 2, y0 = (int)H / 6;
    ux_text_big(b, cx - 5 * 4 * sc, y0, tm, pal[A_TXT], sc);
    const char *title = locked && !pw_ui ? "PixelBeeOS is locked" : (pw_stage == 3 ? "Enter your current password" : (pw_stage == 2 ? "Confirm the new password" : "Choose a password"));
    text(b, cx - 4 * (int)slen(title), y0 + 16 * sc + 28, title, pal[A_DIM]);
    int fw = 320, fx = cx - fw / 2, fy = y0 + 16 * sc + 62;
    draw_rr(b, fx - 1, fy - 1, fw + 2, 46, 15, 0x48484A, 255); draw_rr(b, fx, fy, fw, 44, 14, 0x2C2C2E, 255);
    for (int i = 0; i < lock_len && i < 20; i++) draw_rr(b, fx + 16 + i * 14, fy + 17, 10, 10, 5, pal[A_TXT], 255);
    if ((ms / 500) % 2 == 0 && lock_len < 20) draw_rr(b, fx + 16 + lock_len * 14, fy + 12, 2, 20, 1, pal[A_ACC], 255);
    u32 wait = cur_ms < lock_until ? (lock_until - cur_ms + 999) / 1000 : 0;
    if (wait) { char wb[40] = "Try again in "; int n = 13; if (wait >= 100) wb[n++] = (char)('0' + wait / 100 % 10); if (wait >= 10) wb[n++] = (char)('0' + wait / 10 % 10); wb[n++] = (char)('0' + wait % 10); wb[n++] = 's'; wb[n] = 0; text(b, cx - 4 * (int)slen(wb), fy + 62, wb, pal[A_WARN]); }
    else if (lock_msg[0]) text(b, cx - 4 * (int)slen(lock_msg), fy + 62, lock_msg, pal[A_ERR]);
    const char *hint = pw_ui ? "Enter = continue   Esc = cancel" : "Type your password and press Enter";
    text(b, cx - 4 * (int)slen(hint), (int)H - 60, hint, pal[A_DIM]);
}

/* ---------- window snapping ---------- */
static void ux_snap(int i, int mode) {                             /* 0 left, 1 right, 2 top-left, 3 top-right, 4 bottom-left, 5 bottom-right */
    Win *w = &win_[i]; int y0 = 10, y1 = dky - 8, hw = (int)W / 2, hh = (y1 - y0) / 2;
    if (w->max) { w->max = 0; }
    switch (mode) {
        case 0: w->x = 0; w->y = y0; w->w = hw; w->h = y1 - y0; break;
        case 1: w->x = hw; w->y = y0; w->w = (int)W - hw; w->h = y1 - y0; break;
        case 2: w->x = 0; w->y = y0; w->w = hw; w->h = hh; break;
        case 3: w->x = hw; w->y = y0; w->w = (int)W - hw; w->h = hh; break;
        case 4: w->x = 0; w->y = y0 + hh; w->w = hw; w->h = hh; break;
        default: w->x = hw; w->y = y0 + hh; w->w = (int)W - hw; w->h = hh; break;
    }
    dirty = 1;
}
static void ux_snap_top(int mode) { if (nwin > 0 && !win_[nwin - 1].min) ux_snap(nwin - 1, mode); }
static void ux_snap_release(int app) {                             /* called when a window drag ends */
    int i = win_find(app); if (i < 0 || win_[i].max) return;
    int l = mx <= 2, r = mx >= (int)W - 3, ct = my < 90, cb = my > (int)H - 90;
    if (my < 36 && !l && !r) win_toggle_max(app);                  /* let go on the menu bar: maximise */
    else if (l || r) ux_snap(i, l ? (ct ? 2 : (cb ? 4 : 0)) : (ct ? 3 : (cb ? 5 : 1)));
}

/* ---------- Mission Control ---------- */
static void ux_mc_click(int id) {
    if (id == 4099) { mc_open = 0; dirty = 1; return; }
    if (id == 4090) { mc_open = 0; focus_terminal(); dirty = 1; return; }
    int k = id - 4000; if (k < 0 || k >= nwin) { mc_open = 0; return; }
    Win t = win_[k]; t.min = 0; t.t0 = cur_ms; win_[k] = t; win_raise(k); term_focus = 0; app_on = t.app; mc_open = 0; dirty = 1;
}
static void draw_mission_control(u32 *b) {
    if (!mc_open) return;
    draw_rr(b, 0, 0, (int)W, (int)H, 0, 0x141416, 255);
    hit_add(0, 0, (int)W, (int)H, 4099);
    const char *t1 = "Mission Control"; text(b, ((int)W - 8 * (int)slen(t1)) / 2, 60, t1, pal[A_TXT]);
    const char *t2 = "Click a window to bring it forward   |   Esc or F3 to close"; text(b, ((int)W - 8 * (int)slen(t2)) / 2, 84, t2, pal[A_DIM]);
    int n = nwin + (terminal_visible ? 1 : 0), cw = 280, ch = 170, gap = 24, cols = n < 4 ? (n ? n : 1) : 4;
    if (!n) { const char *e = "No open windows"; text(b, ((int)W - 8 * (int)slen(e)) / 2, (int)H / 2, e, pal[A_DIM]); return; }
    int rows = (n + cols - 1) / cols, gx = ((int)W - (cols * cw + (cols - 1) * gap)) / 2, gy = 130 + (((int)H - 130 - 90) - (rows * ch + (rows - 1) * gap)) / 2; if (gy < 118) gy = 118;
    for (int k = 0; k < n; k++) {
        int term = terminal_visible && k == n - 1, wi = term ? -1 : k, col = k % cols, row = k / cols, x = gx + col * (cw + gap), y = gy + row * (ch + gap);
        int hv = hot(x, y, cw, ch), isfocus = term ? term_focus : (wi == nwin - 1 && !term_focus && !win_[wi].min);
        draw_rr(b, x - 2, y - 2, cw + 4, ch + 4, 18, isfocus ? pal[A_ACC] : 0x48484A, 255); draw_rr(b, x, y, cw, ch, 16, hv ? 0x3A3A3C : 0x2C2C2E, 255);
        draw_rr(b, x + 14, y + 14, cw - 28, ch - 56, 8, 0x1C1C1E, 255); draw_rr(b, x + 14, y + 14, cw - 28, 12, 8, 0x3A3A3C, 255);
        for (int d = 0; d < 3; d++) draw_rr(b, x + 20 + d * 10, y + 17, 6, 6, 3, d == 0 ? 0xFF6E79 : (d == 1 ? 0xF6C66F : 0x62D7B0), 255);
        const char *nm = term ? "Terminal" : app_at(win_[wi].app - 1)->name; int ai = term ? 0 : win_[wi].app - 1;
        app_at(ai)->icon(b, x + cw / 2 - 17, y + 46);
        text(b, x + (cw - 8 * (int)slen(nm)) / 2, y + ch - 32, nm, pal[A_TXT]);
        if (!term && win_[wi].min) text(b, x + (cw - 8 * 11) / 2, y + ch - 16, "(minimised)", pal[A_DIM]);
        hit_add(x, y, cw, ch, term ? 4090 : 4000 + wi);
    }
}

/* ---------- hot corners ---------- */
static void ux_show_desktop(void) {
    for (int i = 0; i < nwin; i++) win_[i].min = 1;
    int rb = terminal_visible; terminal_visible = 0; term_focus = 0; app_on = 0; if (rb) build_scene(); dirty = 1;
}
static void ux_hot_corners(int btn) {
    static int armed = 1;
    int c = (mx <= 1 && my <= 1) ? 1 : ((mx >= (int)W - 2 && my <= 1) ? 2 : ((mx <= 1 && my >= (int)H - 2) ? 3 : ((mx >= (int)W - 2 && my >= (int)H - 2) ? 4 : 0)));
    if (!c) { armed = 1; return; }
    if (!armed || btn || drag_app || !hc_mode) return;
    armed = 0;
    if (c == 1) mc_open = !mc_open;
    else if (c == 2) { for (int i = 0; i < app_count(); i++) if (ci_eq(app_at(i)->name, "notes")) { int rb = terminal_visible; terminal_visible = 0; if (rb) build_scene(); app_at(i)->open(); break; } }
    else if (c == 3) ux_show_desktop();
    else if (hc_mode == 2) ux_lock_now();
    dirty = 1;
}

/* ---------- Spotlight calculator ---------- */
static int ux_calc_query(const char *q, int *out) {                /* 1 if q is a valid arithmetic expression */
    int digit = 0, op = 0; for (const char *p = q; *p; p++) {
        if (*p >= '0' && *p <= '9') digit = 1; else if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%' || *p == '(' || *p == ')') op = 1; else if (*p != ' ') return 0;
    }
    if (!digit || !op) return 0;
    cp = q; cerr = 0; int v = c_expr(); c_ws(); if (*cp || cerr) return 0; *out = v; return 1;
}
static void ux_int_str(int v, char *o) { char t[12]; int n = 0, neg = v < 0; u32 u = neg ? 0u - (u32)v : (u32)v; if (!u) t[n++] = '0'; while (u) { t[n++] = (char)('0' + u % 10); u /= 10; } int k = 0; if (neg) o[k++] = '-'; while (n) o[k++] = t[--n]; o[k] = 0; }

/* ---------- keys: lock screen entry, hotkeys, Mission Control ---------- */
static int ux_key_gate(int k) {
    input_seen = 1;
    if (locked || pw_ui) {
        if (k == 27 && pw_ui) { pw_ui = 0; pw_stage = 0; ux_buf_clear(); lock_msg[0] = 0; dirty = 1; notify("Password unchanged", 1); return 1; }
        if (k == '\b') { if (lock_len) lock_buf[--lock_len] = 0; dirty = 1; return 1; }
        if (k == '\n') { ux_lock_enter(); dirty = 1; return 1; }
        if (k >= 32 && k < 127 && lock_len < 30) { lock_buf[lock_len++] = (char)k; lock_buf[lock_len] = 0; }
        dirty = 1; return 1;
    }
    if (k == KEY_LOCK) { ux_lock_now(); return 1; }
    if (k == KEY_MCTRL) { mc_open = !mc_open; dirty = 1; return 1; }
    if (k == KEY_SNAPL) { ux_snap_top(0); return 1; }
    if (k == KEY_SNAPR) { ux_snap_top(1); return 1; }
    if (mc_open) { if (k == 27) { mc_open = 0; dirty = 1; } return 1; }
    return 0;
}
