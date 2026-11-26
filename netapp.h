/* PixelBeeOS 3.1 - Network app: connect / disconnect / test the internet with buttons (no commands needed).
 * Slow work (card detection, DHCP, the test) is deferred by one frame so the screen first shows "Connecting..." and
 * only then blocks; net_ui_tick() is called from the main loop after each frame. Also auto-connects once at startup. */
#ifndef PIXELBEE_NETAPP_H
#define PIXELBEE_NETAPP_H
#define NT_APP 15
static int nt_job, nt_armed, nt_auto = 1, nt_boot_done, nt_msg_col; static char nt_msg[110];
static struct { int ran, ping_ok, dns_ok, web_ok, web_status; u32 ping_ms; char dns_ip[20]; } nt_test;
static void nt_say(const char *m, int col) { scpy(nt_msg, m, (int)sizeof nt_msg); nt_msg_col = col; }
/* 0 = no usable card, 1 = card ready but no address, 2 = connected */
static int nt_status(void) { return !net_nic.up ? 0 : (net_cfg ? 2 : 1); }
static const char *nt_card_name(void) { return net_nic.up ? net_nic.name : (pci_net_present ? "Unsupported network card" : "None"); }
static void nt_connect(int tries) {
    memset(&nt_test, 0, sizeof nt_test);
    if (!net_nic.up && !net_probe()) { nt_say(net_unsup_id ? "This network card is not supported yet (see Device Manager)." : "No network card found. In your VM choose Intel PRO/1000 (e1000), NAT.", A_ERR); return; }
    if (net_dhcp_n(tries)) { nt_say("The router did not answer. Check that the VM network is NAT or Bridged.", A_WARN); return; }
    nt_say("Connected.", A_OK); notify("Network connected", 1);
}
static void nt_run_test(void) {
    memset(&nt_test, 0, sizeof nt_test); nt_test.ran = 1; if (!net_cfg) return; u8 ip[4]; u32 rtt = 0;
    const u8 *hop = (net_gw[0] | net_gw[1] | net_gw[2] | net_gw[3]) ? net_gw : net_dns; if (net_ping(hop, &rtt) == 0) { nt_test.ping_ok = 1; nt_test.ping_ms = rtt; }
    if (net_resolve("example.com", ip) == 0) { nt_test.dns_ok = 1; ip_str(ip, nt_test.dns_ip); }
    if (nt_test.dns_ok && net_http_get("http://example.com/") == 0) { nt_test.web_ok = 1; nt_test.web_status = net_hstatus; }
    nt_say(nt_test.web_ok ? "Internet works." : (nt_test.dns_ok ? "Name lookup works but web pages do not load." : (nt_test.ping_ok ? "Router reachable, but no internet (DNS failed)." : "Cannot reach the router.")), nt_test.web_ok ? A_OK : A_WARN);
}
static void net_ui_tick(void) {
    if (!nt_boot_done) { nt_boot_done = 1; if (nt_auto) { if (net_probe()) { nt_connect(1); } } return; }      /* startup: one quick attempt only */
    if (!nt_job || !nt_armed) return; int j = nt_job; nt_job = 0; nt_armed = 0;
    if (j == 1) nt_connect(3); else if (j == 2) nt_run_test();
    dirty = 1;
}
static void app_open_net(void) { app_on = NT_APP; dirty = 1; }
static void nt_key(int k) { if (k == 27) app_on = 0; }
static void nt_click(int id) {
    if (nt_job) return;
    if (id == 9100) { nt_job = 1; nt_armed = 0; nt_say("Connecting... please wait (up to ~15 seconds).", A_ACC); }
    else if (id == 9101) { net_cfg = 0; memset(net_ip, 0, 4); memset(&nt_test, 0, sizeof nt_test); nt_say("Disconnected.", A_DIM); notify("Network disconnected", 0); }
    else if (id == 9102 && net_cfg) { nt_job = 2; nt_armed = 0; nt_say("Testing the connection...", A_ACC); }
    else if (id == 9103) app_launch("browser");
    else if (id == 9104) nt_auto = !nt_auto;
    dirty = 1;
}
static void nt_hex(char *o, u8 v) { const char *h = "0123456789ABCDEF"; o[0] = h[v >> 4]; o[1] = h[v & 15]; }
static void nt_wifi(u32 *b, int cx, int cy, int s, u32 col, int a) {              /* wifi glyph: three nested arcs + dot; s = overall size */
    int dy = cy + s / 2;                                                           /* dot position */
    for (int i = 1; i <= 3; i++) { int r = s * i / 3 + s / 6; draw_line(b, cx - r * 3 / 4, dy - r * 55 / 100, cx, dy - r, col, a); draw_line(b, cx, dy - r, cx + r * 3 / 4, dy - r * 55 / 100, col, a);
        draw_line(b, cx - r * 3 / 4, dy - r * 55 / 100 + 1, cx, dy - r + 1, col, a); draw_line(b, cx, dy - r + 1, cx + r * 3 / 4, dy - r * 55 / 100 + 1, col, a); }
    draw_rr(b, cx - 3, dy - 3, 6, 6, 3, col, (u32)a);
}
static void nt_btn(u32 *b, int x, int y, int w, const char *t, int id, u32 col) {
    int busy = nt_job != 0; draw_rr(b, x, y, w, 34, 10, col, busy ? 90 : (hmix(220, 255, hov(x, y, w, 34)))); text(b, x + (w - 8 * slen(t)) / 2, y + 11, t, 0xFFFFFF); if (!busy) hit_add(x, y, w, 34, id);
}
static void nt_row(u32 *b, int x, int y, int w, const char *k, const char *v, u32 vc) { row_card(b, x, y - 7, w); text(b, x + 10, y, k, pal[A_DIM]); text(b, x + 108, y, v, vc); }
static void nt_draw(u32 *b, u32 ms) {
    (void)ms; int x = wx + 28, y = wy + 56, st = nt_status(), busy = nt_job != 0; if (busy) nt_armed = 1;
    heading(b, x, y, "Network", pal[A_ACC]);
    /* status card */
    u32 col = busy ? 0x3D8BFF : (st == 2 ? 0x2FBF71 : (st == 1 ? 0xE0A030 : 0xD9534F));
    draw_rr(b, x - 12, y + 30, ww - 32, 92, 16, 0xFFFFFF, 14); draw_rr(b, x + 6, y + 46, 58, 58, 29, col, 235); nt_wifi(b, x + 35, y + 64, 16, 0xFFFFFF, 255);
    if (st != 2 && !busy) { draw_line(b, x + 15, y + 95, x + 55, y + 55, 0xFFFFFF, 255); draw_line(b, x + 16, y + 95, x + 56, y + 55, 0xFFFFFF, 255); }
    const char *title = busy ? "Connecting..." : (st == 2 ? "Connected" : (st == 1 ? "Not connected" : "No network card"));
    heading(b, x + 82, y + 44, title, busy ? pal[A_ACC] : (st == 2 ? pal[A_OK] : (st == 1 ? pal[A_WARN] : pal[A_ERR])));
    char ln[64]; if (st == 2) { scpy(ln, net_nic.name, 28); int p = slen(ln); ln[p++] = ' '; ln[p++] = ' '; ip_str(net_ip, ln + p); text(b, x + 82, y + 78, ln, pal[A_TXT]); }
    else text(b, x + 82, y + 78, st == 1 ? "Press Connect to get an internet address." : (pci_net_present ? "A network device was found but is not supported." : "No network device detected."), pal[A_TXT]);
    text(b, x + 82, y + 98, nt_msg[0] ? nt_msg : (st == 2 ? "Online." : "Offline."), nt_msg[0] ? pal[nt_msg_col] : pal[A_DIM]);
    /* buttons */
    int bx = x - 12;
    if (st == 2) { nt_btn(b, bx, y + 132, 136, "Reconnect", 9100, 0x2B6CB0); nt_btn(b, bx + 146, y + 132, 136, "Disconnect", 9101, 0x8A4B4B); nt_btn(b, bx + 292, y + 132, 136, "Test internet", 9102, 0x2F8F6A); nt_btn(b, bx + 438, y + 132, 136, "Open browser", 9103, 0x5B4BB0); }
    else nt_btn(b, bx, y + 132, 200, busy ? "Please wait..." : "Connect", 9100, 0x2F8F6A);
    /* details (left) */
    int cy = y + 208, w1 = 310; heading(b, x, cy - 24, "Details", pal[A_ACC]); char v[40];
    nt_row(b, x - 12, cy + 12, w1, "Card", st ? net_nic.name : nt_card_name(), pal[A_TXT]);
    if (st) { for (int i = 0; i < 6; i++) { nt_hex(v + i * 3, net_nic.mac[i]); v[i * 3 + 2] = ':'; } v[17] = 0; } else scpy(v, "-", 40); nt_row(b, x - 12, cy + 42, w1, "MAC address", v, pal[A_TXT]);
    if (st == 2) ip_str(net_ip, v); else scpy(v, "-", 40); nt_row(b, x - 12, cy + 72, w1, "IP address", v, pal[A_TXT]);
    if (st == 2) ip_str(net_gw, v); else scpy(v, "-", 40); nt_row(b, x - 12, cy + 102, w1, "Router", v, pal[A_TXT]);
    if (st == 2) ip_str(net_dns, v); else scpy(v, "-", 40); nt_row(b, x - 12, cy + 132, w1, "DNS server", v, pal[A_TXT]);
    { char a[12], c[12]; fmt_base(net_stat_rx, 10, a); fmt_base(net_stat_tx, 10, c); scpy(v, a, 12); int p = slen(v); scpy(v + p, " in / ", 8); p = slen(v); scpy(v + p, c, 12); p = slen(v); scpy(v + p, " out", 6); nt_row(b, x - 12, cy + 162, w1, "Packets", v, pal[A_TXT]); }
    /* internet test + options (right) */
    int rx = x + 318, w2 = ww - 28 - 318 - 18; heading(b, rx, cy - 24, "Internet test", pal[A_ACC]);
    const char *names[3] = {"Router", "Name lookup", "Web page"}; int oks[3] = {nt_test.ping_ok, nt_test.dns_ok, nt_test.web_ok};
    for (int i = 0; i < 3; i++) {
        int ry = cy + 12 + i * 30; row_card(b, rx - 12, ry - 7, w2); text(b, rx - 2, ry, names[i], pal[A_DIM]);
        const char *r = !nt_test.ran ? "-" : (oks[i] ? "OK" : "failed"); text(b, rx + 110, ry, r, !nt_test.ran ? pal[A_DIM] : (oks[i] ? pal[A_OK] : pal[A_ERR]));
        if (nt_test.ran && oks[i]) { char d[24]; if (i == 0) { fmt_base(nt_test.ping_ms, 10, d); int p = slen(d); scpy(d + p, " ms", 5); } else if (i == 1) scpy(d, nt_test.dns_ip, 20); else { scpy(d, "HTTP ", 8); fmt_base((u32)nt_test.web_status, 10, d + 5); } text(b, rx + 150, ry, d, pal[A_TXT]); }
    }
    row_card(b, rx - 12, cy + 106, w2); text(b, rx - 2, cy + 113, "Connect at startup", pal[A_DIM]);
    draw_rr(b, rx + w2 - 80, cy + 104, 58, 26, 12, nt_auto ? 0x318E9A : 0x647A91, 200); text(b, rx + w2 - 66, cy + 113, nt_auto ? "ON" : "OFF", 0xFFFFFF); hit_add(rx + w2 - 80, cy + 104, 58, 26, 9104);
    text(b, rx - 2, cy + 146, "https:// pages are encrypted (TLS 1.3) but", pal[A_DIM]); text(b, rx - 2, cy + 164, "the site identity is not verified yet.", pal[A_WARN]);
}
static void icon_net(u32 *b, int x, int y) {                                      /* globe on a blue tile */
    draw_rr(b, x + 3, y + 3, 28, 28, 8, 0x1B5E9E, 255);
    draw_rr(b, x + 8, y + 8, 18, 18, 9, 0xFFFFFF, 255); draw_rr(b, x + 10, y + 10, 14, 14, 7, 0x1B5E9E, 255);               /* ring */
    draw_line(b, x + 10, y + 17, x + 24, y + 17, 0xFFFFFF, 255);                                                         /* equator */
    draw_rr(b, x + 13, y + 8, 8, 18, 4, 0xFFFFFF, 255); draw_rr(b, x + 15, y + 10, 4, 14, 2, 0x1B5E9E, 255);              /* meridian */
}
#endif
