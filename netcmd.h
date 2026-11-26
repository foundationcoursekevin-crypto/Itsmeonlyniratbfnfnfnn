/* PixelBeeOS 3.1 - terminal + browser front end for the network stack.
 *   net [status] | net up | net dhcp      ping HOST      dns HOST      http URL      get URL /diskpath
 * Everything blocks (with timeouts) while it waits for the network: the screen does not redraw during a request. */
#ifndef PIXELBEE_NETCMD_H
#define PIXELBEE_NETCMD_H
static void ip_str(const u8 *ip, char *o) { int p = 0; for (int i = 0; i < 4; i++) { u32 v = ip[i]; char d[3]; int n = 0; do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) o[p++] = d[--n]; if (i < 3) o[p++] = '.'; } o[p] = 0; }
static void net_hex(u32 v, int digits) { const char *h = "0123456789abcdef"; for (int i = digits - 1; i >= 0; i--) t_putc(h[(v >> (i * 4)) & 15], A_TXT); }
static int net_auto(void) { if (!net_nic.up && !net_probe()) return 0; if (!net_cfg) return net_dhcp() == 0; return 1; }
static const char *net_errname(int r) {
    switch (r) { case -1: return "bad address"; case -2: return "https:// is not available in this build"; case -3: return "could not look up that name (check DNS / network)"; case -4: return "timed out"; case -5: return "connection refused"; case -6: return "the server sent something that is not HTTP"; case -7: return net_tls_errstr(); default: return "failed"; }
}
static void net_explain_missing(void) {
    if (!net_nic.up) {
        t_say("No supported network card found.", A_ERR);
        if (net_unsup_id) { t_puts("Found an unsupported network device (vendor:device ", A_DIM); net_hex(net_unsup_id & 0xFFFF, 4); t_putc(':', A_DIM); net_hex(net_unsup_id >> 16, 4); t_say(").", A_DIM); }
        t_say("Supported: Intel e1000 and Realtek RTL8139. In QEMU add:  -nic user,model=e1000", A_DIM);
    } else t_say("Not connected yet. Run:  net up", A_WARN);
}
static void cmd_net(void) {
    const char *s = ntok > 1 ? tl[1] : "status"; char b[20];
    if (ci_eq(s, "up") || ci_eq(s, "dhcp")) {
        if (!net_nic.up && !net_probe()) { net_explain_missing(); return; }
        t_puts("Network card: ", A_DIM); t_say(net_nic.name, A_ACC); t_say("Asking for an address (DHCP)...", A_DIM);
        if (net_dhcp()) { t_say("No answer from a DHCP server.", A_ERR); return; }
        ip_str(net_ip, b); t_puts("Connected. Address ", A_OK); t_say(b, A_TXT); notify("Network connected", 1); return;
    }
    if (!ci_eq(s, "status")) { t_say("Usage: net [status] | net up | net dhcp", A_WARN); return; }
    if (!net_nic.up) { net_explain_missing(); return; }
    t_puts(" Card:     ", A_DIM); t_say(net_nic.name, A_ACC);
    t_puts(" MAC:      ", A_DIM); for (int i = 0; i < 6; i++) { net_hex(net_nic.mac[i], 2); if (i < 5) t_putc(':', A_DIM); } t_putc('\n', A_TXT);
    if (!net_cfg) { t_say(" Address:  none (run: net up)", A_WARN); return; }
    ip_str(net_ip, b); t_puts(" Address:  ", A_DIM); t_say(b, A_TXT); ip_str(net_mask, b); t_puts(" Mask:     ", A_DIM); t_say(b, A_TXT);
    ip_str(net_gw, b); t_puts(" Gateway:  ", A_DIM); t_say(b, A_TXT); ip_str(net_dns, b); t_puts(" DNS:      ", A_DIM); t_say(b, A_TXT);
    t_puts(" Packets:  ", A_DIM); t_num(net_stat_rx, 0); t_puts(" in, ", A_TXT); t_num(net_stat_tx, 0); t_puts(" out, ", A_TXT); t_num(net_stat_bad, 0); t_say(" bad", A_TXT);
}
static int net_ready(void) { if (!net_cfg) { net_explain_missing(); return 0; } return 1; }
static void cmd_ping(void) {
    if (ntok < 2) { t_say("Usage: ping HOST", A_WARN); return; } if (!net_ready()) return;
    u8 ip[4]; char b[20]; int r = net_resolve(tl[1], ip); if (r) { t_puts("ping: ", A_ERR); t_say(net_errname(-3), A_ERR); return; }
    ip_str(ip, b); int ok = 0;
    for (int i = 0; i < 4; i++) {
        u32 rtt = 0; r = net_ping(ip, &rtt);
        if (r == 0) { ok++; t_puts("Reply from ", A_OK); t_puts(b, A_TXT); t_puts(": time=", A_TXT); t_num(rtt, 0); t_say(" ms", A_TXT); }
        else if (r == -3) { t_say("Cannot reach the gateway (ARP failed)", A_ERR); return; } else t_say("Request timed out", A_WARN);
        u32 s = now_ms(); while (now_ms() - s < 400) net_poll();
    }
    t_num((u32)ok, 0); t_say(" of 4 replies", ok ? A_OK : A_ERR);
}
static void cmd_dns(void) {
    if (ntok < 2) { t_say("Usage: dns HOST", A_WARN); return; } if (!net_ready()) return;
    u8 ip[4]; char b[20]; int r = net_resolve(tl[1], ip); if (r) { t_puts("dns: ", A_ERR); t_say(r == -5 ? "no such name" : net_errname(-3), A_ERR); return; }
    ip_str(ip, b); t_puts(tl[1], A_TXT); t_puts(" = ", A_DIM); t_say(b, A_OK);
}
/* plain text out of an HTML page (skips <script>/<style>, decodes a few entities) */
static int net_text(const u8 *s, int n, char *o, int max) {
    int p = 0, intag = 0, skip = 0, sp = 1;
    for (int i = 0; i < n && p < max - 1; i++) {
        char c = (char)s[i];
        if (intag) { if (c == '>') intag = 0; continue; }
        if (c == '<') {
            intag = 1; char t[8] = {0}; int k = 0, j = i + 1; if (j < n && s[j] == '/') j++; while (j < n && k < 6 && hlc((char)s[j]) >= 'a' && hlc((char)s[j]) <= 'z') t[k++] = hlc((char)s[j++]);
            if (s[i + 1] == '/' ) { if (!skip || 1) skip = 0; } else if (!ci_eq(t, "script") && !ci_eq(t, "style")) {} else skip = 1;
            if ((ci_eq(t, "p") || ci_eq(t, "br") || ci_eq(t, "div") || ci_eq(t, "li") || ci_eq(t, "h1") || ci_eq(t, "h2") || ci_eq(t, "h3") || ci_eq(t, "tr")) && p && o[p - 1] != '\n') { o[p++] = '\n'; sp = 1; }
            continue;
        }
        if (skip) continue;
        if (c == '&') { if (i + 4 < n && s[i + 1] == 'a' && s[i + 2] == 'm' && s[i + 3] == 'p' && s[i + 4] == ';') { o[p++] = '&'; i += 4; sp = 0; continue; } if (i + 3 < n && s[i + 1] == 'l' && s[i + 2] == 't' && s[i + 3] == ';') { o[p++] = '<'; i += 3; sp = 0; continue; } if (i + 3 < n && s[i + 1] == 'g' && s[i + 2] == 't' && s[i + 3] == ';') { o[p++] = '>'; i += 3; sp = 0; continue; } if (i + 5 < n && s[i + 1] == 'n' && s[i + 2] == 'b' && s[i + 3] == 's' && s[i + 4] == 'p' && s[i + 5] == ';') { c = ' '; i += 5; } }
        if (c == '\r') continue; if (c == '\n' || c == '\t' || c == ' ') { if (!sp) { o[p++] = ' '; sp = 1; } continue; }
        if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '?'; o[p++] = c; sp = 0;
    }
    o[p] = 0; return p;
}
static void cmd_http(void) {
    if (ntok < 2) { t_say("Usage: http http://host/path", A_WARN); return; } if (!net_ready()) return;
    t_puts("GET ", A_DIM); t_say(tl[1], A_DIM); int r = net_http_get(tl[1]); if (r) { t_puts("http: ", A_ERR); t_say(net_errname(r), A_ERR); return; }
    if (net_hs_tls) t_say("Encrypted (TLS 1.3). Server identity NOT verified: do not enter passwords.", A_WARN);
    t_puts("HTTP ", A_OK); t_num((u32)net_hstatus, 0); t_puts("  ", A_OK); t_num((u32)net_blen, 0); t_say(net_htrunc ? " bytes (page larger than 58 KB: cut)" : " bytes", A_OK);
    static char txt[1800]; int n = net_text(net_body, net_blen, txt, (int)sizeof txt); for (int i = 0; i < n; i++) t_putc(txt[i], A_TXT); t_putc('\n', A_TXT);
}
static void cmd_get(void) {
    if (ntok < 3) { t_say("Usage: get http://host/file /diskpath   (saves to the mounted disk D:)", A_WARN); return; } if (!net_ready()) return;
    if (!vol.mounted || vol.ro) { t_say("Mount your PIXELBEEDAT disk first: disk mount 0 1", A_WARN); return; }
    int r = net_http_get(tl[1]); if (r) { t_puts("get: ", A_ERR); t_say(net_errname(r), A_ERR); return; }
    if (net_hstatus != 200) { t_puts("Server answered HTTP ", A_ERR); t_num((u32)net_hstatus, 0); t_putc('\n', A_TXT); return; }
    dk_clock(); r = f32_write_file(&vol, tl[2], net_body, (u32)net_blen); if (r) { dk_fail(r); return; }
    t_puts("Saved ", A_OK); t_num((u32)net_blen, 0); t_puts(" bytes to D:", A_OK); t_say(tl[2], A_TXT); if (net_htrunc) t_say("Warning: the file was larger than 58 KB and was cut.", A_WARN);
}
static int net_cmd_dispatch(void) {
    if (ci_eq(tl[0], "net")) cmd_net(); else if (ci_eq(tl[0], "ping")) cmd_ping(); else if (ci_eq(tl[0], "dns")) cmd_dns();
    else if (ci_eq(tl[0], "http")) cmd_http(); else if (ci_eq(tl[0], "get")) cmd_get(); else return 0; return 1;
}

/* ---- BeeBrowser: real http:// pages ---- */
static void pg_http(const char *url) {
    if (!net_auto()) { pg_offline(url); return; }
    int r = net_http_get(url);
    if (r) { bl_add(1, "This site can't be reached", -1); bl_wrap(0, net_errname(r)); bl_add(0, "", -1); bl_linkline("Go to BeeSearch", "vin://home"); return; }
    if (net_hs_tls) bl_wrap(6, "Encrypted with TLS 1.3 - but the site identity is NOT verified. Do not enter passwords or card numbers.");
    char st[40] = "HTTP "; int p = 5, v = net_hstatus; char d[6]; int dn = 0; do { d[dn++] = (char)('0' + v % 10); v /= 10; } while (v); while (dn) st[p++] = d[--dn]; st[p] = 0; bl_add(6, st, -1);
    int html = h_find(net_body, net_blen < 400 ? net_blen : 400, "<html") >= 0 || h_find(net_body, net_blen < 400 ? net_blen : 400, "<!doctype") >= 0 || h_find(net_body, net_blen < 400 ? net_blen : 400, "<body") >= 0 || h_find(net_body, net_blen < 400 ? net_blen : 400, "<h1") >= 0;
    if (html) html_render((const char *)net_body, net_blen);
    else { int s0 = 0; for (int i = 0; i <= net_blen; i++) if (i == net_blen || net_body[i] == '\n') { char tmp[200]; int c = i - s0 > 199 ? 199 : i - s0; for (int k = 0; k < c; k++) { char ch = (char)net_body[s0 + k]; tmp[k] = (ch == '\r' || (unsigned char)ch < 32) ? ' ' : ch; } tmp[c] = 0; if (c) bl_wrap(0, tmp); s0 = i + 1; } }
    if (net_htrunc) bl_wrap(6, "(page larger than 58 KB: the rest was cut)");
    bl_add(0, "", -1); bl_linkline("Back to BeeSearch", "vin://home");
}
#endif
