/* PixelBeeOS 3.1 - Liquid Glass Edition (based on 2.11 Turbo) with interactive task, device and security centers. Original PixelBeeOS identity with a compact responsive launcher,
 * translucent menu bar and familiar desktop conventions. Boots via GRUB Multiboot 1. */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define MAXW 1920   /* bigger panels are centred (black bars) rather than cropped */
#define MAXH 1200
#define KEY_UP 0x100
#define KEY_DOWN 0x101
#define KEY_LEFT 0x102
#define KEY_RIGHT 0x103
#define KEY_ALTTAB 0x110
#define KEY_LOCK 0x112      /* Super+L */
#define KEY_MCTRL 0x113     /* F3 or Super+Up: Mission Control */
#define KEY_SNAPL 0x114     /* Super+Left */
#define KEY_SNAPR 0x115     /* Super+Right */
#define KEY_SPOTLIGHT 0x111
#define KEY_HOME 0x116
#define KEY_END 0x117
#define KEY_DEL 0x118
#define KEY_PGUP 0x119
#define KEY_PGDN 0x11A
static void mouse_packet(int btn, int dx, int dy);
static void mouse_drain(void); static void cursor_sync(void); static void power_off(void); static void power_restart(void);
static void run_in_terminal(const char *cmd);
static void hardware_probe(void);
static void build_scene(void);
static void desktop_open_this_pc(void); static void app_open_mines(void);
static void desktop_open_folder(int which);
static void desktop_open_drive(void);
static void desktop_open_node(int node);
static int in_flip, pwr_arm; static unsigned pwr_until;
#define CUR_W 16   /* anti-aliased pointer sprite incl. soft shadow; origin is 1px up-left of the hotspot */
#define CUR_H 24
#define PWR_OFF_ID 900
#define PWR_RST_ID 901
/* ---- App registry: every app is one row in app_tab[] (bottom of apps.h) ---- */
typedef struct { const char *name, *cmd; void (*open)(void); void (*key)(int); void (*draw)(u32 *, u32);
                 void (*click)(int); void (*mouse)(int, int, int); void (*icon)(u32 *, int, int); int pw, ph; } App;
static int app_count(void); static const App *app_at(int i); static int app_fast;
static void app_open_notes_node(int idx);
static int name_has_query(const char *name, const char *q);

#include "font.h"

/* Freestanding signed 64-bit division helper (no hosted runtime/libgcc required). */
long long __divdi3(long long numerator, long long denominator) {
    unsigned long long a = numerator < 0 ? 0ULL - (unsigned long long)numerator : (unsigned long long)numerator;
    unsigned long long b = denominator < 0 ? 0ULL - (unsigned long long)denominator : (unsigned long long)denominator;
    if (!b) return 0;
    unsigned long long q = 0, r = 0;
    for (int bit = 63; bit >= 0; --bit) {
        r = (r << 1) | ((a >> bit) & 1ULL);
        if (r >= b) { r -= b; q |= 1ULL << bit; }
    }
    return ((numerator < 0) != (denominator < 0)) ? -(long long)q : (long long)q;
}

/* ============================ hardware layer ============================ */
#ifdef HOSTTEST
#include <string.h>
static void outb(u16 p, u8 v)  { (void)p; (void)v; }
static u8   inb(u16 p)         { (void)p; return 0; }
static u16  inw(u16 p)         { (void)p; return 0; }
static void outw(u16 p, u16 v) { (void)p; (void)v; }
static void outl(u16 p, u32 v) { (void)p; (void)v; }
static u32 inl(u16 p) { (void)p; return 0xFFFFFFFFu; }
static u32 host_ms;
static u32 now_ms(void) { return host_ms; }
static int poll_key(void) { return 0; }
static int kbc_ok = 1, kbd_alive = 1, mouse_alive = 1, input_seen = 1;   /* host build: pretend the input devices answered */
static void mouse_init(void) {}
static void ps2_pump(void) {}
static void rtc_read(int *h, int *mi, int *sc, int *d, int *mo, int *y) {
    u32 t = host_ms / 1000 + 14 * 3600 + 7 * 60;
    *h = (t / 3600) % 24; *mi = (t / 60) % 60; *sc = t % 60; *d = 5; *mo = 11; *y = 2026;
}
static void speaker_on(int hz) { (void)hz; }
static void speaker_off(void) {}
#else
static inline void outb(u16 p, u8 v)  { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8   inb(u16 p)         { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outl(u16 p, u32 v) { __asm__ volatile("outl %0,%1" :: "a"(v), "Nd"(p)); }
static inline u16  inw(u16 p)         { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32  inl(u16 p)         { u32 v; __asm__ volatile("inl %1,%0" : "=a"(v) : "Nd"(p)); return v; }

void *memcpy(void *d, const void *s, unsigned n) {
    void *r = d; unsigned q = n >> 2, t = n & 3;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(q) :: "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(t) :: "memory"); return r;
}
void *memset(void *d, int c, unsigned n) {
    void *r = d; unsigned q = n >> 2, t = n & 3, v = (u8)c * 0x01010101u;
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(q) : "a"(v) : "memory");
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(t) : "a"(c) : "memory"); return r;
}
void *memmove(void *d, const void *s, unsigned n) {
    u8 *a = d; const u8 *b = s;
    if (a < b) { if ((unsigned)(b - a) >= 4) return memcpy(d, s, n); while (n--) *a++ = *b++; }   /* forward overlap: rep movsl is safe when the gap is >= 4 bytes */
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}

/* millisecond clock polled from PIT channel 0 (no interrupts needed) */
static u32 now_ms(void) {
    static u32 ms = 0, rem = 0; static u16 last = 0;
    outb(0x43, 0x00);
    u16 c = inb(0x40); c |= (u16)inb(0x40) << 8;
    u16 d = (u16)(last - c); last = c;
    rem += d; ms += rem / 1193; rem %= 1193;
    return ms;
}

/* PS/2 keyboard, scancode set 1, polled */
static const char kn[] = "\0\x1b" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char ks[] = "\0\x1b" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }
static void rtc_read(int *h, int *mi, int *sc, int *d, int *mo, int *y) {   /* CMOS real-time clock */
    int guard = 0; while ((cmos(0x0A) & 0x80) && guard++ < 20000) {}
    u8 se = cmos(0), mn = cmos(2), hr = cmos(4), dy = cmos(7), mt = cmos(8), yr = cmos(9), b = cmos(0x0B);
    int pm = hr & 0x80; hr &= 0x7F;
    if (!(b & 4)) {
        se = (se & 15) + (se >> 4) * 10; mn = (mn & 15) + (mn >> 4) * 10; hr = (hr & 15) + (hr >> 4) * 10;
        dy = (dy & 15) + (dy >> 4) * 10; mt = (mt & 15) + (mt >> 4) * 10; yr = (yr & 15) + (yr >> 4) * 10;
    }
    if (!(b & 2)) { hr %= 12; if (pm) hr += 12; }
    *h = hr; *mi = mn; *sc = se; *d = dy; *mo = mt; *y = 2000 + yr;
}
/* PC speaker on PIT channel 2 */
static void speaker_on(int hz) {
    u32 div = 1193182 / (u32)hz;
    outb(0x43, 0xB6); outb(0x42, div & 255); outb(0x42, (div >> 8) & 255); outb(0x61, inb(0x61) | 3);
}
static void speaker_off(void) { outb(0x61, inb(0x61) & 0xFC); }

/* PS/2 mouse: 3-byte packets, polled from the same 8042 controller */
static void kbc_wait_w(void) { int t = 100000; while ((inb(0x64) & 2) && t--) {} }
static int kbc_wait_r(void) { int t = 100000; while (!(inb(0x64) & 1) && t--) {} return inb(0x64) & 1; }
static int mouse_cmd(u8 c) { kbc_wait_w(); outb(0x64, 0xD4); kbc_wait_w(); outb(0x60, c); if (kbc_wait_r()) return inb(0x60); return -1; }   /* returns the reply byte (0xFA = ACK) or -1 */
static int kbc_ok = 1;   /* 0 = no PS/2 (8042) controller answered: the PC is probably USB-only */
static int kbd_alive, mouse_alive, input_seen;   /* did a PS/2 keyboard / mouse acknowledge our commands; has any real input arrived yet */
/* Send a command to the keyboard and handle the PS/2 RESEND response. */
static int kbd_cmd(u8 c) {
    for (int retry = 0; retry < 3; retry++) {
        kbc_wait_w(); outb(0x60, c);
        if (!kbc_wait_r()) return -1;
        u8 reply = inb(0x60);
        if (reply == 0xFA) return 0;       /* ACK */
        if (reply != 0xFE) return -1;     /* RESEND is the only retry response */
    }
    return -1;
}
static void mouse_init(void) {
    /* Robust 8042 bring-up (works on desktop BIOSes that leave the controller half-configured). */
    if (inb(0x64) == 0xFF) { kbc_ok = 0; return; }                    /* floating bus: no controller */
    kbc_wait_w(); outb(0x64, 0xAD); kbc_wait_w(); outb(0x64, 0xA7);   /* quiet both ports while we configure */
    for (int g = 0; g < 64 && (inb(0x64) & 1); g++) inb(0x60);       /* throw away anything left over from GRUB / BIOS */
    kbc_wait_w(); outb(0x64, 0x20); u8 cfg = kbc_wait_r() ? inb(0x60) : 0x45;
    cfg |= 0x40;                                                      /* scancode translation: keyboard speaks set 1 */
    cfg &= (u8)~0x30;                                                 /* clocks on for keyboard and mouse ports */
    cfg |= 3;
    kbc_wait_w(); outb(0x64, 0x60); kbc_wait_w(); outb(0x60, cfg);
    kbc_wait_w(); outb(0x64, 0xAE);                                   /* enable keyboard port */
    kbc_wait_w(); outb(0x64, 0xA8);                                   /* enable aux (mouse) port */
    for (int g = 0; g < 64 && (inb(0x64) & 1); g++) inb(0x60);
    /* Reset the keyboard, consume its BAT completion byte, then enable scanning. */
    kbd_cmd(0xF5);                                                    /* disable scanning during setup */
    if (kbd_cmd(0xFF) == 0) {
        kbd_alive = 1;
        for (int t = 0; t < 400000 && !(inb(0x64) & 1); t++) {}
        if (inb(0x64) & 1) (void)inb(0x60);                           /* BAT: normally 0xAA */
    }
    if (kbd_cmd(0xF4) == 0) kbd_alive = 1;                            /* enable keyboard scanning */
    if (mouse_cmd(0xFF) == 0xFA) mouse_alive = 1;                     /* mouse reset, then drop its 0xAA/0x00 id bytes */
    for (int t = 0; t < 400000 && !(inb(0x64) & 1); t++) {}
    for (int g = 0; g < 8 && (inb(0x64) & 1); g++) inb(0x60);
    mouse_cmd(0xF6);
    if (mouse_cmd(0xF3) == 0xFA) mouse_cmd(200);                      /* 200 samples/s: twice the default 100, noticeably smoother pointer */
    if (mouse_cmd(0xE8) == 0xFA) mouse_cmd(3);                        /* 8 counts per mm: finer movement */
    if (mouse_cmd(0xF4) == 0xFA) mouse_alive = 1;    /* defaults, start streaming */
}
static void mouse_byte(u8 b) {
    static int idx = 0; static u8 p[3];
    if (idx == 0 && !(b & 8)) return;                                 /* resync */
    p[idx++] = b;
    if (idx == 3) {
        idx = 0; if (p[0] & 0xC0) return;                              /* overflow = garbage packet */
        int dx = p[1] - ((p[0] << 4) & 0x100), dy = p[2] - ((p[0] << 3) & 0x100);
        mouse_packet(p[0] & 7, dx, -dy);
    }
}
static int key_scan(u8 sc) {
    static int shift = 0, caps = 0, ext = 0, alt = 0, super = 0, numlock = 1;
    if (sc == 0xE0) { ext = 1; return 0; }
    int rel = sc & 0x80; sc &= 0x7F;
    if (sc == 0x38) { alt = !rel; ext = 0; return 0; }               /* left or right Alt */
    if (alt && sc == 0x0F && !rel) { ext = 0; return KEY_ALTTAB; }   /* Alt+Tab */
    if (ext) {
        ext = 0;
        if (sc == 0x5B || sc == 0x5C) { super = !rel; return 0; } /* left/right Windows key */
        if (!rel) { if (sc == 0x47) return KEY_HOME; if (sc == 0x4F) return KEY_END; if (sc == 0x53) return KEY_DEL; if (sc == 0x49) return KEY_PGUP; if (sc == 0x51) return KEY_PGDN; }
        if (!rel && sc == 0x1C) return '\n';                       /* keypad Enter */
        if (!rel && sc == 0x35) return '/';                         /* keypad / */
        if (!rel && super) { if (sc == 0x48) return KEY_MCTRL; if (sc == 0x4B) return KEY_SNAPL; if (sc == 0x4D) return KEY_SNAPR; }
        if (!rel) { if (sc == 0x48) return KEY_UP; if (sc == 0x50) return KEY_DOWN; if (sc == 0x4B) return KEY_LEFT; if (sc == 0x4D) return KEY_RIGHT; }
        return 0;
    }
    if (super && sc == 0x39 && !rel) return KEY_SPOTLIGHT; /* Win + Space */
    if (super && sc == 0x26 && !rel) return KEY_LOCK;      /* Win + L */
    if (sc == 0x3D && !rel) return KEY_MCTRL;               /* F3 */
    if (sc == 0x2A || sc == 0x36) { shift = !rel; return 0; }
    if (sc == 0x45) { if (!rel) numlock = !numlock; return 0; }       /* Num Lock */
    if (sc >= 0x47 && sc <= 0x53 && sc != 0x4A && sc != 0x4E) {        /* keypad digits and . (arrows when Num Lock is off) */
        if (rel) return 0;
        static const char pad[] = "789\0" "456\0" "123" "0.";                /* 0x47..0x53; entries 3 and 7 are the - and + keys handled below */
        if (!numlock) { if (sc == 0x48) return KEY_UP; if (sc == 0x50) return KEY_DOWN; if (sc == 0x4B) return KEY_LEFT; if (sc == 0x4D) return KEY_RIGHT; if (sc == 0x47) return KEY_HOME; if (sc == 0x4F) return KEY_END; if (sc == 0x53) return KEY_DEL; if (sc == 0x49) return KEY_PGUP; if (sc == 0x51) return KEY_PGDN; return 0; }
        return (u8)pad[sc - 0x47];
    }
    if (!rel && sc == 0x37) return '*';                                 /* keypad * */
    if (!rel && sc == 0x4A) return '-';                                 /* keypad - */
    if (!rel && sc == 0x4E) return '+';                                 /* keypad + */
    if (!rel && sc == 0x56) return shift ? '|' : '\\';                  /* extra key on 102/105-key keyboards */
    if (rel) return 0;
    if (sc == 0x3A) { caps = !caps; return 0; }
    if (sc >= 58) return 0;
    char c = shift ? ks[sc] : kn[sc];
    if (caps && !shift && c >= 'a' && c <= 'z') c -= 32;
    else if (caps && shift && c >= 'A' && c <= 'Z') c += 32;
    return (u8)c;
}
static u8 kq[64]; static int kqh, kqt;
static void ps2_pump(void) {                 /* called often, also from inside the drawing code: mouse packets are decoded the moment they arrive */
    for (int g = 0; g < 128; g++) {
        u8 st = inb(0x64); if (!(st & 1)) break; u8 sc = inb(0x60); input_seen = 1;
        if (st & 0x20) mouse_byte(sc);
        else { int n = (kqt + 1) & 63; if (n != kqh) { kq[kqt] = sc; kqt = n; } }
    }
    cursor_sync();                           /* pointer follows the mouse even while a frame is still being drawn */
}
static int poll_key(void) {
    ps2_pump(); mouse_drain();
    while (kqh != kqt) { u8 sc = kq[kqh]; kqh = (kqh + 1) & 63; int k = key_scan(sc); if (k) return k; }
    return 0;
}
#endif

/* ============================ framebuffer ============================ */
static u32 W = 1024, H = 768, PITCH = 4096;
/* Display format (filled in by kmain from the Multiboot framebuffer info). The UI always composes 0x00RRGGBB in back[];
   flip() converts only the pixels that changed, so 16/24/32 bpp and RGB/BGR panels all work. */
static u8 *fbp;                                   /* first visible byte (the UI is centred when the screen is bigger than MAXW x MAXH) */
static u32 fb_bpp = 32, fb_bytes = 4;
static u8 fb_rp = 16, fb_gp = 8, fb_bp = 0, fb_rs = 8, fb_gs = 8, fb_bs = 8;   /* channel bit position / size */
static int fb_native = 1;                         /* 32 bpp 0x00RRGGBB: copy pixels untouched */
static int vsync_ok, vsync_on = 0, flip_ms;       /* VGA retrace sync (auto-detected), cost of the last flip */
static u32 mem_kb = 131072;
#define WIN_TINT 0x1C1C1E   /* one dark glass material shared by the Terminal and every app window */
static int flat_ui = 0;   /* 0 = frosted glass on DESKTOP surfaces (menu bar, dock, panels); 1 = everything solid. App windows and the Terminal are always solid. */
static int win_glass_on = 0, desk_icons = 0;   /* windows stay solid; desktop shortcut icons are hidden */
#define PA(a) (flat_ui ? 255 : (a))
static int win_alpha = 214, blur_r = 8, prompt_mode, app_on, dock_ix0; /* rich frosted glass; blur runs once per scene and is cached, so it does not cost per-frame speed */
static int launcher_open; static char launcher_query[48]; static int launcher_len;
static int desk_menu_open, folder_prompt, gallery_open, desk_menu_x, desk_menu_y;
/* PixelBeeOS 2.3 (ux.h): lock screen, Mission Control, hot corners */
static int locked, pw_set, pw_ui, pw_stage, mc_open, hc_mode = 1 /* 0 off, 1 on, 2 on + lock corner */, autolock_min = 5, lock_len, lock_fail, input_seen;
static u32 pw_h[2], pw_salt, last_input_ms, lock_until; static char lock_buf[32], pw_tmp[32], lock_msg[48];
static void ux_lock_now(void); static void ux_passwd_start(void); static void ux_hot_corners(int btn); static void ux_snap_release(int app);
static void ux_mc_click(int id); static int ux_key_gate(int k); static void ux_snap_top(int mode); static void ux_show_desktop(void);
static char new_folder_name[24]; static int new_folder_len, folder_parent;
static char launchpad_query[48]; static int launchpad_len;
static int terminal_visible; /* desktop is the default; Terminal is opened on demand */
static int fa_pc_view; /* Files window is currently showing the This PC overview */
static int launchpad_open, control_open;
static int display_level = 100, alerts_enabled = 1;
static u32 perf_fps, perf_frames, perf_epoch; static int comp_ms;
static int performance_mode = 0;
static int low_ram_mode = 0;
static u32 frame_interval_ms = 16; /* 60 Hz target on capable hardware; 30 Hz on low-memory systems */
static int acpi_present, audio_device_present;
static int pci_device_count, pci_net_present, pci_storage_present, pci_usb_present;

/* Automatic 2 GB-class profile: keep the frosted-glass look, but use a tiny blur
   radius and a 30 Hz redraw budget to reduce CPU work on older laptops. */
static void apply_memory_profile(u32 detected_kb) {
    low_ram_mode = detected_kb && detected_kb <= 3u * 1024u * 1024u;
    if (low_ram_mode) {
        frame_interval_ms = 33;
        if (!performance_mode) { flat_ui = 0; blur_r = 2; win_alpha = 174; }
    } else {
        frame_interval_ms = 16;
        if (!performance_mode) { flat_ui = 0; blur_r = 8; win_alpha = 214; }
    }
}
static int mx = 512, my = 384, pmx = 512, pmy = 384, mbtn, mouse_seen;   /* mx,my = position the UI logic sees; pmx,pmy = raw pointer, updated instantly */
typedef struct { int x, y, w, h, id; } Hit;
static Hit hits[256]; static int nhits;
static void hit_add(int x, int y, int w, int h, int id) { if (nhits < 256) { hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].id = id; nhits++; } }
static int hot(int x, int y, int w, int h) { return mouse_seen && mx >= x && mx < x + w && my >= y && my < y + h; }
static char wtitle[40] = "Terminal  -  PixelBeeOS", prompt_txt[24];
static u32 bgbuf[MAXW * MAXH];     /* pre-rendered static scene: Prism wallpaper + floating glass panels */
static u32 bgblur[MAXW * MAXH];     /* blurred wallpaper: the 'frosted' backdrop for floating windows */
static u32 back[MAXW * MAXH];      /* per-frame composition target (also scratch while building the scene) */
static u32 la[MAXW], lb[MAXW];

/* 2.11 Turbo: bit-exact packed blend. Red+blue are blended together in one 32-bit multiply and green in a second one
   (instead of three separate multiplies/divides per pixel). Output is identical to the old per-channel D255() blend. */
#define RB_MASK 0x00FF00FFu
static inline u32 mix(u32 d, u32 s, u32 a) {
    if (a == 0) return d;
    if (a >= 255) return s;
    u32 ia = 255 - a;
    u32 rb = (d & RB_MASK) * ia + (s & RB_MASK) * a + 0x00800080u; rb = ((rb + ((rb >> 8) & RB_MASK)) >> 8) & RB_MASK;
    u32 g  = ((d >> 8) & 255) * ia + ((s >> 8) & 255) * a + 128u;   g  = ((g + (g >> 8)) >> 8) & 255;
    return rb | (g << 8);
}
/* blend n pixels of one constant colour: the colour side of the maths is done once per span, not once per pixel */
static inline void blend_span(u32 *p, int n, u32 col, u32 a) {
    if (a == 0 || n <= 0) return;
    if (a >= 255) { for (int i = 0; i < n; i++) p[i] = col; return; }
    u32 ia = 255 - a, srb = (col & RB_MASK) * a + 0x00800080u, sg = ((col >> 8) & 255) * a + 128u;
    for (int i = 0; i < n; i++) {
        u32 d = p[i];
        u32 rb = (d & RB_MASK) * ia + srb; rb = ((rb + ((rb >> 8) & RB_MASK)) >> 8) & RB_MASK;
        u32 g  = ((d >> 8) & 255) * ia + sg; g = ((g + (g >> 8)) >> 8) & 255;
        p[i] = rb | (g << 8);
    }
}

/* PixelBeeOS frosted glass: strengthen colour and highlights for the original desktop material. */
static void lift(u32 *b, int n, int sat, int white) {
    for (int k = 0; k < n; k++) {
        u32 c = b[k]; int r = (c >> 16) & 255, g = (c >> 8) & 255, bl = c & 255, gr = (r * 30 + g * 59 + bl * 11) / 100;
        r = gr + (r - gr) * sat / 100; g = gr + (g - gr) * sat / 100; bl = gr + (bl - gr) * sat / 100;
        r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); bl = bl < 0 ? 0 : (bl > 255 ? 255 : bl);
        b[k] = mix(((u32)r << 16) | ((u32)g << 8) | (u32)bl, 0xFFFFFF, (u32)white);
    }
}
/* rim light strength at (i,j): bright specular edge at the top-left, a faint glint at the bottom-right, soft in between */
static u32 rim_a(int i, int j, int w, int h, u32 base) {
    int u = (i * 255 / (w > 1 ? w : 1) + j * 255 / (h > 1 ? h : 1)) / 2;
    u32 spec = u < 128 ? (u32)(255 - u * 2) : (u32)((u - 128) * 2 * 45 / 100);
    return base * (70 + spec * 185 / 255) / 255;
}

/* anti-aliased rounded-rect coverage 0..255 for pixel (px,py) in rect (x,y,w,h,r) */
static u32 rr_cov(int px, int py, int x, int y, int w, int h, int r) {
    if (px < x || py < y || px >= x + w || py >= y + h) return 0;
    if (r <= 0) return 255;
    int cx = px < x + r ? x + r : (px >= x + w - r ? x + w - r - 1 : px);
    int cy = py < y + r ? y + r : (py >= y + h - r ? y + h - r - 1 : py);
    int dx = px - cx, dy = py - cy, d2 = dx * dx + dy * dy;
    if (d2 <= (r - 1) * (r - 1)) return 255;
    int R = 2 * r + 1;
    int v = (R * R - 4 * d2) * 255 / (4 * R);
    return v < 0 ? 0 : (v > 255 ? 255 : (u32)v);
}

static int hole_x, hole_y, hole_w, hole_h;      /* draw_rr skips this rectangle when hole_w > 0 */
static void draw_rr(u32 *buf, int x, int y, int w, int h, int r, u32 col, u32 alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;
    if (2 * r > w) r = w / 2; if (2 * r > h) r = h / 2; if (r < 0) r = 0;
    int xa = x < 0 ? 0 : x, xb = x + w > (int)W ? (int)W : x + w;
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        int band = j < r || j >= h - r; u32 *row = &buf[yy * W]; int inh = hole_w > 0 && yy >= hole_y && yy < hole_y + hole_h;
        /* straight middle part of the row (no corner maths) is blended as spans; only the rounded ends go per pixel */
        int sa = band ? x + r : xa, sb = band ? x + w - r : xb; if (sa < xa) sa = xa; if (sb > xb) sb = xb;
        for (int xx = xa; xx < xb; xx++) {
            if (xx >= sa && xx < sb) {                                           /* straight run [xx, e) */
                int e = sb;
                if (inh) { if (xx < hole_x && hole_x < e) e = hole_x; else if (xx >= hole_x && xx < hole_x + hole_w) { xx = hole_x + hole_w - 1; continue; } }
                blend_span(&row[xx], e - xx, col, alpha); xx = e - 1; continue;
            }
            if (inh && xx >= hole_x && xx < hole_x + hole_w) { xx = hole_x + hole_w - 1; continue; }
            int i = xx - x; u32 a = alpha;
            if (band && (i < r || i >= w - r)) { u32 c = rr_cov(xx, yy, x, y, w, h, r); if (!c) continue; a = c * alpha / 255; }
            row[xx] = a >= 255 ? col : mix(row[xx], col, a);
        }
    }
}

static void draw_line(u32 *buf, int x0, int y0, int x1, int y1, u32 col, u32 a) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        for (int k = 0; k < 2; k++) {
            int xx = x0 + k, yy = y0;
            if (xx >= 0 && yy >= 0 && xx < (int)W && yy < (int)H) { u32 *p = &buf[yy * W + xx]; *p = mix(*p, col, a); }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}


/* PixelBeeOS signature: compact gold-and-white bee mark, drawn natively at any UI size. */
static void bee_mark(u32 *b, int x, int y, int z) {
    int u = z / 24; if (u < 1) u = 1;
    draw_rr(b, x + 3*u, y + 2*u, 10*u, 6*u, 3*u, 0xFFFFFF, 255);
    draw_rr(b, x + 1*u, y + 8*u, 10*u, 5*u, 3*u, 0xF4F7FF, 245);
    draw_line(b, x+15*u, y+7*u, x+17*u, y+1*u, 0xF6C928, 255);
    draw_line(b, x+18*u, y+8*u, x+22*u, y+4*u, 0xF6C928, 255);
    draw_rr(b, x+15*u, y, 4*u, 4*u, 2*u, 0xFFD12E, 255);
    draw_rr(b, x+20*u, y+2*u, 4*u, 4*u, 2*u, 0xFFD12E, 255);
    draw_rr(b, x+13*u, y+6*u, 10*u, 11*u, 5*u, 0xFFD12E, 255);
    draw_rr(b, x+6*u, y+13*u, 13*u, 9*u, 4*u, 0xF5BE19, 255);
    draw_line(b, x+8*u, y+16*u, x+17*u, y+16*u, 0x17233E, 255);
    draw_line(b, x+8*u, y+19*u, x+16*u, y+19*u, 0x17233E, 255);
    draw_rr(b, x+18*u, y+9*u, 2*u, 2*u, 1*u, 0x17233E, 255);
}

static int sharp_text = 1;   /* 1 = crisp, bolder text (soft font edges are snapped); 0 = original soft edges */
static inline u32 sharpen(u32 a) { if (!sharp_text) return a; if (a < 48) return 0; if (a > 150) return 255; return (a - 48) * 255 / 102; }
static void glyph(u32 *buf, int x, int y, int c, u32 col) {
    if (c < 32 || c > 126) c = '?';
    const u8 *g = font[c - 32];
    for (int j = 0; j < 16; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        for (int i = 0; i < 8; i++) {
            u32 a = sharpen(g[j * 8 + i]); if (!a) continue;
            int xx = x + i; if (xx < 0 || xx >= (int)W) continue;
            u32 *p = &buf[yy * W + xx]; *p = a == 255 ? col : mix(*p, col, a);
        }
    }
}
static void text(u32 *buf, int x, int y, const char *s, u32 col) { for (; *s; s++, x += 8) glyph(buf, x, y, *s, col); }

/* ---- shared UI pieces used by every app so they look like one system ---- */
static void heading(u32 *b, int x, int y, const char *s, u32 col) {        /* bold section title with a fine divider */
    text(b, x, y, s, col); text(b, x + 1, y, s, col);
    draw_rr(b, x, y + 24, 44, 3, 1, col, 220); draw_rr(b, x + 48, y + 25, 8, 1, 0, 0xFFFFFF, 30);
}
static void row_card(u32 *b, int x, int y, int w) { draw_rr(b, x, y, w, 29, 9, 0xFFFFFF, 9); }   /* soft rounded row behind a label/value pair */
static void titlebar_fill(u32 *b, int x, int y, int w, int h, int rd, int foc) {   /* gradient title bar, correctly rounded at the top corners */
    int a0 = foc ? 40 : 18, a1 = foc ? 12 : 6;
    for (int j = 0; j < 34; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        u32 a = (u32)(a0 + (a1 - a0) * j / 34);
        int xs = x + rd, xe = x + w - rd; if (xs < 0) xs = 0; if (xe > (int)W) xe = (int)W;
        if (xe > xs) blend_span(&b[yy * W + xs], xe - xs, 0xFFFFFF, a);
        for (int i = 0; i < rd; i++) for (int side = 0; side < 2; side++) {
            int xx = side ? x + w - 1 - i : x + i; if (xx < 0 || xx >= (int)W) continue;
            u32 c = j < rd ? rr_cov(xx, yy, x, y, w, h, rd) : 255; if (!c) continue;
            u32 *p = &b[yy * W + xx]; *p = mix(*p, 0xFFFFFF, a * c / 255);
        }
    }
}

/* ---- frosted glass: blur what's behind, tint, top sheen, 1px rim light ---- */
static void box_line(const u32 *in, u32 *out, int n, int r) {
    int win = 2 * r + 1; u32 sr = 0, sg = 0, sb = 0;
    for (int i = -r; i <= r; i++) { int j = i < 0 ? 0 : (i >= n ? n - 1 : i); u32 c = in[j]; sr += (c >> 16) & 255; sg += (c >> 8) & 255; sb += c & 255; }
    for (int i = 0; i < n; i++) {
        out[i] = ((sr / win) << 16) | ((sg / win) << 8) | (sb / win);
        int a = i - r, b = i + r + 1; a = a < 0 ? 0 : a; b = b >= n ? n - 1 : b;
        u32 ca = in[a], cb = in[b];
        sr += ((cb >> 16) & 255) - ((ca >> 16) & 255); sg += ((cb >> 8) & 255) - ((ca >> 8) & 255); sb += (cb & 255) - (ca & 255);
    }
}
static void blur_rect(u32 *reg, int w, int h, int r) {
    for (int j = 0; j < h; j++) { box_line(reg + j * w, lb, w, r); memcpy(reg + j * w, lb, w * 4); }
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) la[j] = reg[j * w + i];
        box_line(la, lb, h, r);
        for (int j = 0; j < h; j++) reg[j * w + i] = lb[j];
    }
}
static void glass(int x, int y, int w, int h, int r, u32 tint, int ta, int sheen, int ba) {
    if (flat_ui) {                                                    /* flat: opaque rounded surface + 1px edge, no blur */
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
            u32 co = rr_cov(i, j, 0, 0, w, h, r); if (!co) continue;
            u32 ci = rr_cov(i, j, 1, 1, w - 2, h - 2, r > 0 ? r - 1 : 0), *p = &bgbuf[(y + j) * W + x + i];
            *p = mix(mix(*p, 0x48484A, co), tint, ci);
        }
        return;
    }
    u32 *reg = back;
    for (int j = 0; j < h; j++) memcpy(reg + j * w, bgbuf + (y + j) * W + x, w * 4);
    if (blur_r > 0) blur_rect(reg, w, h, blur_r);
    lift(reg, w * h, 165, 10);
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        u32 co = rr_cov(i, j, 0, 0, w, h, r); if (!co) continue;
        u32 ci = rr_cov(i, j, 1, 1, w - 2, h - 2, r > 0 ? r - 1 : 0);
        u32 *p = &bgbuf[(y + j) * W + x + i];
        u32 d = mix(*p, reg[j * w + i], co);
        d = mix(d, tint, (u32)ta * ci / 255);
        int s = h - j; u32 sh = (u32)(sheen * (s * s / h) / h); if (sh > 255) sh = 255;
        d = mix(d, 0xFFFFFF, sh * ci / 255);
        u32 bd = co > ci ? co - ci : 0;
        d = mix(d, 0xFFFFFF, rim_a(i, j, w, h, (u32)ba) * bd / 255);
        if (i < r || i >= w - r || j < r || j >= h - r) {                       /* inner glow just inside the rim */
            u32 cn = rr_cov(i, j, 3, 3, w - 6, h - 6, r > 3 ? r - 3 : 0), gl = ci > cn ? ci - cn : 0;
            d = mix(d, 0xFFFFFF, rim_a(i, j, w, h, 46) * gl / 255);
        }
        *p = d;
    }
}

/* ---- PixelBeeOS original landscape wallpapers ---- */
static int tri(int x, int p) { x %= p; if (x < 0) x += p; return x < p / 2 ? x : p - x; }
static int ridge(int x, int p, int off) { return tri(x + off, p) * 255 / (p / 2); }
static int topy[MAXW];

typedef struct { const char *name; u32 sky[3]; u32 col[3][3]; u32 fog, tint, clk; } Theme;
#define THEME_COUNT 11
static const Theme themes[THEME_COUNT] = {
    {"glacier", {0x061A33, 0x2E78A8, 0xCDEAF6}, {{0x6F9BBD, 0x8DB8D6, 0x56809F}, {0xA6CDE5, 0xC9E6F5, 0x84B0CE}, {0xD7F0FA, 0xF4FBFF, 0xA6D3E8}}, 0xE2F4FB, 0x07172B, 0x0B2A44},
    {"night",   {0x02040D, 0x0B1D3F, 0x2A4A7A}, {{0x1B2F55, 0x2A4573, 0x13223F}, {0x2B4676, 0x3D5E92, 0x1E3560}, {0x3E5F94, 0x5B7FB4, 0x2C4A7C}}, 0x3A5A8E, 0x050B18, 0xDCEEFF},
    {"aurora",  {0x03121F, 0x0E6B5C, 0x7BE8C2}, {{0x2F7F86, 0x47A3A0, 0x216066}, {0x5FB7B0, 0x84D6C4, 0x3E8F8C}, {0xA9E6DC, 0xD4FAF1, 0x7FC9C0}}, 0xC8F3EA, 0x06201E, 0x07342E},
    {"sunset",  {0x2B1245, 0xC4476F, 0xFFC27A}, {{0x8C4F7F, 0xB06A8B, 0x6A3A6B}, {0xC9778E, 0xE897A0, 0x9A5778}, {0xF3B7A5, 0xFFD8C0, 0xC98595}}, 0xFFE0C2, 0x2A1030, 0x4A1F3A},
    {"nebula", {0x090D29, 0x343D85, 0xB95EAE}, {{0x273C70, 0x435BA0, 0x182B59}, {0x654E9A, 0x846DB8, 0x493A7C}, {0xB46BAA, 0xD48FC3, 0x89518C}}, 0xD9B8E8, 0x101638, 0xD9E6FF},
    {"prism",  {0x070B20, 0x25366E, 0x7A4E9C}, {{0x15264E, 0x294D78, 0x101C3D}, {0x263A70, 0x3B5F91, 0x1A2C58}, {0x534B88, 0x7466A8, 0x38406D}}, 0x111A38, 0x28396A, 0xE6EEFF},
    {"lumen",  {0x10152D, 0x48628E, 0xD4A7C9}, {{0x354C79, 0x587AA8, 0x22395F}, {0x526B98, 0x7D96BE, 0x38527E}, {0x967DAA, 0xBCA2C8, 0x6E628F}}, 0xC6C2DA, 0x394A76, 0xF0E8FF},
    {"bloom", {0x0A0E1C, 0x1A2250, 0x39358F}, {{0x1A2250, 0x252E68, 0x10163A}, {0x2A3278, 0x39358F, 0x1C2358}, {0x4A3F9E, 0x5E4FB5, 0x35307A}}, 0x141A3C, 0x0D1226, 0xF2F4FF},
    {"liquid", {0x0B1233, 0x3A1F7A, 0xFF7AB8}, {{0x1A2250, 0x252E68, 0x10163A}, {0x2A3278, 0x39358F, 0x1C2358}, {0x4A3F9E, 0x5E4FB5, 0x35307A}}, 0x141A3C, 0x3A2FA8, 0xFFFFFF},
    {"slate", {0x0A0E14, 0x1C2738, 0x3E5C86}, {{0x1D2A3C, 0x243247, 0x121A26}, {0x243247, 0x2C3D55, 0x182233}, {0x2C3D55, 0x364A66, 0x1F2B3E}}, 0x141C28, 0x1B2536, 0xEAF0F8},
    {"frost", {0x101A2C, 0x3A5E9E, 0x2B7889}, {{0x1A1048, 0x251668, 0x100A30}, {0x251668, 0x35208A, 0x180E44}, {0x35208A, 0x4A2CA8, 0x251668}}, 0x120733, 0x2B1170, 0xFFFFFF},
};
static int theme_i = 10;  /* 10 = frost: calm navy gradient with clear frosted glass (reference-reel glass recipe); 9 = slate */


/* ---- "frost" wallpaper: calm, professional deep-navy gradient with a few soft, desaturated light blooms (no shapes, no neon) ---- */
static int isqrt_i(int v);
static int cl_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int ss255(int t) { if (t <= 0) return 0; if (t >= 255) return 255; return t * t * (765 - 2 * t) / 65025; }
static int wave_y8(int x, int w, int h, const int *kv) {          /* crest height (1/256 px) of a smooth 5-knot ribbon; kv in percent of the screen height */
    static const int kt[5] = {0, 250, 500, 750, 1000};
    int t = x * 1000 / (w > 0 ? w : 1), k = 0; if (t > 999) t = 999; while (k < 3 && t >= kt[k + 1]) k++;
    int seg = cl_i((t - kt[k]) * 255 / (kt[k + 1] - kt[k]), 0, 255), s8 = ss255(seg);
    int v = kv[k] * 256 + (kv[k + 1] - kv[k]) * 256 * s8 / 255;
    return (int)((long long)h * v / 100);
}
static void paint_reel(void) {                       /* "frost": deep indigo night with three soft layered ribbons (macOS-style), so the glass has real structure to bend */
    int w = (int)W, h = (int)H; if (w < 1) w = 1; if (h < 1) h = 1;
    static const int kv[3][5] = {{46, 40, 50, 58, 50}, {64, 70, 60, 54, 62}, {84, 78, 82, 74, 80}};
    static const u32 top[3] = {0x3B63F0, 0x8446F0, 0xE0529E}, deep[3] = {0x1D2D96, 0x3E1FA8, 0x6A1F9A};
    static const u8 bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int crest[3];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (y == 0 || (x & 63) == 0) {}                       /* (crest rows are recomputed per column below) */
            u32 c = mix(0x0B1030, 0x1B1058, (u32)((y * 255 * 16 / h + bayer[(y & 3) * 4 + (x & 3)]) >> 4));
            for (int k = 0; k < 3; k++) {
                crest[k] = wave_y8(x, w, h, kv[k]);
                int dd8 = y * 256 - crest[k];
                if (dd8 >= -256) {
                    int dd = dd8 / 256, span = h * 22 / 100 + 1;
                    u32 f = mix(top[k], deep[k], (u32)ss255(cl_i(dd * 255 / span, 0, 255)));
                    u32 cov = (u32)cl_i(dd8 / 2 + 128, 0, 255);
                    if (dd8 >= 0 && dd < 3) f = mix(f, 0xFFFFFF, (u32)(70 - dd * 22));            /* lit crest edge */
                    c = mix(c, f, cov * 235 / 255);
                } else if (dd8 > -(h * 4 / 100) * 256) {                                          /* soft glow above the crest */
                    int g = 255 + (dd8 / 256) * 255 / (h * 4 / 100); c = mix(c, top[k], (u32)(g * g / 255 * 45 / 255));
                }
            }
            bgbuf[y * W + x] = c;
        }
    }
}

static void paint_bg(void) {
    const Theme *T = &themes[theme_i];
    if (theme_i == 10) { paint_reel(); return; }
    if (theme_i >= 7) {   /* "bloom": deep graphite-indigo base with three soft light blooms, no rings or wave lines */
        int w = (int)W, h = (int)H; if (w < 1) w = 1; if (h < 1) h = 1;
        typedef struct { int cx, cy, r, col, str; } Bloom;
        static const Bloom bls[3][4] = {{{22, 18, 62, 0x5B4BFF, 150}, {86, 30, 52, 0x2D86FF, 120}, {60, 96, 70, 0xB04CD8, 105}, {0, 0, 0, 0, 0}},
                                        {{14, 20, 58, 0x1DB8FF, 235}, {84, 18, 54, 0xFF4FA8, 215}, {70, 88, 62, 0xFF9A3C, 205}, {20, 92, 56, 0x7A4DFF, 215}},
                                        {{28, 0, 90, 0x4B6E9E, 70}, {50, 106, 38, 0x6F9BE0, 100}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}}};     /* slate: one wide gentle highlight + a soft glow low in the screen so the dock glass has something to refract */
        const Bloom *bl = bls[theme_i - 7]; u32 base0 = theme_i == 9 ? 0x1D2A3C : (theme_i == 8 ? 0x0B1233 : 0x080B18), base1 = theme_i == 9 ? 0x0B0F16 : (theme_i == 8 ? 0x23286E : 0x151C42);
        static const u8 bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            u32 c = mix(base0, base1, (u32)((y * 200 * 16 / h + x * 55 * 16 / w + bayer[(y & 3) * 4 + (x & 3)]) >> 4));     /* 4x4 ordered dither: no visible banding in the dark gradient */
            for (int k = 0; k < 4; k++) {
                if (!bl[k].r) continue;
                int R = w * bl[k].r / 100, dx = x - w * bl[k].cx / 100, dy = y - h * bl[k].cy / 100;
                if (dx > R || dx < -R || dy > R || dy < -R) continue;
                int d2 = dx * dx + dy * dy, R2 = R * R; if (d2 >= R2) continue;
                int t = 255 - (int)((long long)d2 * 255 / R2);
                c = mix(c, (u32)bl[k].col, (u32)(t * t / 255 * bl[k].str / 255));
            }
            bgbuf[y * W + x] = c;
        }
        return;
    }
    int hz = (int)H * 62 / 100;
    for (int y = 0; y < (int)H; y++) {
        u32 c;
        if (y >= hz) c = T->fog;
        else { int t = y * 255 / hz; c = t < 128 ? mix(T->sky[0], T->sky[1], t * 2) : mix(T->sky[1], T->sky[2], (t - 128) * 2); }
        for (u32 x = 0; x < W; x++) {
            u32 px = c;
            if (theme_i == 5 && (int)y < (int)(H * 48 / 100)) {
                int wave = (int)H * 13 / 100 + ridge((int)x, (int)(W / 2 ? W / 2 : 1), 43) * ((int)H * 13 / 100) / 255;
                int d = (int)y - wave; if (d < 0) d = -d;
                if (d < (int)H * 5 / 100) {
                    int a = ((int)H * 5 / 100 - d) * 2; if (a > 72) a = 72;
                    u32 glow = theme_i == 6 ? mix(0xFFB6D9, 0x78D8E8, (int)x * 255 / (int)W)
                                               : mix(0x35D5D4, 0xA56BFF, (int)x * 255 / (int)W);
                    px = mix(px, glow, (u32)a);
                }
                int wave2 = (int)H * 31 / 100 + ridge((int)x + 117, (int)(W / 3 ? W / 3 : 1), 91) * ((int)H * 7 / 100) / 255;
                int d2 = (int)y - wave2; if (d2 < 0) d2 = -d2;
                if (d2 < (int)H * 3 / 100) {
                    int a = ((int)H * 3 / 100 - d2) * 2; if (a > 42) a = 42;
                    px = mix(px, theme_i == 6 ? 0x9A83EF : 0x4A80E8, (u32)a);
                }
            }
            bgbuf[y * W + x] = px;
        }
    }
    for (int i = 0; i < 120; i++) {              /* drifting snow specks */
        int x = (i * 7919 + 131) % (int)W, y = (i * 104729 + 977) % hz, a = 90 + (i * 37) % 130;
        for (int k = 0; k < (i % 5 == 0 ? 2 : 1); k++)
            for (int l = 0; l < (i % 5 == 0 ? 2 : 1); l++) { u32 *p = &bgbuf[(y + l) * W + x + k]; *p = mix(*p, 0xFFFFFF, a); }
    }
    static const int base[3] = {60, 72, 84}, amp[3] = {23, 19, 12};
    static const int pa[3][4] = {{620, 260, 97, 40}, {470, 190, 71, 200}, {360, 150, 59, 333}};
    for (int L = 0; L < 3; L++) {
        for (u32 x = 0; x < W; x++) {
            int h = (ridge(x, pa[L][0], pa[L][3]) * 5 + ridge(x, pa[L][1], pa[L][3] / 2) * 2 + ridge(x, pa[L][2], 5)) / 8;
            topy[x] = (int)H * base[L] / 100 - h * ((int)H * amp[L] / 100) / 255;
        }
        for (u32 x = 0; x < W; x++) {
            int sl = topy[x > 18 ? x - 18 : 0] - topy[x + 18 < W ? x + 18 : W - 1];   /* broad slope: lit west faces, shaded east faces */
            u32 c0 = sl > 5 ? T->col[L][1] : (sl < -5 ? T->col[L][2] : T->col[L][0]);
            for (int y = topy[x]; y < (int)H; y++) {
                int dy = y - topy[x];
                u32 c = c0;
                u32 fog = (u32)(dy * 255 / ((int)H * 30 / 100)); if (fog > 190) fog = 190;
                c = mix(c, T->fog, fog);
                if (dy < 14) c = mix(c, 0xFFFFFF, (u32)(14 - dy) * 16);
                bgbuf[y * W + x] = c;
            }
        }
    }
}

/* ============================ layout / scene ============================ */
static int wx, wy, ww, wh, tx, ty, cols, rows, dkx, dky, dkw, dkh, dock_apps_shown, dock_r = 28;
#define COLS_MAX 160
#define ROWS_MAX 60
static u32 pal[] = {0xF2F2F7, 0x64D2FF, 0xC7C7CC, 0xFF6961, 0x30D158, 0xFFD60A};   /* macOS dark system colours */
enum { A_TXT, A_ACC, A_DIM, A_ERR, A_OK, A_WARN };

/* ============================ window manager: every app gets its own floating window ============================ */
/* Apps set app_on to open/close themselves; task_sync() (once per frame) turns that into windows.
 * win_[] is in z-order (last = top = keyboard focus). term_focus = the terminal has the keyboard. */
static int slen(const char *s); static int dirty;
static void copy_rr(u32 *dst, const u32 *src, int x, int y, int w, int h, int r);
static void glass_finish(u32 *b, int x, int y, int w, int h, int r, int sheen, int rim);
#define WCACHE_PX 540000
static u32 wcache[8][WCACHE_PX]; static u32 gscratch[MAXW * MAXH];
static struct { int ok, x, y, w, h, alpha, blur; u32 tint; } wck[8];
static u32 cur_ms;
typedef struct { int app, x, y, w, h, max, min, rx, ry, rw, rh; u32 t0; } Win;   /* t0: when the window was created or restored (drives the open animation) */
static Win win_[8]; static int nwin, term_focus, drag_app, drag_ox, drag_oy, dock_peek;
static int win_find(int app) { for (int i = 0; i < nwin; i++) if (win_[i].app == app) return i; return -1; }
static void win_raise(int i) { Win t = win_[i]; for (int j = i; j < nwin - 1; j++) win_[j] = win_[j + 1]; win_[nwin - 1] = t; }
static void win_clamp(Win *w) {
    if (w->x > (int)W - w->w) w->x = (int)W - w->w; if (w->y > dky - 8 - w->h) w->y = dky - 8 - w->h;
    if (w->x < 0) w->x = 0; if (w->y < 0) w->y = 0;
}
static void win_create(int app) {
    const App *ap = app_at(app - 1); int w = ap->pw ? ap->pw : 640, h = ap->ph ? ap->ph : 440;
    if (w > (int)W - 20) w = (int)W - 20; if (h > dky - 16) h = dky - 16;
    if (nwin >= 6) { for (int j = 0; j < nwin - 1; j++) win_[j] = win_[j + 1]; nwin--; }
    int n = nwin % 5; Win nw = {0}; nw.app = app; nw.w = w; nw.h = h; nw.t0 = cur_ms;
    nw.x = ((int)W - w) / 2 + (n - 2) * 34; nw.y = 68 + n * 24; win_clamp(&nw); win_[nwin++] = nw;
}
static void win_focus_top(void) {          /* after a close/minimize: topmost visible window gets the keyboard, else the terminal */
    if (nwin > 0 && !win_[nwin - 1].min) { term_focus = 0; app_on = win_[nwin - 1].app; }
    else { term_focus = nwin > 0; app_on = 0; }
    dirty = 1;
}
/* ============================ animations ============================
   Window open: scale-up + fade + rise.  Window close / minimize: the last frame of the window shrinks (and flies into the dock
   when minimized) while it fades.  Launchpad, Search, Control Center and the desktop menu fade and rise into place.
   All of it is skipped when flat_ui / performance mode is on. */
#define ANIM_OPEN_MS 240
#define ANIM_CLOSE_MS 200
#define ANIM_OV_MS 260
static u32 ab_bd[WCACHE_PX], ab_win[WCACHE_PX], ghost[WCACHE_PX], ov_snap[MAXW * MAXH];
static int anim_busy, ghost_on, ghost_x, ghost_y, ghost_w, ghost_h, ghost_min; static u32 ghost_t0;
static int ov_prev[4]; static u32 ov_t0[4];
static int row_diff(const u32 *a, const u32 *b, int n) { for (int i = 0; i < n; i++) if (a[i] != b[i]) return 1; return 0; }
static int ease_out(u32 age, u32 dur) { int t = age >= dur ? 255 : (int)(age * 255 / dur), u = 255 - t; return 255 - u * u / 255 * u / 255; }
/* easeOutBack: arrives with a small overshoot (about 4%) and settles, like a spring. 0..255 scale, may briefly exceed 255. */
static int ease_back(u32 age, u32 dur) {
    int t = age >= dur ? 1024 : (int)(age * 1024 / dur), u = t - 1024, u2 = u * u / 1024, u3 = u2 * u / 1024;
    return (1024 + (22 * u3) / 10 + (12 * u2) / 10) * 255 / 1024;
}
/* Bilinear sample of a layer at 24.8 fixed-point (fx, fy). Taps that equal the backdrop layer (not part of the window) or fall outside count as the
   destination background d, so scaled window edges blend smoothly instead of stair-stepping. */
static inline u32 layer_tap(const u32 *win, const u32 *bgl, int w, int h, int x, int y, u32 d) {
    if (x < 0 || y < 0 || x >= w || y >= h) return d;
    u32 p = win[y * w + x]; return p == bgl[y * w + x] ? d : p;
}
static int anim_lowq;                                   /* set when frames are slow: animations drop to nearest-neighbour / plain fades so the pointer stays responsive */
static u32 layer_bilinear(const u32 *win, const u32 *bgl, int w, int h, int fx, int fy, u32 d) {
    int x0 = fx >> 8, y0 = fy >> 8; u32 ax = (u32)(fx & 255), ay = (u32)(fy & 255);
    if (anim_lowq) { ax = ay = 0; if ((fx & 255) >= 128) x0++; if ((fy & 255) >= 128) y0++; }
    u32 c00 = layer_tap(win, bgl, w, h, x0, y0, d);
    if (!(ax | ay)) return c00;
    u32 c10 = layer_tap(win, bgl, w, h, x0 + 1, y0, d), c01 = layer_tap(win, bgl, w, h, x0, y0 + 1, d), c11 = layer_tap(win, bgl, w, h, x0 + 1, y0 + 1, d);
    return mix(mix(c00, c10, ax), mix(c01, c11, ax), ay);
}
static void rect_get(u32 *dst, const u32 *src, int x, int y, int w, int h) { for (int j = 0; j < h; j++) memcpy(&dst[j * w], &src[(y + j) * W + x], (unsigned)w * 4); }
static int anim_box(const Win *w, int *x, int *y, int *bw, int *bh) {
    if (flat_ui || w->max) return 0;
    int x0 = w->x - 18, y0 = w->y - 8, x1 = w->x + w->w + 18, y1 = w->y + w->h + 30;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > (int)W) x1 = (int)W; if (y1 > (int)H) y1 = (int)H;
    *x = x0; *y = y0; *bw = x1 - x0; *bh = y1 - y0; return *bw > 0 && *bh > 0 && *bw * *bh <= WCACHE_PX;
}
static void ghost_start(const Win *w, int minimize) {     /* call before a visible window is closed or minimized: keeps its last frame */
    int x, y, bw, bh; if (w->min || !anim_box(w, &x, &y, &bw, &bh)) return;
    rect_get(ghost, back, x, y, bw, bh); ghost_x = x; ghost_y = y; ghost_w = bw; ghost_h = bh; ghost_min = minimize; ghost_t0 = cur_ms; ghost_on = 1; dirty = 1;
}
static void ghost_draw(u32 *b, u32 ms) {
    if (!ghost_on) return;
    u32 age = ms - ghost_t0; if (age >= ANIM_CLOSE_MS) { ghost_on = 0; return; }
    anim_busy = 1;
    int e = ease_out(age, ANIM_CLOSE_MS), gcx = ghost_x + ghost_w / 2, gcy = ghost_y + ghost_h / 2;
    int tx = ghost_min ? dkx + dkw / 2 : gcx, ty = ghost_min ? dky + dkh / 2 : gcy;
    int sc = ghost_min ? 256 - e * 196 / 255 : 256 - e * 22 / 255;
    int dcx = gcx + (tx - gcx) * e / 255, dcy = gcy + (ty - gcy) * e / 255;
    int alpha = 255 - (ghost_min ? e * 215 / 255 : e);
    int dw = ghost_w * sc / 256 + 2, dh = ghost_h * sc / 256 + 2;
    int x0 = dcx - dw / 2, y0 = dcy - dh / 2, x1 = x0 + dw, y1 = y0 + dh;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > (int)W) x1 = (int)W; if (y1 > (int)H) y1 = (int)H;
    rect_get(ab_bd, b, ghost_x, ghost_y, ghost_w, ghost_h);                 /* what is behind the window now */
    static int sxt[MAXW]; for (int x = x0; x < x1; x++) sxt[x] = gcx * 256 + (2 * (x - dcx) + 1) * 32768 / sc - 128 - ghost_x * 256;
    for (int y = y0; y < y1; y++) {
        if ((y & 15) == 0) ps2_pump();
        int fy = gcy * 256 + (2 * (y - dcy) + 1) * 32768 / sc - 128 - ghost_y * 256; if (fy < -256 || fy >= ghost_h * 256) continue;
        for (int x = x0; x < x1; x++) {
            int fx = sxt[x]; if (fx < -256 || fx >= ghost_w * 256) continue;
            u32 d = b[y * W + x], gp = layer_bilinear(ghost, ab_bd, ghost_w, ghost_h, fx, fy, d);
            if (gp != d) b[y * W + x] = mix(d, gp, (u32)alpha);
        }
    }
}
static u32 prev[MAXW * MAXH];                            /* what the screen showed last frame (defined again with the flip code) */
static u32 ov_lay[WCACHE_PX], ov_lay_bg[WCACHE_PX];     /* closing panel: its last picture and the backdrop it sat on */
static int ov_c_slot = -1, ov_c_x, ov_c_y, ov_c_w, ov_c_h, ov_last; static u32 ov_c_t0;
#define ANIM_OVC_MS 190
static int ov_isl_rect(int *x, int *y, int *w, int *h);
/* Draw layer lay (w x h at x0,y0) scaled about its centre by sc/256, sunk by dy pixels and faded by alpha over what is already in back[]. Bilinear. */
static void ov_scaled(const u32 *lay, const u32 *lbg, int x0, int y0, int w, int h, int sc, int dy, int alpha) {
    int cxg = x0 + w / 2, cyg = y0 + h / 2, m = 16;
    int X0 = x0 - m < 0 ? 0 : x0 - m, X1 = x0 + w + m > (int)W ? (int)W : x0 + w + m, Y0 = y0 - m < 0 ? 0 : y0 - m, Y1 = y0 + h + m > (int)H ? (int)H : y0 + h + m;
    static int sxt[MAXW];
    for (int x = X0; x < X1; x++) sxt[x] = (cxg - x0) * 256 + (2 * (x - cxg) + 1) * 32768 / sc - 128;
    for (int y = Y0; y < Y1; y++) {
        if ((y & 15) == 0) ps2_pump();
        int fy = (cyg - y0) * 256 + (2 * (y - dy - cyg) + 1) * 32768 / sc - 128; if (fy < -256 || fy >= h * 256) continue;
        for (int x = X0; x < X1; x++) {
            int fx = sxt[x]; if (fx < -256 || fx >= w * 256) continue;
            u32 d = back[y * W + x], s = layer_bilinear(lay, lbg, w, h, fx, fy, d);
            if (s != d) back[y * W + x] = mix(d, s, (u32)alpha);
        }
    }
}
static int ov_rx, ov_ry, ov_rw, ov_rh, ov_rect[4][4];   /* biggest glass panel drawn while a slot is open: where its closing animation happens */
static void ov_close_start(int slot) {                  /* panel just closed: keep its last picture (from prev[]) and shrink + fade it out */
    ov_c_slot = -1; if (flat_ui || anim_lowq || !ov_rect[slot][2]) return;
    int ix, iy, iw, ih, hasi = ov_isl_rect(&ix, &iy, &iw, &ih), wd = (int)W, ht = (int)H;
    int x0 = ov_rect[slot][0] - 12, y0 = ov_rect[slot][1] - 10, x1 = ov_rect[slot][0] + ov_rect[slot][2] + 12, y1 = ov_rect[slot][1] + ov_rect[slot][3] + 18;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > wd) x1 = wd; if (y1 > ht) y1 = ht;
    int w = x1 - x0, h = y1 - y0; if (w < 8 || h < 8 || w * h > WCACHE_PX) return;
    for (int j = 0; j < h; j++) { if ((j & 31) == 0) ps2_pump(); for (int i = 0; i < w; i++) {
        int X = x0 + i, Y = y0 + j; u32 bgp = back[Y * wd + X], pp = prev[Y * wd + X];
        if (hasi && X >= ix && X < ix + iw && Y >= iy && Y < iy + ih) pp = bgp;          /* the notification card is not part of the panel */
        ov_lay[j * w + i] = pp; ov_lay_bg[j * w + i] = bgp;
    } }
    ov_c_slot = slot; ov_c_x = x0; ov_c_y = y0; ov_c_w = w; ov_c_h = h; ov_c_t0 = cur_ms; anim_busy = 1;
}
static int ov_begin(int slot, int open) {               /* returns slot while that panel is animating in, else -1 */
    ov_last = slot; ov_rw = ov_rh = 0;
    if (!open) { if (ov_prev[slot]) { ov_prev[slot] = 0; ov_close_start(slot); } return -1; }
    if (ov_c_slot == slot) ov_c_slot = -1;               /* reopened while closing */
    if (!ov_prev[slot]) ov_t0[slot] = cur_ms;
    ov_prev[slot] = 1;
    if (flat_ui || cur_ms - ov_t0[slot] >= ANIM_OV_MS) return -1;
    memcpy(ov_snap, back, (unsigned)W * H * 4); anim_busy = 1; return slot;
}
static void ov_end(int slot) {                          /* opening: panel grows from ~94% with a small spring overshoot, fading and rising; closing: shrinks and fades */
    if (ov_rw > 0 && ov_prev[ov_last]) { ov_rect[ov_last][0] = ov_rx; ov_rect[ov_last][1] = ov_ry; ov_rect[ov_last][2] = ov_rw; ov_rect[ov_last][3] = ov_rh; }
    if (slot < 0) {
        if (ov_c_slot == ov_last && ov_c_slot >= 0) {
            u32 age = cur_ms - ov_c_t0; if (age >= ANIM_OVC_MS) { ov_c_slot = -1; return; }
            int e = ease_out(age, ANIM_OVC_MS); anim_busy = 1;
            ov_scaled(ov_lay, ov_lay_bg, ov_c_x, ov_c_y, ov_c_w, ov_c_h, 256 - e * 15 / 255, e * 10 / 255, 255 - e);
        }
        return;
    }
    int e = ease_out(cur_ms - ov_t0[slot], ANIM_OV_MS), es = ease_back(cur_ms - ov_t0[slot], ANIM_OV_MS), dy = (255 - e) * 14 / 255, wd = (int)W, ht = (int)H;
    int y0 = -1, y1 = -1; for (int y = 0; y < ht; y++) if (row_diff(&back[y * wd], &ov_snap[y * wd], wd)) { if (y0 < 0) y0 = y; y1 = y; }
    if (y0 < 0) return;
    int x0 = wd, x1 = -1;
    for (int y = y0; y <= y1; y++) {
        int a = 0; while (a < wd && back[y * wd + a] == ov_snap[y * wd + a]) a++; if (a < x0) x0 = a;
        int c = wd - 1; while (c > a && back[y * wd + c] == ov_snap[y * wd + c]) c--; if (c > x1) x1 = c;
    }
    if (x1 < x0) return;
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w * h <= WCACHE_PX && !anim_lowq) {              /* normal case: smooth scale */
        for (int j = 0; j < h; j++) { memcpy(&ov_lay[j * w], &back[(y0 + j) * wd + x0], (unsigned)w * 4); memcpy(&ov_lay_bg[j * w], &ov_snap[(y0 + j) * wd + x0], (unsigned)w * 4); }
        int m = 16, X0 = x0 - m < 0 ? 0 : x0 - m, X1 = x1 + 1 + m > wd ? wd : x1 + 1 + m, Y0 = y0 - m < 0 ? 0 : y0 - m, Y1 = y1 + 1 + m > ht ? ht : y1 + 1 + m;
        for (int y = Y0; y < Y1; y++) memcpy(&back[y * wd + X0], &ov_snap[y * wd + X0], (unsigned)(X1 - X0) * 4);   /* undo the full-size panel */
        ov_scaled(ov_lay, ov_lay_bg, x0, y0, w, h, 240 + es * 16 / 255, dy, e > 255 ? 255 : e);
        return;
    }
    int yb = y1 + dy; if (yb >= ht) yb = ht - 1;         /* huge panel: old cheap fade + rise */
    for (int y = yb; y >= y0; y--) {
        int sy = y - dy;
        for (int x = x0; x <= x1; x++) back[y * wd + x] = sy < y0 ? ov_snap[y * wd + x] : mix(ov_snap[y * wd + x], back[sy * wd + x], (u32)e);
    }
}
static void win_min_i(int i) { Win t = win_[i]; t.min = 1; for (int j = i; j > 0; j--) win_[j] = win_[j - 1]; win_[0] = t; }   /* minimized windows sit at the bottom */
static void win_close(int app) {
    int i = win_find(app); if (i < 0) return;
    ghost_start(&win_[i], 0);
    for (int j = i; j < nwin - 1; j++) win_[j] = win_[j + 1];
    nwin--; win_focus_top();
}
static void win_minimize(int app) { int i = win_find(app); if (i < 0) return; ghost_start(&win_[i], 1); win_min_i(i); win_focus_top(); }
static void win_toggle_max(int app) {
    int i = win_find(app); if (i < 0) return; Win *w = &win_[i];
    if (w->max) { w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh; w->max = 0; }
    else { w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h; w->x = 0; w->y = 0; w->w = (int)W; w->h = (int)H; w->max = 1; }
    dirty = 1;
}
static int dock_hidden(void) { for (int i = nwin - 1; i >= 0; i--) { if (win_[i].min) continue; return win_[i].max; } return 0; }
static void focus_terminal(void) {          /* show the terminal: maximized windows get minimized so it is reachable */
    int rebuild = !terminal_visible; terminal_visible = 1; if (rebuild) build_scene(); term_focus = 1; app_on = 0;
    for (;;) { int f = -1; for (int i = 0; i < nwin; i++) if (win_[i].max && !win_[i].min) { f = i; break; } if (f < 0) break; win_min_i(f); }
    dirty = 1;
}
static void win_cycle(void) {               /* Alt+Tab */
    int vis = 0, lo = -1; for (int i = 0; i < nwin; i++) if (!win_[i].min) { vis++; if (lo < 0) lo = i; }
    if (vis >= 2) { win_raise(lo); term_focus = 0; app_on = win_[nwin - 1].app; }
    else if (vis == 1) { if (term_focus) { term_focus = 0; app_on = win_[nwin - 1].app; } else focus_terminal(); }
    else if (nwin > 0) { win_[nwin - 1].min = 0; win_[nwin - 1].t0 = cur_ms; term_focus = 0; app_on = win_[nwin - 1].app; }
    dirty = 1;
}
static void task_sync(void) {
    if (app_on == 0) {
        if (nwin > 0 && !term_focus) { nwin--; win_focus_top(); }   /* focused app closed itself */
        return;
    }
    term_focus = 0;
    int i = win_find(app_on);
    if (i < 0) win_create(app_on); else { if (win_[i].min) { win_[i].min = 0; win_[i].t0 = cur_ms; } if (i != nwin - 1) win_raise(i); }
}
static void win_focus_at(int x, int y) {
    for (int i = nwin - 1; i >= 0; i--) {
        Win w = win_[i]; if (w.min) continue;
        if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) { term_focus = 0; if (i != nwin - 1) win_raise(i); app_on = w.app; return; }
    }
    if (nwin > 0 && x >= wx && x < wx + ww && y >= wy && y < wy + wh) { term_focus = 1; app_on = 0; }
}
/* glass window: backdrop + tint + sheen + rim is rendered once into a cache and only re-rendered when the window moves or the style changes */
static void reel_glass(u32 *dst, const u32 *src, int x, int y, int w, int h, int r, int shadow, int dark);
#define WIN_DARK 105   /* dark base under app/Terminal glass so text stays crisp */
static void win_glass(u32 *b, const Win *w, int rd) {
    int s = w->app - 1, n = w->w * w->h, rl = theme_i == 10;
    if (s < 0 || s > 7 || n > WCACHE_PX) { if (rl) { reel_glass(b, bgblur, w->x, w->y, w->w, w->h, rd, 0, WIN_DARK); return; } copy_rr(b, bgblur, w->x, w->y, w->w, w->h, rd); draw_rr(b, w->x, w->y, w->w, w->h, rd, WIN_TINT, (u32)win_alpha); glass_finish(b, w->x, w->y, w->w, w->h, rd, 52, 200); return; }
    u32 *c = wcache[s]; u32 tint = rl ? 0x0A1018 : WIN_TINT;
    if (!(wck[s].ok && wck[s].x == w->x && wck[s].y == w->y && wck[s].w == w->w && wck[s].h == w->h && wck[s].alpha == win_alpha && wck[s].blur == blur_r && wck[s].tint == tint)) {
        for (int j = 0; j < w->h; j++) memcpy(&gscratch[(w->y + j) * W + w->x], &bgblur[(w->y + j) * W + w->x], w->w * 4);
        if (rl) reel_glass(gscratch, bgblur, w->x, w->y, w->w, w->h, rd, 0, WIN_DARK);
        else { draw_rr(gscratch, w->x, w->y, w->w, w->h, rd, tint, (u32)win_alpha); glass_finish(gscratch, w->x, w->y, w->w, w->h, rd, 52, 200); }
        for (int j = 0; j < w->h; j++) memcpy(&c[j * w->w], &gscratch[(w->y + j) * W + w->x], w->w * 4);
        wck[s].ok = 1; wck[s].x = w->x; wck[s].y = w->y; wck[s].w = w->w; wck[s].h = w->h; wck[s].alpha = win_alpha; wck[s].blur = blur_r; wck[s].tint = tint;
    }
    int r = rd; if (2 * r > w->w) r = w->w / 2; if (2 * r > w->h) r = w->h / 2;
    for (int j = 0; j < w->h; j++) {
        int yy = w->y + j; if (yy < 0 || yy >= (int)H) continue;
        u32 *src = &c[j * w->w], *dst = &b[yy * W + w->x];
        if (!(j < r || j >= w->h - r)) { memcpy(dst, src, w->w * 4); continue; }
        for (int i = 0; i < w->w; i++) {
            if (i < r || i >= w->w - r) { u32 cv = rr_cov(w->x + i, yy, w->x, w->y, w->w, w->h, r); if (cv) dst[i] = mix(dst[i], src[i], cv); } else dst[i] = src[i];
        }
    }
}
/* Mac-style window controls, shared by the Terminal and all apps: red close, yellow minimise, green zoom. live3 = 0 greys the zoom light. */
static void traffic_lights(u32 *b, int x, int y, int foc, int live3) {
    for (int d = 0; d < 3; d++) {
        int dx = x + 14 + d * 22, dy = y + 9;
        u32 cc = d == 0 ? 0xFF6E79 : (d == 1 ? 0xF6C66F : 0x62D7B0); u32 a = hot(dx, dy, 14, 14) ? 255 : (foc ? 226 : 150);
        if (d == 2 && !live3) { cc = 0x7B8498; a = 120; }
        draw_rr(b, dx, dy, 14, 14, 7, cc, a);
        if (d == 0 && hot(dx, dy, 14, 14)) { draw_line(b, dx + 4, dy + 4, dx + 10, dy + 10, 0x4B2030, 230); draw_line(b, dx + 10, dy + 4, dx + 4, dy + 10, 0x4B2030, 230); }
        else if (d == 1 && hot(dx, dy, 14, 14)) draw_line(b, dx + 4, dy + 7, dx + 10, dy + 7, 0x59431A, 230);
        else if (d == 2 && live3 && hot(dx, dy, 14, 14)) { draw_line(b, dx + 4, dy + 7, dx + 10, dy + 7, 0x174B3A, 230); draw_line(b, dx + 7, dy + 4, dx + 7, dy + 10, 0x174B3A, 230); }
    }
}
static void wins_draw(u32 *b, u32 ms) {
    int ox = wx, oy = wy, ow = ww, oh = wh;
    for (int i = 0; i < nwin; i++) {
        Win w = win_[i]; if (w.min) continue; const App *ap = app_at(w.app - 1);
        int abx, aby, abw, abh, an = 0; u32 aage = ms - w.t0;
        if (aage < ANIM_OPEN_MS && anim_box(&w, &abx, &aby, &abw, &abh)) { an = 1; anim_busy = 1; rect_get(ab_bd, b, abx, aby, abw, abh); }
        int foc = (i == nwin - 1 && !term_focus), rd = w.max ? 0 : 14;
        if (!w.max) { hole_x = w.x; hole_y = w.y; hole_w = w.w; hole_h = w.h; for (int s = 0; s < 3; s++) draw_rr(b, w.x - s + 2, w.y - s + 10, w.w + 2 * s, w.h + 2 * s, 16 + s, 0x000000, flat_ui ? 22 : 14); hole_w = 0; }
        if (win_glass_on && !flat_ui && blur_r > 0 && !w.max) win_glass(b, &w, rd);
        else draw_rr(b, w.x, w.y, w.w, w.h, rd, WIN_TINT, 255);
        titlebar_fill(b, w.x, w.y, w.w, w.h, rd, foc);
        const char *title = (w.app == 7 && fa_pc_view) ? "This PC" : ap->name;
        text(b, w.x + (w.w - 8 * slen(title)) / 2, w.y + 9, title, foc ? pal[A_TXT] : pal[A_DIM]);
        traffic_lights(b, w.x, w.y, foc, 1);
        draw_line(b, w.x + 1, w.y + 34, w.x + w.w - 2, w.y + 34, 0xFFFFFF, foc ? 34 : 18);   /* title-bar divider, same on every window */
        hit_add(w.x, w.y, w.w, w.h, 1100 + w.app); hit_add(w.x, w.y, w.w, 34, 1200 + w.app);
        hit_add(w.x + 10, w.y + 5, 20, 22, 1000 + w.app);
        hit_add(w.x + 32, w.y + 5, 20, 22, 1300 + w.app);
        hit_add(w.x + 54, w.y + 5, 20, 22, 1400 + w.app);
        wx = w.x; wy = w.y; ww = w.w; wh = w.h;
        ps2_pump(); if (ap->draw) ap->draw(b, ms);
        if (an) {                                              /* open animation: scale up from 92%, fade in, rise a few pixels */
            rect_get(ab_win, b, abx, aby, abw, abh);
            int es = ease_back(aage, ANIM_OPEN_MS), e = ease_out(aage, ANIM_OPEN_MS), sc = 236 + es * 20 / 255, off = (255 - e) * 14 / 255, cx = w.x + w.w / 2, cy = w.y + w.h / 2;
            static int sxt[MAXW]; for (int x = 0; x < abw; x++) sxt[x] = cx * 256 + (2 * ((abx + x) - cx) + 1) * 32768 / sc - 128 - abx * 256;
            for (int y = 0; y < abh; y++) {
                if ((y & 15) == 0) ps2_pump();
                int fy = cy * 256 + (2 * ((aby + y) - off - cy) + 1) * 32768 / sc - 128 - aby * 256;
                for (int x = 0; x < abw; x++) {
                    u32 bd = ab_bd[y * abw + x];
                    u32 wp = layer_bilinear(ab_win, ab_bd, abw, abh, sxt[x], fy, bd);
                    b[(aby + y) * W + abx + x] = wp == bd ? bd : mix(bd, wp, (u32)e);
                }
            }
        }
    }
    wx = ox; wy = oy; ww = ow; wh = oh;
}
/* the dock is baked into bgbuf; when a maximized window hides it, paint it back on top (rounded) while the mouse is at the bottom edge */
static void dock_overlay(u32 *b) {
    for (int i = 0; i < 8; i++) draw_rr(b, dkx - i, dky - i + 8, dkw + 2 * i, dkh + 2 * i, dock_r + i, 0x04101E, 6);
    int r = dock_r;
    for (int y = dky; y < dky + dkh && y < (int)H; y++) for (int x = dkx; x < dkx + dkw && x < (int)W; x++) {
        int ddx = x < dkx + r ? dkx + r - x : (x >= dkx + dkw - r ? x - (dkx + dkw - 1 - r) : 0), ddy = y < dky + r ? dky + r - y : (y >= dky + dkh - r ? y - (dky + dkh - 1 - r) : 0);
        if (ddx * ddx + ddy * ddy <= r * r) b[y * W + x] = bgbuf[y * W + x];
    }
}

/* ---- glass for floating windows: pre-blurred wallpaper backdrop + tint + sheen + rim light ---- */
static void copy_rr(u32 *dst, const u32 *src, int x, int y, int w, int h, int r) {
    if (2 * r > w) r = w / 2; if (2 * r > h) r = h / 2; if (r < 0) r = 0;
    int xa = x < 0 ? 0 : x, xb = x + w > (int)W ? (int)W : x + w; if (xb <= xa) return;
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        if (!(j < r || j >= h - r)) { memcpy(&dst[yy * W + xa], &src[yy * W + xa], (xb - xa) * 4); continue; }
        for (int xx = xa; xx < xb; xx++) {
            int i = xx - x; u32 *d = &dst[yy * W + xx];
            if (i < r || i >= w - r) { u32 c = rr_cov(xx, yy, x, y, w, h, r); if (c) *d = mix(*d, src[yy * W + xx], c); } else *d = src[yy * W + xx];
        }
    }
}
static void glass_finish(u32 *b, int x, int y, int w, int h, int r, int sheen, int rim) {
    if (flat_ui) { sheen = 0; rim = 34; }
    if (2 * r > w) r = w / 2; if (2 * r > h) r = h / 2; if (r < 0) r = 0;
    int hh = h < 170 ? h : 170, xa = x + r < 0 ? 0 : x + r, xb = x + w - r > (int)W ? (int)W : x + w - r;
    for (int j = 0; j < hh; j++) {                                    /* soft light from the top edge */
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue; int s = hh - j; u32 sh = (u32)(sheen * (s * s / hh) / hh); if (sh > 255) sh = 255; if (!sh) continue;
        for (int xx = xa; xx < xb; xx++) b[yy * W + xx] = mix(b[yy * W + xx], 0xFFFFFF, sh);
    }
    for (int j = 0; j < h; j++) {                                     /* 1px rim light, only where it can exist */
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        for (int i = 0; i < w; i++) {
            int edge = i < 4 || i >= w - 4 || j < 4 || j >= h - 4, corner = (i < r || i >= w - r) && (j < r || j >= h - r);
            if (!edge && !corner) { if (i == 4) i = w - 5; continue; }
            int xx = x + i; if (xx < 0 || xx >= (int)W) continue;
            u32 co = rr_cov(i, j, 0, 0, w, h, r), ci = rr_cov(i, j, 1, 1, w - 2, h - 2, r > 0 ? r - 1 : 0), bd = co > ci ? co - ci : 0;
            u32 cn = rr_cov(i, j, 3, 3, w - 6, h - 6, r > 3 ? r - 3 : 0), gl = ci > cn ? ci - cn : 0;
            u32 *pp = &b[yy * W + xx];
            if (bd) *pp = mix(*pp, 0xFFFFFF, rim_a(i, j, w, h, (u32)rim) * bd / 255);
            if (gl && !flat_ui) *pp = mix(*pp, 0xFFFFFF, rim_a(i, j, w, h, 46) * gl / 255);
        }
    }
}

/* ---- PixelBeeOS 3.1 LIQUID GLASS ----
   Clear, bright glass: the wallpaper shows through almost untouched in the middle, while a lens band along the edge
   bends the backdrop inward and splits it into slightly shifted red/blue fringes. A thin specular rim, an inner glow
   and a soft top highlight finish it. Only the edge band does per-pixel maths, so big panels stay fast. */
static int lg_soft;                                              /* professional (graphite) look: neutral glass, no colour fringing, minimal lens */
static int isqrt_i(int v) { if (v <= 0) return 0; int x = v, y = (x + 1) >> 1; while (y < x) { x = y; y = (x + v / x) >> 1; } return x; }
static u32 lg_sample(const u32 *src, int sx, int sy, int ch) {
    if (sx < 0) sx = 0; if (sx >= (int)W) sx = (int)W - 1; if (sy < 0) sy = 0; if (sy >= (int)H) sy = (int)H - 1;
    u32 c = src[sy * W + sx]; return ch == 0 ? (c >> 16) & 255 : (ch == 1 ? (c >> 8) & 255 : c & 255);
}
/* ---- PixelBeeOS "frost" glass: a port of the Kyant AndroidLiquidGlass (backdrop) recipe, in integer math ----
   per pixel: rounded-rect SDF -> inside the refraction band the backdrop is sampled from a point bent inward along the SDF gradient
   (circleMap curve, the library's lens() effect) -> vibrancy (saturation 1.5) -> translucent surface -> 45-degree rim highlight. */
static u32 lgl_sample(const u32 *src, int x8, int y8) {           /* bilinear sample, coordinates in 1/8 px */
    int xi = x8 >> 3, yi = y8 >> 3, fx = (x8 & 7) * 32, fy = (y8 & 7) * 32;
    if (xi < 0) { xi = 0; fx = 0; } if (yi < 0) { yi = 0; fy = 0; } if (xi > (int)W - 2) { xi = (int)W - 2; fx = 0; } if (yi > (int)H - 2) { yi = (int)H - 2; fy = 0; }
    const u32 *p = &src[yi * W + xi];
    return mix(mix(p[0], p[1], (u32)fx), mix(p[W], p[W + 1], (u32)fx), (u32)fy);
}
static u32 lgl_vib(u32 c, int sat) {                                 /* saturation boost, 256 = 1.0 (library uses 1.5) */
    int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255), l = (54 * r + 183 * g + 19 * b) >> 8;
    r = l + (((r - l) * sat) >> 8); g = l + (((g - l) * sat) >> 8); b = l + (((b - l) * sat) >> 8);
    return ((u32)cl_i(r, 0, 255) << 16) | ((u32)cl_i(g, 0, 255) << 8) | (u32)cl_i(b, 0, 255);
}
static void reel_glass(u32 *dst, const u32 *src, int x, int y, int w, int h, int r, int shadow, int dark) {
    if (2 * r > w) r = w / 2; if (2 * r > h) r = h / 2; if (r < 4) r = 4;
    int mn = w < h ? w : h, refH = mn * 38 / 100; if (refH > 26) refH = 26; if (refH < 6) refH = 6;
    int amt = refH * 9 / 10, gr = r * 3 / 2; if (gr > mn / 2) gr = mn / 2;
    int hb = 14, safe = refH + 2;                                       /* hb: rim highlight width in 1/8 px */
    if (shadow) for (int i = 0; i < 7; i++) draw_rr(dst, x - i, y - i + 7, w + 2 * i, h + 2 * i, r + i, 0x03020E, 4);
    int hw8 = w * 4, hh8 = h * 4, r8 = r * 8, g8 = gr * 8, refH8 = refH * 8;
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i; if (xx < 0 || xx >= (int)W) continue;
            u32 *p = &dst[yy * W + xx], c = src[yy * W + xx];
            if (i > safe && i < w - 1 - safe && j > safe && j < h - 1 - safe) {   /* interior: no refraction, just vibrancy + surface */
                c = lgl_vib(c, 300); c = mix(mix(c, 0x0A1018, (u32)dark), 0xFFFFFF, 38); *p = c; continue;
            }
            int px = (2 * i + 1 - w) * 4, py = (2 * j + 1 - h) * 4, sx = px < 0 ? -1 : 1, sy = py < 0 ? -1 : 1, ax = px * sx, ay = py * sy;
            int qx = ax - (hw8 - r8), qy = ay - (hh8 - r8);
            int ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, in = qx > qy ? qx : qy; if (in > 0) in = 0;
            int d8 = r8 - isqrt_i(ox * ox + oy * oy) - in;                      /* distance inside the edge, 1/8 px */
            int co = cl_i(d8 + 4, 0, 8) * 255 / 8; if (!co) continue;
            int gx = 0, gy = 0;                                                 /* outward unit gradient of the SDF, x256 (corner radius grown like the shader) */
            { int cx2 = ax - (hw8 - g8), cy2 = ay - (hh8 - g8);
              if (cx2 >= 0 || cy2 >= 0) { int vx = cx2 > 0 ? cx2 : 0, vy = cy2 > 0 ? cy2 : 0, len = isqrt_i(vx * vx + vy * vy); if (len < 1) len = 1; gx = vx * 256 / len * sx; gy = vy * 256 / len * sy; }
              else if (cx2 >= cy2) gx = 256 * sx; else gy = 256 * sy; }
            if (d8 < refH8) {                                                   /* refraction band */
                int t = 256 - d8 * 32 / refH, circ = 256 - isqrt_i(65536 - t * t), disp = circ * amt / 32;   /* circleMap * amount, 1/8 px */
                c = lgl_sample(src, xx * 8 + 4 - disp * gx / 256, yy * 8 + 4 - disp * gy / 256);
            }
            c = lgl_vib(c, 300);
            c = mix(mix(c, 0x0A1018, (u32)dark), 0xFFFFFF, 38);                 /* translucent surface */
            if (d8 < hb) { int dd = ((gx + gy) * 181) >> 8; if (dd < 0) dd = -dd; c = mix(c, 0xFFFFFF, (u32)(dd * 150 / 256 * (hb - d8) / hb)); }   /* 45-degree rim highlight */
            *p = co == 255 ? c : mix(*p, c, (u32)co);
        }
    }
}
static void liquid_panel(u32 *dst, const u32 *src, int x, int y, int w, int h, int r, int white_a, int dark_a, int shadow) {
    if (w * h > ov_rw * ov_rh) { ov_rx = x; ov_ry = y; ov_rw = w; ov_rh = h; }
    if (flat_ui) { copy_rr(dst, src, x, y, w, h, r); draw_rr(dst, x, y, w, h, r, 0x2C2C2E, PA(232)); glass_finish(dst, x, y, w, h, r, 0, 34); return; }
    if (theme_i == 10) { reel_glass(dst, src, x, y, w, h, r, shadow, 0); return; }
    int bst = h <= 80;                                   /* small floating glass (dock, top pills) gets the full "fluid" treatment: bevel, lens, light streak, glints */
    lg_soft = theme_i == 9 && !bst; if (2 * r > w) r = w / 2; if (2 * r > h) r = h / 2; if (r < 4) r = 4;
    if (bst) { white_a += 6; dark_a = dark_a * 38 / 100; }
    int bw = r * 3 / 4; if (bw > 22) bw = 22; if (bw < 3) bw = 3;
    int inner = r - bw; if (inner < 0) inner = 0;
    if (shadow) for (int i = 0; i < 7; i++) draw_rr(dst, x - i, y - i + 7, w + 2 * i, h + 2 * i, r + i, 0x050A1E, 6);
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || yy >= (int)H) continue;
        int s = h - j; u32 top = (u32)((bst ? 46 : (lg_soft ? 16 : 34)) * (s * s / h) / h); if (top > 90) top = 90;            /* soft light from the top edge */
        u32 body_w = (u32)white_a + top, body_d = (u32)dark_a * (u32)j / (u32)h / 2 + (u32)dark_a / 2;   /* a little darker towards the bottom */
        int syv = 255 - j * 255 * 5 / (h * 2); if (syv < 0) syv = 0;
        for (int i = 0; i < w; i++) {
            int xx = x + i; if (xx < 0 || xx >= (int)W) continue;
            int cx = i < r ? r : (i >= w - r ? w - r - 1 : i), cy = j < r ? r : (j >= h - r ? h - r - 1 : j);
            int dx = i - cx, dy = j - cy, d2 = dx * dx + dy * dy;
            u32 *p = &dst[yy * W + xx], c, co = 255;
            int sxv = 255 - i * 255 * 5 / (w * 3); if (sxv < 0) sxv = 0; u32 sk = bst ? (u32)((sxv * syv >> 8) * 44 / 255) : 0;   /* diagonal specular streak, strongest top-left */
            if (d2 <= inner * inner) { c = src[yy * W + xx]; c = mix(c, lg_soft ? 0x0B1018 : 0x0A1430, body_d); c = mix(c, 0xFFFFFF, body_w + sk); }
            else {
                co = rr_cov(i, j, 0, 0, w, h, r); if (!co) continue;
                int dist = isqrt_i(d2), depth = r - dist; if (depth < 0) depth = 0;
                int t = 255 - depth * 255 / bw; if (t < 0) t = 0;                                  /* 255 on the rim, 0 where the lens ends */
                int fr = bst ? 1 : (lg_soft ? 0 : 2), disp = ((t * t) >> 8) * bw * (bst ? 12 : (lg_soft ? 3 : 9)) / 2550; if (dist < 1) dist = 1;                /* refraction strength in pixels */
                int ux = -dx * 256 / dist, uy = -dy * 256 / dist;                                  /* unit vector pointing into the glass */
                u32 rc = lg_sample(src, xx + (ux * (disp + fr)) / 256, yy + (uy * (disp + fr)) / 256, 0);
                u32 gc = lg_sample(src, xx + (ux * disp) / 256, yy + (uy * disp) / 256, 1);
                u32 bc = lg_sample(src, xx + (ux * (disp > fr ? disp - fr : 0)) / 256, yy + (uy * (disp > fr ? disp - fr : 0)) / 256, 2);
                c = (rc << 16) | (gc << 8) | bc;
                u32 glow = (u32)((t * t >> 8) * (bst ? 54 : (lg_soft ? 10 : 34)) / 255); if (bst && dy > 0) glow += (u32)((t * t >> 8) * 22 / 255);          /* lens glow; extra glint on the lower edge */
                c = mix(c, lg_soft ? 0x0B1018 : 0x0A1430, body_d); c = mix(c, 0xFFFFFF, body_w + glow + sk);
                u32 ci = rr_cov(i, j, 1, 1, w - 2, h - 2, r - 1), bd = co > ci ? co - ci : 0;
                int in2 = bst ? 6 : 4; u32 cn = rr_cov(i, j, in2, in2, w - 2 * in2, h - 2 * in2, r > in2 ? r - in2 : 0), gl = ci > cn ? ci - cn : 0;
                c = mix(c, 0xFFFFFF, rim_a(i, j, w, h, bst ? 90 : 40) * gl / 255);                            /* inner glow */
                c = mix(c, 0xFFFFFF, rim_a(i, j, w, h, 255) * bd / 255);                           /* specular rim */
            }
            *p = co == 255 ? c : mix(*p, c, co);
        }
    }
}

static u32 bgclean[MAXW * MAXH]; static int bgclean_ok, dock_run[8], dock_nrun, dock_sig = -1;
/* The dock shows two fixed items (Terminal, This PC) plus ONLY the apps that are currently open. It is re-baked on the clean scene when that set changes. */
/* ============================ macOS-style dock ============================
   Frosted rounded bar, big vivid gradient icons with a white glyph, running dots, thin separators, hover magnification and name labels.
   The bar and icons are drawn every frame (dock_draw), so they can magnify; dock_bake only decides what is in the dock and how wide it is. */
static void glyph_s(u32 *buf, int x, int y, int c, u32 col, int sc);
static int bar_day, bar_mon;
#define DK_MAX 20
static int dk_n, dk_kind[DK_MAX], dk_id[DK_MAX], dk_app[DK_MAX], dk_sepb[DK_MAX], dk_x[DK_MAX], dk_w[DK_MAX], dk_bx, dk_bw;
static const char *dk_tip[DK_MAX];
#define DK_S 38
#define DK_GAP 6
#define DK_SEP 12
#define DK_PAD 10
static void dcirc(u32 *b, int cx, int cy, int r, u32 col, u32 a) { if (r < 1) r = 1; draw_rr(b, cx - r, cy - r, 2 * r, 2 * r, r, col, a); }
static void mac_tile(u32 *b, int x, int y, int s, u32 c1, u32 c2) {
    int r = s * 23 / 100;
    for (int i = 5; i >= 1; i--) draw_rr(b, x - i / 2, y + i + s / 22, s + i, s + i / 2, r + i / 2, 0x000000, 7);   /* soft drop shadow */
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) {
        int xx = x + i, yy = y + j; if (xx < 0 || yy < 0 || xx >= (int)W || yy >= (int)H) continue;
        u32 cv = rr_cov(xx, yy, x, y, s, s, r); if (!cv) continue;
        u32 c = mix(c1, c2, (u32)(j * 255 / s));
        if (j < s / 2) c = mix(c, 0xFFFFFF, (u32)((s / 2 - j) * 46 / (s / 2)));                  /* gloss */
        if (j == 0 || i == 0 || i == s - 1) c = mix(c, 0xFFFFFF, 26);                              /* thin rim */
        b[yy * W + xx] = cv == 255 ? c : mix(b[yy * W + xx], c, cv);
    }
}
static const char *mon3[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
static void mac_icon(u32 *b, int x, int y, int s, int kind) {
#define U(v) ((v) * s / 52)
#define RR(px, py, pw, ph, pr, col, al) draw_rr(b, x + U(px), y + U(py), U(pw), U(ph), U(pr), col, al)
#define LN(x0, y0, x1, y1, col) do { int ax_ = x + U(x0), ay_ = y + U(y0), bx_ = x + U(x1), by_ = y + U(y1); draw_line(b, ax_, ay_, bx_, by_, col, 255); draw_line(b, ax_ + 1, ay_, bx_ + 1, by_, col, 255); draw_line(b, ax_, ay_ + 1, bx_, by_ + 1, col, 255); draw_line(b, ax_ + 1, ay_ + 1, bx_ + 1, by_ + 1, col, 255); } while (0)
#define CC(cx, cy, r, col) dcirc(b, x + U(cx), y + U(cy), U(r), col, 255)
    const u32 WH = 0xFFFFFF;
#define MIDC(c1, c2, py) mix(c1, c2, (u32)((py) * 255 / 52))     /* the tile gradient colour at a given row, for knock-out shapes */
    switch (kind) {
    case 0:  mac_tile(b, x, y, s, 0x2F5CC4, 0x0B1B45); bee_mark(b, x + U(9), y + U(9), U(34)); break;
    case 1:  mac_tile(b, x, y, s, 0x4C4C52, 0x0E0E10); glyph_s(b, x + (s - 32) / 2, y + (s - 32) / 2 - 1, '>', 0xFFFFFF, 2); glyph_s(b, x + (s - 32) / 2 + 16, y + (s - 32) / 2 - 1, '_', 0xFFFFFF, 2); break;
    case 2:  mac_tile(b, x, y, s, 0x6AD2FF, 0x1766F0); RR(11, 12, 30, 21, 4, WH, 255); RR(14, 15, 24, 15, 2, 0x2F7DEB, 255); RR(23, 34, 6, 4, 1, WH, 255); RR(17, 38, 18, 3, 1, WH, 255); break;
    case 10: mac_tile(b, x, y, s, 0xFFFFFF, 0xE4E4E9); { int m = bar_mon >= 1 && bar_mon <= 12 ? bar_mon - 1 : 0, d = bar_day < 1 ? 1 : bar_day; text(b, x + (s - 24) / 2, y + U(7), mon3[m], 0xFF3B30);
        if (d >= 10) { glyph_s(b, x + (s - 32) / 2, y + U(21), (char)('0' + d / 10), 0x1C1C1E, 2); glyph_s(b, x + (s - 32) / 2 + 16, y + U(21), (char)('0' + d % 10), 0x1C1C1E, 2); } else glyph_s(b, x + (s - 16) / 2, y + U(21), (char)('0' + d), 0x1C1C1E, 2); } break;
    case 11: mac_tile(b, x, y, s, 0x62D8FF, 0x0B5BF0); CC(26, 26, 18, WH); CC(26, 26, 15, MIDC(0x62D8FF, 0x0B5BF0, 26)); LN(18, 34, 34, 18, 0xFF3B30); LN(26, 26, 34, 18, WH); LN(18, 34, 26, 26, 0xE4EEFF); break;
    case 12: mac_tile(b, x, y, s, 0xFF84BC, 0x9A5BFF); CC(26, 26, 18, WH); CC(18, 21, 4, 0xFF3B30); CC(27, 16, 4, 0xFFC800); CC(35, 24, 4, 0x30D158); CC(21, 33, 4, 0x0A84FF); CC(33, 34, 4, 0xC9A8F5); break;
    case 13: mac_tile(b, x, y, s, 0x6A6A70, 0x1C1C1F); RR(9, 9, 34, 12, 3, 0x232326, 255); for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) RR(9 + c * 9, 25 + r * 8, 7, 6, 2, c == 3 ? 0xFF9F0A : 0xA2A2A8, 255); break;
    case 14: mac_tile(b, x, y, s, 0x3C3C3F, 0x08080A); CC(26, 26, 19, WH); LN(26, 26, 26, 13, 0x1C1C1E); LN(26, 26, 35, 30, 0x1C1C1E); CC(26, 26, 2, 0xFF9F0A); break;
    case 15: mac_tile(b, x, y, s, 0xFFE761, 0xFFB400); RR(12, 9, 28, 35, 3, WH, 250); RR(12, 9, 28, 6, 2, 0xFFD60A, 255); for (int i = 0; i < 4; i++) RR(16, 20 + i * 6, 20, 2, 1, 0xC9C9CF, 255); break;
    case 16: mac_tile(b, x, y, s, 0x86DAFF, 0x2D88FF); RR(9, 13, 15, 9, 3, WH, 255); RR(9, 17, 34, 24, 4, 0xE4F3FF, 255); RR(9, 22, 34, 19, 4, WH, 255); break;
    case 17: mac_tile(b, x, y, s, 0x55E070, 0x0A7A38); CC(14, 36, 4, WH); CC(22, 36, 4, WH); CC(30, 36, 4, WH); CC(38, 30, 4, WH); CC(38, 22, 5, WH); CC(16, 15, 4, 0xFF3B30); break;
    case 18: mac_tile(b, x, y, s, 0xBBBBC0, 0x58585D); LN(11, 26, 41, 26, 0xF2F2F7); LN(26, 11, 26, 41, 0xF2F2F7); LN(15, 15, 37, 37, 0xF2F2F7); LN(37, 15, 15, 37, 0xF2F2F7); CC(26, 26, 13, 0xF2F2F7); CC(26, 26, 6, MIDC(0xBBBBC0, 0x58585D, 26)); break;
    case 19: mac_tile(b, x, y, s, 0x39393C, 0x0A0A0B); LN(9, 40, 43, 40, 0x58585D); LN(9, 33, 17, 25, 0x30D158); LN(17, 25, 25, 31, 0x30D158); LN(25, 31, 33, 15, 0x30D158); LN(33, 15, 43, 21, 0x30D158); break;
    case 20: mac_tile(b, x, y, s, 0xFFB03A, 0xFF3B30); RR(11, 28, 9, 13, 2, WH, 255); RR(22, 20, 9, 21, 2, WH, 255); RR(33, 11, 9, 30, 2, WH, 255); break;
    case 21: mac_tile(b, x, y, s, 0x7573F5, 0x3230AA); for (int i = 0; i < 4; i++) { RR(11, 11 + i * 9, 6, 6, 3, WH, 255); RR(21, 12 + i * 9, 21, 5, 2, WH, 215); } break;
    case 22: mac_tile(b, x, y, s, 0x93A0B8, 0x3A4458); RR(15, 15, 22, 22, 4, WH, 255); RR(20, 20, 12, 12, 2, 0x4B5770, 255); for (int i = 0; i < 3; i++) { RR(18 + i * 6, 9, 2, 6, 1, WH, 255); RR(18 + i * 6, 37, 2, 6, 1, WH, 255); RR(9, 18 + i * 6, 6, 2, 1, WH, 255); RR(37, 18 + i * 6, 6, 2, 1, WH, 255); } break;
    case 23: mac_tile(b, x, y, s, 0x46A8FF, 0x0A44D8); RR(13, 10, 26, 20, 5, WH, 255); RR(16, 24, 20, 17, 9, WH, 255); LN(20, 24, 25, 29, 0x0A44D8); LN(25, 29, 33, 18, 0x0A44D8); break;
    case 24: mac_tile(b, x, y, s, 0x41E3D2, 0x0A78FF); CC(26, 26, 18, WH); CC(26, 26, 16, MIDC(0x41E3D2, 0x0A78FF, 26)); RR(18, 8, 16, 36, 8, WH, 255); RR(20, 9, 12, 34, 6, MIDC(0x41E3D2, 0x0A78FF, 26), 255); LN(9, 26, 43, 26, WH); break;
    case 25: mac_tile(b, x, y, s, 0x8E9BB8, 0x2B3550); CC(26, 28, 11, 0x0E0E12); LN(26, 12, 26, 44, 0x0E0E12); LN(10, 28, 42, 28, 0x0E0E12); LN(15, 17, 37, 39, 0x0E0E12); LN(37, 17, 15, 39, 0x0E0E12); CC(22, 24, 3, WH); break;
    case 30: mac_tile(b, x, y, s, 0xA4A4AA, 0x4A4A50); CC(23, 23, 12, WH); CC(23, 23, 9, MIDC(0xA4A4AA, 0x4A4A50, 23)); LN(32, 32, 41, 41, WH); LN(33, 31, 42, 40, WH); break;
    case 31: mac_tile(b, x, y, s, 0x808086, 0x2A2A2E); RR(10, 12, 32, 13, 6, WH, 255); CC(35, 18, 5, 0x2A2A2E); RR(10, 29, 32, 13, 6, 0x0A84FF, 255); CC(17, 35, 5, WH); break;
    case 32: mac_tile(b, x, y, s, 0xFF7468, 0xCF0F28); CC(26, 28, 13, WH); CC(26, 28, 10, MIDC(0xFF7468, 0xCF0F28, 28)); RR(22, 12, 8, 14, 0, MIDC(0xFF7468, 0xCF0F28, 19), 255); RR(24, 12, 4, 17, 2, WH, 255); break;
    case 33: mac_tile(b, x, y, s, 0x6BD2FF, 0x0A6FF0); CC(26, 27, 13, WH); CC(26, 27, 10, MIDC(0x6BD2FF, 0x0A6FF0, 27)); RR(26, 10, 14, 16, 0, MIDC(0x6BD2FF, 0x0A6FF0, 18), 255); LN(27, 14, 36, 14, WH); LN(36, 14, 36, 22, WH); LN(27, 14, 33, 8, WH); LN(27, 14, 33, 20, WH); break;
    default: mac_tile(b, x, y, s, 0x8E8E93, 0x48484A); break;
    }
#undef MIDC
#undef U
#undef RR
#undef LN
#undef CC
}
static void dk_add(int kind, int id, int app, const char *tip) { if (dk_n >= DK_MAX) return; dk_kind[dk_n] = kind; dk_id[dk_n] = id; dk_app[dk_n] = app; dk_sepb[dk_n] = 0; dk_tip[dk_n] = tip; dk_n++; }
static int dk_base_w(void) { int w = 2 * DK_PAD; for (int i = 0; i < dk_n; i++) { w += DK_S; if (i < dk_n - 1) w += dk_sepb[i] ? DK_SEP : DK_GAP; } return w; }
static void dock_bake(void) {
    /* pinned: launcher, Terminal, This PC | running apps | Search, Control Center, power, restart */
    dock_apps_shown = dock_nrun;
    for (;;) {
        dk_n = 0;
        dk_add(0, 1, 0, "PixelBeeOS"); dk_add(1, 2, -1, "Terminal"); dk_add(2, 3, 7, "This PC");
        if (dock_apps_shown) dk_sepb[dk_n - 1] = 1;
        for (int k = 0; k < dock_apps_shown; k++) dk_add(10 + dock_run[k] - 1, 500 + dock_run[k] - 1, dock_run[k], app_at(dock_run[k] - 1)->name);
        dk_sepb[dk_n - 1] = 1;
        dk_add(30, 2002, 0, "Search"); dk_add(31, 2004, 0, "Control Center"); dk_add(32, PWR_OFF_ID, 0, "Shut down"); dk_add(33, PWR_RST_ID, 0, "Restart");
        dkw = dk_base_w();
        if (dkw <= (int)W - 20 || dock_apps_shown == 0) break;
        dock_apps_shown--;
    }
    if (dkw > (int)W - 20) dkw = (int)W - 20;
    dkh = DK_S + 14; dkx = ((int)W - dkw) / 2; dky = (int)H - 8 - dkh; dock_r = 17;
}
static void dock_place(void) {                           /* hover magnification: icons near the pointer grow (up to ~1.45x) and push their neighbours apart */
    int bxc[DK_MAX], x = dkx + DK_PAD, total = 2 * DK_PAD;
    for (int i = 0; i < dk_n; i++) { bxc[i] = x + DK_S / 2; x += DK_S + (dk_sepb[i] ? DK_SEP : DK_GAP); }
    int active = mouse_seen && my >= dky - 40 && my < (int)H, R = 110;
    for (int i = 0; i < dk_n; i++) {
        int sc = 256;
        if (active) { int d = mx - bxc[i]; if (d < 0) d = -d; if (d < R) { int t = 255 - d * 255 / R; t = t * t / 255; t = t * (765 - 2 * t) / 765; sc = 256 + 100 * t / 255; } }
        dk_w[i] = DK_S * sc / 256; total += dk_w[i] + (i < dk_n - 1 ? (dk_sepb[i] ? DK_SEP : DK_GAP) : 0);
    }
    dk_bw = total; if (dk_bw > (int)W - 10) dk_bw = (int)W - 10; dk_bx = ((int)W - dk_bw) / 2;
    x = dk_bx + DK_PAD; for (int i = 0; i < dk_n; i++) { dk_x[i] = x; x += dk_w[i] + (dk_sepb[i] ? DK_SEP : DK_GAP); }
}
static void dock_draw(u32 *b, u32 ms) {
    dock_place();
    liquid_panel(b, bgblur, dk_bx, dky, dk_bw, dkh, dock_r, 16, 70, 1);
    for (int i = 0; i < dk_n; i++) if (dk_sepb[i]) draw_rr(b, dk_x[i] + dk_w[i] + (DK_SEP - 1) / 2, dky + 14, 1, dkh - 28, 0, 0xFFFFFF, 70);
    int hov = -1;
    for (int i = 0; i < dk_n; i++) {
        int iy = dky + dkh - 9 - dk_w[i];
        mac_icon(b, dk_x[i], iy, dk_w[i], dk_kind[i]); hit_add(dk_x[i], iy, dk_w[i], dk_w[i], dk_id[i]);
        if (hot(dk_x[i], iy, dk_w[i], dk_w[i] + 9)) hov = i;
        if (dk_id[i] == PWR_OFF_ID || dk_id[i] == PWR_RST_ID) { if (pwr_arm == dk_id[i] && ms < pwr_until) draw_rr(b, dk_x[i], iy, dk_w[i], dk_w[i], dk_w[i] * 23 / 100, 0xFFFFFF, 100); }
        int al = 0;                                                       /* running indicator dot */
        if (dk_app[i] == -1) al = (!nwin || term_focus) ? 235 : 110;
        else if (dk_app[i] > 0) { int wi = win_find(dk_app[i]); if (wi >= 0) al = win_[wi].min ? 80 : ((wi == nwin - 1 && !term_focus) ? 235 : 130); }
        if (al) draw_rr(b, dk_x[i] + dk_w[i] / 2 - 2, dky + dkh - 6, 4, 4, 2, 0xFFFFFF, (u32)al);
    }
    if (hov >= 0) {                                                       /* name label above the icon */
        const char *tip = dk_tip[hov]; int tw = slen(tip) * 8 + 24, tx0 = dk_x[hov] + dk_w[hov] / 2 - tw / 2, ty0 = dky + dkh - 9 - dk_w[hov] - 34;
        if (tx0 < 6) tx0 = 6; if (tx0 + tw > (int)W - 6) tx0 = (int)W - 6 - tw;
        draw_rr(b, tx0, ty0, tw, 26, 9, 0x2B2B30, 225); draw_rr(b, tx0, ty0, tw, 26, 9, 0xFFFFFF, 14); text(b, tx0 + 12, ty0 + 5, tip, 0xF2F2F7);
    }
}
static void dock_sync(int force) {
    int nr = 0, nl[8];
    for (int k = 0; k < dock_nrun; k++) if (win_find(dock_run[k]) >= 0) nl[nr++] = dock_run[k];
    for (int i = 0; i < nwin; i++) { int a = win_[i].app, f = 0; if (a == 7) continue; for (int k = 0; k < nr; k++) if (nl[k] == a) f = 1; if (!f && nr < 8) nl[nr++] = a; }
    int sig = nr; for (int k = 0; k < nr; k++) sig = sig * 31 + nl[k];
    if (force || sig != dock_sig || nr != dock_nrun) { for (int k = 0; k < nr; k++) dock_run[k] = nl[k]; dock_nrun = nr; dock_sig = sig; dock_bake(); }
}

static void build_scene(void) {
    for (int k = 0; k < 8; k++) wck[k].ok = 0;   /* window glass caches depend on the wallpaper */
    paint_bg();
    memcpy(bgblur, bgbuf, W * H * 4); if (!flat_ui && blur_r > 0) { if (theme_i == 10) blur_rect(bgblur, (int)W, (int)H, 2); else { blur_rect(bgblur, (int)W, (int)H, blur_r); lift(bgblur, (int)(W * H), 165, 10); } }   /* frost: keep the glass clear (2px blur, no lift) */
    ww = (int)W - 80; if (ww > 1100) ww = 1100;
    wh = (int)H - 70 - 86;
    wx = ((int)W - ww) / 2; wy = 70;
    cols = (ww - 32) / 8; if (cols > COLS_MAX) cols = COLS_MAX;
    rows = (wh - 34 - 20) / 16; if (rows > ROWS_MAX) rows = ROWS_MAX;
    tx = wx + 16; ty = wy + 34 + 10;
    if (terminal_visible) {
        for (int i = 0; i < 14; i++) draw_rr(bgbuf, wx - i, wy - i + 14, ww + 2 * i, wh + 2 * i, 14 + i, 0x04101E, 5);
        { int sv = flat_ui; flat_ui = 1; glass(wx, wy, ww, wh, 14, WIN_TINT, 255, 52, 200); flat_ui = sv; titlebar_fill(bgbuf, wx, wy, ww, wh, 14, 1); }   /* Terminal is a normal app window: solid */
        for (int i = 0; i < ww - 2; i++) { u32 *p = &bgbuf[(wy + 33) * W + wx + 1 + i]; *p = mix(*p, 0xFFFFFF, 26); }
    }

    memcpy(bgclean, bgbuf, W * H * 4); bgclean_ok = 1;   /* wallpaper + panels + Terminal, no dock: the dock is re-baked on top of this when windows open/close */
    dock_sync(1);
}

/* ============================ terminal ============================ */
static char tc[ROWS_MAX][COLS_MAX]; static u8 ta_[ROWS_MAX][COLS_MAX];
static int cx_, cy_, dirty = 1, hide_cur;

static void t_clear(void) { memset(tc, ' ', sizeof tc); memset(ta_, 0, sizeof ta_); cx_ = cy_ = 0; dirty = 1; }
static void t_nl(void) {
    cx_ = 0;
    if (++cy_ >= rows) {
        cy_ = rows - 1;
        memmove(tc[0], tc[1], (unsigned)(ROWS_MAX - 1) * COLS_MAX); memmove(ta_[0], ta_[1], (unsigned)(ROWS_MAX - 1) * COLS_MAX);
        memset(tc[rows - 1], ' ', COLS_MAX); memset(ta_[rows - 1], 0, COLS_MAX);
    }
}
#define CAP_MAX 6000
static char cap_buf[CAP_MAX]; static int cap_n, cap_on;   /* pipe capture: terminal output is collected here while a command feeds a | filter */
static void t_putc(char c, u8 a) {
    dirty = 1;
    if (cap_on) { if (cap_n < CAP_MAX - 1) cap_buf[cap_n++] = c == '\t' ? ' ' : c; return; }
    if (c == '\n') { t_nl(); return; }
    if (c == '\t') { do t_putc(' ', a); while (cx_ % 4); return; }
    tc[cy_][cx_] = c; ta_[cy_][cx_] = a;
    if (++cx_ >= cols) t_nl();
}
static void t_puts(const char *s, u8 a) { while (*s) t_putc(*s++, a); }
static void t_say(const char *s, u8 a) { t_puts(s, a); t_putc('\n', a); }
static void t_pad2(int n) { t_putc('0' + (n / 10) % 10, A_TXT); t_putc('0' + n % 10, A_TXT); }
static void t_num(u32 n, int pad) {
    char b[12]; int i = 11; b[i] = 0; if (!n) b[--i] = '0';
    while (n) { b[--i] = '0' + n % 10; n /= 10; }
    int len = 11 - i; for (int k = len; k < pad; k++) t_putc(' ', A_TXT);
    t_puts(b + i, A_TXT);
}

/* ============================ dynamic island ============================ */
static int timer_on; static u32 timer_end;
static int iw = 112 << 8, ih = 34 << 8, vw, vh, tw = 112 << 8, th = 34 << 8;
static char isl_msg[64]; static int isl_kind, isl_expand; static u32 isl_until, cur_ms;
/* Smooth hover: every hovered control eases its highlight in and out (about 90 ms) instead of snapping. Keyed by rectangle. */
static struct { int key; int a; u32 t; } hv_[64];
static int hov(int x, int y, int w, int h) {
    int key = (int)(((u32)x * 73856093u) ^ ((u32)y * 19349663u) ^ ((u32)w * 83492791u) ^ ((u32)h * 2654435761u) | 1u), f = 0, old = 0;
    for (int i = 0; i < 64; i++) { if (hv_[i].key == key) { f = i + 1; break; } if (hv_[i].t < hv_[old].t) old = i; }
    int i = f ? f - 1 : old; if (!f) { hv_[i].key = key; hv_[i].a = 0; hv_[i].t = cur_ms; }
    int target = hot(x, y, w, h) ? 255 : 0, dt = (int)(cur_ms - hv_[i].t); if (dt > 60) dt = 60; if (dt < 0) dt = 0;
    if (dt) { int st = (target - hv_[i].a) * dt / 70; if (!st && hv_[i].a != target) st = target > hv_[i].a ? 1 : -1; hv_[i].a += st; hv_[i].t = cur_ms; }
    if (hv_[i].a != target) anim_busy = 1;
    return hv_[i].a;
}
static inline int hmix(int lo, int hi, int a) { return lo + (hi - lo) * a / 255; }
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void idle_size(void) { tw = (timer_on ? 150 : 112) << 8; th = 34 << 8; }
static void notify(const char *m, int kind) {
    int i = 0; for (; m[i] && i < 56; i++) isl_msg[i] = m[i]; isl_msg[i] = 0;
    isl_kind = kind; isl_expand = 1; isl_until = cur_ms + 2400;
    int w = 52 + slen(isl_msg) * 8 + 24; if (w < 200) w = 200; if (w > (int)W - 40) w = (int)W - 40;
    tw = w << 8; th = 62 << 8; dirty = 1;
}
static int isl_moving(void) {
    int a = tw - iw, b = th - ih;
    return (a > 256 || a < -256 || b > 256 || b < -256 || vw > 64 || vw < -64 || vh > 64 || vh < -64);
}
static void isl_step(void) {                       /* damped spring: slight bounce, like the real thing */
    vw = ((vw + (tw - iw) / 8) * 7) / 10; iw += vw;
    vh = ((vh + (th - ih) / 8) * 7) / 10; ih += vh;
    if (!isl_moving()) { iw = tw; ih = th; vw = vh = 0; }
}
static int ov_isl_rect(int *x, int *y, int *w, int *h) {   /* where the notification card currently is (with its shadow), if shown */
    if (!isl_expand && !timer_on) return 0;
    int iw_ = iw >> 8, ih_ = ih >> 8; *w = iw_ + 12; *h = ih_ + 14; *x = (int)W - iw_ - 22 - 6; *y = (int)H - ih_ - 86 - 4; return 1;
}
static void draw_island(u32 *buf, u32 ms) {
    if (!isl_expand && !timer_on) return; /* no empty capsule on an idle desktop */
    int w = iw >> 8, h = ih >> 8, x = (int)W - w - 22, y = (int)H - h - 86;
    int r = h / 2; if (r > 28) r = 28;
    draw_rr(buf, x - 3, y + 4, w + 6, h + 6, r + 3, 0x000000, 48);          /* soft card shadow */
    draw_rr(buf, x, y, w, h, r, 0x4AA7D8, 150);                             /* PixelBeeOS notification accent */
    draw_rr(buf, x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, 0x102B49, 218);
    if (isl_expand && w >= (tw >> 8) - 30) {
        static const u32 kc[3] = {0x6FE3FF, 0x8FFFC4, 0xFF8FA3}; static const char kg[3] = {'i', '+', '!'};
        draw_rr(buf, x + 14, y + h / 2 - 14, 28, 28, 14, kc[isl_kind], 255);
        glyph(buf, x + 14 + 10, y + h / 2 - 8, kg[isl_kind], 0x04101E);
        text(buf, x + 52, y + h / 2 - 8, isl_msg, 0xF2FAFF);
    } else if (!isl_expand && w >= 100) {
        u32 pulse = ((ms / 700) & 1) ? 255 : 170;
        if (timer_on) {                                    /* live activity: countdown */
            u32 sec = ((timer_end > ms ? timer_end - ms : 0) + 999) / 1000; char b[16] = "Timer 00:00";
            b[6] = '0' + (sec / 60 / 10) % 10; b[7] = '0' + (sec / 60) % 10; b[9] = '0' + (sec % 60) / 10; b[10] = '0' + sec % 10;
            draw_rr(buf, x + 18, y + h / 2 - 4, 8, 8, 4, 0xFFB454, pulse);
            text(buf, x + 36, y + h / 2 - 8, b, 0xFFF0DC);
        } else {
            draw_rr(buf, x + 18, y + h / 2 - 4, 8, 8, 4, 0x6FE3FF, pulse);
            text(buf, x + 36, y + h / 2 - 8, "PixelBeeOS", 0xDCEEFA);
        }
    }
}

/* ============================ RAM file system ============================ */
#define NODES 96
#define FSIZE 2048
typedef struct { char name[24]; int parent; u8 used, isdir; int size; } Node;
static Node nd[NODES]; static char fdata[NODES][FSIZE]; static int cwd;
/* Standard user folders on C: (the current filesystem is RAM-backed, not a physical disk). */
static int home_node, desktop_node, documents_node, downloads_node, pictures_node, music_node, videos_node, projects_node;

static char lw(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int ci_eq(const char *a, const char *b) { while (*a && *b) { if (lw(*a) != lw(*b)) return 0; a++; b++; } return *a == *b; }
static void scpy(char *d, const char *s, int max) { int i = 0; for (; s[i] && i < max - 1; i++) d[i] = s[i]; d[i] = 0; }
static int fs_find(int dir, const char *n) {
    for (int i = 1; i < NODES; i++) if (nd[i].used && nd[i].parent == dir && ci_eq(nd[i].name, n)) return i;
    return -1;
}
static int valid_name(const char *n) {
    int l = slen(n); if (l < 1 || l > 23) return 0;
    for (int i = 0; i < l; i++) { char c = n[i]; if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0; }
    return 1;
}
static int fs_new(int dir, const char *n, int isdir) {
    for (int i = 1; i < NODES; i++) if (!nd[i].used) {
        nd[i].used = 1; nd[i].isdir = (u8)isdir; nd[i].parent = dir; nd[i].size = 0; scpy(nd[i].name, n, 24); return i;
    }
    return -1;
}
static int resolve(const char *p) {
    int cur = cwd;
    if (p[0] && p[1] == ':') p += 2;
    if (*p == '\\' || *p == '/') { cur = 0; p++; }
    char seg[24];
    while (*p) {
        int n = 0; while (*p && *p != '\\' && *p != '/') { if (n < 23) seg[n++] = *p; p++; }
        seg[n] = 0; if (*p) p++;
        if (n == 0 || (n == 1 && seg[0] == '.')) continue;
        if (n == 2 && seg[0] == '.' && seg[1] == '.') { cur = nd[cur].parent < 0 ? 0 : nd[cur].parent; continue; }
        int idx = fs_find(cur, seg);
        if (idx < 0 || (!nd[idx].isdir && *p)) return -1;
        cur = idx;
    }
    return cur;
}
static int resolve_parent(const char *p, int *parent, char *leaf) {   /* split "dir\leaf" */
    char dir[96]; int l = slen(p), k = l;
    while (k > 0 && p[k - 1] != '\\' && p[k - 1] != '/') k--;
    scpy(leaf, p + k, 24);
    if (k == 0) { *parent = cwd; return 1; }
    int i = 0; for (; i < k && i < 95; i++) dir[i] = p[i]; dir[i] = 0;
    int d = resolve(dir); if (d < 0 || !nd[d].isdir) return 0;
    *parent = d; return 1;
}
static void node_path(int node, char *out) {
    int chain[8], n = 0, c = node;
    while (c > 0 && n < 8) { chain[n++] = c; c = nd[c].parent; }
    int o = 0; out[o++] = 'C'; out[o++] = ':'; out[o++] = '\\';
    for (int i = n - 1; i >= 0; i--) { for (const char *s = nd[chain[i]].name; *s; s++) out[o++] = *s; if (i) out[o++] = '\\'; }
    out[o] = 0;
}
static void path_str(char *out) { node_path(cwd, out); }
static void add_txt(char *leaf) {                      /* "notes" -> "notes.txt" */
    int l = slen(leaf), dot = 0; for (int i = 0; i < l; i++) if (leaf[i] == '.') dot = 1;
    if (!dot && l <= 19) { leaf[l] = '.'; leaf[l + 1] = 't'; leaf[l + 2] = 'x'; leaf[l + 3] = 't'; leaf[l + 4] = 0; }
}

/* ============================ text editor (Esc = save) ============================ */
static int ed_on, ed_node, ed_len; static char edbuf[FSIZE];
static void ed_draw(void) {
    t_clear();
    t_puts(" Editing ", A_DIM); t_puts(nd[ed_node].name, A_ACC); t_say("   -   type freely, Esc saves and closes", A_DIM);
    t_say(" ------------------------------------------------", A_DIM);
    for (int i = 0; i < ed_len; i++) t_putc(edbuf[i], A_TXT);
}
static void ed_begin(int idx) {
    ed_on = 1; ed_node = idx; ed_len = nd[idx].size; memcpy(edbuf, fdata[idx], (unsigned)ed_len); ed_draw();
}

/* ============================ shell ============================ */
static char line[256]; static int ll;
static char hist[8][256]; static int hn, hpos;
static int ntok; static char tk[16][64], tl[16][64]; static const char *trest[16];

static void prompt(void) {
    if (prompt_mode == 1) t_puts("> ", A_ACC);
    else if (prompt_mode == 2) { t_puts(prompt_txt, A_ACC); t_puts("> ", A_ACC); }
    else { char p[200]; path_str(p); t_puts(p, A_ACC); t_puts("> ", A_ACC); }
    ll = 0;
}
static void tokenize(const char *s) {
    ntok = 0;
    while (*s && ntok < 16) {
        while (*s == ' ') s++;
        if (!*s) break;
        trest[ntok] = s; int n = 0;
        while (*s && *s != ' ') { if (n < 63) { tk[ntok][n] = *s; tl[ntok][n] = lw(*s); n++; } s++; }
        tk[ntok][n] = tl[ntok][n] = 0; ntok++;
    }
}
static int is_(const char *w, const char *a, const char *b, const char *c, const char *d) {
    return ci_eq(w, a) || (b && ci_eq(w, b)) || (c && ci_eq(w, c)) || (d && ci_eq(w, d));
}
static void err(const char *m) { t_say(m, A_ERR); notify(m, 2); }

static void cmd_help(int page) {
    if (page == 1) {
        t_say(" TOOLS & FUN", A_ACC);
        t_say(" calc 2+3*(4-1)            integer calculator", A_TXT);
        t_say(" date                      date and time", A_TXT);
        t_say(" timer 90 | 5m | stop      countdown shown in the island", A_TXT);
        t_say(" theme prism|glacier|night|aurora|sunset|nebula", A_TXT);
        t_say(" snake                     game  (arrows or WASD, Esc quits)", A_TXT);
        t_say(" beep                      PC speaker alert check", A_TXT);
        t_say(" sysinfo | uptime | memory | history | about", A_TXT);
        t_say(" island TEXT               show text in the island", A_TXT);
        t_say(" restart | shutdown", A_TXT);
        return;
    }
    t_say(" FILES & FOLDERS      easy word / classic", A_ACC);
    t_say(" list                       dir", A_TXT);
    t_say(" go to NAME | go back       cd NAME | cd ..", A_TXT);
    t_say(" make folder NAME           md NAME", A_TXT);
    t_say(" make file NAME             edit NAME     (Esc saves)", A_TXT);
    t_say(" show NAME                  type NAME", A_TXT);
    t_say(" write NAME text            append NAME text", A_TXT);
    t_say(" echo text > NAME           (>> adds to the end)", A_TXT);
    t_say(" copy A B | move A B        rename A B", A_TXT);
    t_say(" delete NAME                del NAME", A_TXT);
    t_say(" tree | find TEXT           clear (cls)", A_TXT);
    t_say(" help tools  ->  calculator, timer, themes, snake and more", A_DIM);
}

/* ---- integer calculator ---- */
static const char *cp; static int cerr;
static int c_expr(void);
static void c_ws(void) { while (*cp == ' ') cp++; }
static int c_fac(void) {
    c_ws();
    if (*cp == '-') { cp++; return -c_fac(); }
    if (*cp == '(') { cp++; int v = c_expr(); c_ws(); if (*cp == ')') cp++; else cerr = 1; return v; }
    if (*cp >= '0' && *cp <= '9') { int v = 0; while (*cp >= '0' && *cp <= '9') { v = v * 10 + (*cp - '0'); cp++; } return v; }
    cerr = 1; return 0;
}
static int c_term(void) {
    int v = c_fac();
    for (;;) {
        c_ws(); char o = *cp;
        if (o == '*') { cp++; v *= c_fac(); }
        else if (o == '/' || o == '%') {
            cp++; int r = c_fac();
            if (!r) { cerr = 2; return 0; }
            if (r == -1) v = (o == '/') ? -v : 0; else v = (o == '/') ? v / r : v % r;
        } else return v;
    }
}
static int c_expr(void) {
    int v = c_term();
    for (;;) { c_ws(); char o = *cp; if (o == '+') { cp++; v += c_term(); } else if (o == '-') { cp++; v -= c_term(); } else return v; }
}
static void t_snum(int v) { if (v < 0) { t_putc('-', A_OK); t_num(0u - (u32)v, 0); } else t_num((u32)v, 0); }

static int ci_has(const char *hay, const char *nee) {
    for (; *hay; hay++) { const char *a = hay, *b = nee; while (*a && *b && lw(*a) == lw(*b)) { a++; b++; } if (!*b) return 1; }
    return 0;
}
static void tree_print(int dir, int depth) {
    for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == dir) {
        for (int j = 0; j < depth; j++) t_puts("  ", A_DIM);
        t_puts(nd[k].isdir ? "+ " : "- ", A_DIM); t_say(nd[k].name, nd[k].isdir ? A_ACC : A_TXT);
        if (nd[k].isdir && depth < 8) tree_print(k, depth + 1);
    }
}
static int parse_dur(const char *p) {
    if (*p < '0' || *p > '9') return -1;
    int v = 0; while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; if (v > 100000) return -1; }
    if (*p == 'm' || *p == 'M') v *= 60; else if (*p == 'h' || *p == 'H') v *= 3600; else if (*p && *p != 's' && *p != 'S') return -1;
    return v;
}

/* ---- snake ---- */
static int game_on, gover, gw, gh, sl, sdx, sdy, fx, fy, score, spk_active;
static int sx[400], sy[400]; static u32 game_last, rng = 12345, spk_until;
static u32 rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7fff; }
static void gput(int x, int y, char ch, u8 a) { if (x >= 0 && y >= 0 && x < cols && y < rows) { tc[y][x] = ch; ta_[y][x] = a; } }
static void gstr(int x, int y, const char *t, u8 a) { for (; *t; t++, x++) gput(x, y, *t, a); }
static int game_ms(void) { int v = 130 - score * 3; return v < 60 ? 60 : v; }
static void game_draw(void) {
    t_clear(); hide_cur = 1;
    char hd[64] = " SNAKE   score 000   arrows/WASD move, Esc quits";
    hd[15] = '0' + (score / 100) % 10; hd[16] = '0' + (score / 10) % 10; hd[17] = '0' + score % 10;
    gstr(0, 0, hd, A_ACC);
    for (int x = 0; x <= gw + 1; x++) { gput(x, 1, (x == 0 || x == gw + 1) ? '+' : '-', A_DIM); gput(x, gh + 2, (x == 0 || x == gw + 1) ? '+' : '-', A_DIM); }
    for (int y = 0; y < gh; y++) { gput(0, y + 2, '|', A_DIM); gput(gw + 1, y + 2, '|', A_DIM); }
    gput(fx + 1, fy + 2, '*', A_ERR);
    for (int i = sl - 1; i >= 0; i--) gput(sx[i] + 1, sy[i] + 2, i ? 'o' : 'O', i ? A_OK : A_ACC);
    if (gover) gstr(gw / 2 - 12, gh / 2 + 2, "  GAME OVER - press any key  ", A_WARN);
}
static void game_food(void) {
    for (int tries = 0; tries < 500; tries++) {
        fx = (int)(rnd() % (u32)gw); fy = (int)(rnd() % (u32)gh); int ok = 1;
        for (int i = 0; i < sl; i++) if (sx[i] == fx && sy[i] == fy) ok = 0;
        if (ok) return;
    }
}
static void game_start(void) {
    gw = cols - 2; if (gw > 60) gw = 60; gh = rows - 4; if (gh > 22) gh = 22;
    sl = 4; for (int i = 0; i < sl; i++) { sx[i] = gw / 2 - i; sy[i] = gh / 2; }
    sdx = 1; sdy = 0; score = 0; gover = 0; game_on = 1; game_last = cur_ms; rng ^= cur_ms; game_food(); game_draw();
}
static void game_step(void) {
    int nx = sx[0] + sdx, ny = sy[0] + sdy;
    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) { gover = 1; }
    for (int i = 0; i < sl - 1 && !gover; i++) if (sx[i] == nx && sy[i] == ny) gover = 1;
    if (gover) { notify("Game over", 2); speaker_on(220); spk_active = 1; spk_until = cur_ms + 350; return; }
    int tx_ = sx[sl - 1], ty_ = sy[sl - 1];
    for (int i = sl - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = nx; sy[0] = ny;
    if (nx == fx && ny == fy) {
        if (sl < 399) { sx[sl] = tx_; sy[sl] = ty_; sl++; }
        score++; game_food();
        if (score % 5 == 0) notify("Nice! Keep going", 1);
    }
}
static void game_exit(void) { game_on = 0; hide_cur = 0; t_clear(); prompt(); }
static void game_key(int k) {
    if (gover) { game_exit(); return; }
    if (k == 27) { game_exit(); return; }
    if ((k == KEY_UP || k == 'w' || k == 'W') && sdy != 1) { sdx = 0; sdy = -1; }
    else if ((k == KEY_DOWN || k == 's' || k == 'S') && sdy != -1) { sdx = 0; sdy = 1; }
    else if ((k == KEY_LEFT || k == 'a' || k == 'A') && sdx != 1) { sdx = -1; sdy = 0; }
    else if ((k == KEY_RIGHT || k == 'd' || k == 'D') && sdx != -1) { sdx = 1; sdy = 0; }
}

#include "cmds.h"
#include "apps.h"
#include "ux.h"
#include "fat32.h"
#include "ata.h"
#include "disk.h"
#define NET_HAVE_TLS 1
#include "crypto.h"
#include "tls.h"
#include "net.h"
#include "nettls.h"
#include "netdrv.h"
#include "netcmd.h"
#include "netapp.h"
#include "mines.h"

/* Hardware facts are detected independently from drivers. This does not claim
 * that battery or codec control is implemented merely because a device exists. */
#include "acpi.h"
static int acpi_sum_ok(const volatile u8 *p, int n) {
    u8 sum = 0; for (int i = 0; i < n; i++) sum = (u8)(sum + p[i]); return sum == 0;
}
static void hardware_probe(void) {
    acpi_present = 0; audio_device_present = 0;
    pci_device_count = pci_net_present = pci_storage_present = pci_usb_present = 0;
#ifdef HOSTTEST
    return;
#else
    /* ACPI: find the RSDP (EBDA + BIOS area) and read the shutdown / reboot data (acpi.h). */
    { u32 rsdp = acpi_find_rsdp(); acpi_present = rsdp != 0; acpi_init_tables(rsdp); }
    /* Enumerate PCI functions. Detection is not the same as having a working driver. */
    for (int bus = 0; bus < 8; bus++)
        for (int dev = 0; dev < 32; dev++)
            for (int fn = 0; fn < 8; fn++) {
                u32 id = pci_rd(bus, dev, fn, 0);
                if ((id & 0xFFFFu) == 0xFFFFu) { if (!fn) break; continue; }
                pci_device_count++;
                u32 cls = pci_rd(bus, dev, fn, 8);
                int cl = (int)(cls >> 24), sub = (int)((cls >> 16) & 255);
                if (cl == 4) audio_device_present = 1;
                if (cl == 2) pci_net_present = 1;
                if (cl == 1) pci_storage_present = 1;
                if (cl == 0x0C && sub == 3) pci_usb_present = 1;
                if (!fn && !(pci_rd(bus, dev, 0, 0x0C) & 0x800000u)) break;
            }
#endif
}

static void cmd_list(void) {
    char p[200]; path_str(p); t_puts(" Directory of ", A_DIM); t_say(p, A_ACC);
    int nf = 0, nd_ = 0;
    for (int i = 1; i < NODES; i++) if (nd[i].used && nd[i].parent == cwd) {
        if (nd[i].isdir) { t_puts("  <DIR>      ", A_ACC); t_say(nd[i].name, A_ACC); nd_++; }
        else { t_puts("  ", A_TXT); t_num((u32)nd[i].size, 6); t_puts(" B     ", A_TXT); t_say(nd[i].name, A_TXT); nf++; }
    }
    if (!nf && !nd_) t_say("  (empty)", A_DIM);
    t_puts("  ", A_DIM); t_num((u32)nf, 0); t_puts(" file(s), ", A_DIM); t_num((u32)nd_, 0); t_say(" folder(s)", A_DIM);
}

/* ============================ shell power: pipes, scripts, if, alias ============================ */
static void exec_line(const char *s);
static int sh_depth;
static char al_n[8][16], al_t[8][80]; static int al_c;
static int alias_find(const char *n) { for (int i = 0; i < al_c; i++) if (ci_eq(al_n[i], n)) return i; return -1; }
static void join_tokens(char *out, int max, int from) {
    int o = 0;
    for (int i = from; i < ntok; i++) { if (i > from && o < max - 1) out[o++] = ' '; for (const char *p = tk[i]; *p && o < max - 1; p++) out[o++] = *p; }
    out[o] = 0;
}
static void cmd_alias(int remove) {
    if (remove) {
        int i = ntok > 1 ? alias_find(tk[1]) : -1; if (i < 0) { err("No such alias"); return; }
        for (int j = i; j < al_c - 1; j++) { scpy(al_n[j], al_n[j + 1], 16); scpy(al_t[j], al_t[j + 1], 80); } al_c--; t_say("Alias removed", A_OK); return;
    }
    if (ntok < 2) {
        if (!al_c) t_say(" No aliases yet.  Try:  alias ll = list", A_DIM);
        for (int i = 0; i < al_c; i++) { t_puts(" ", A_DIM); t_puts(al_n[i], A_ACC); t_puts("  =  ", A_DIM); t_say(al_t[i], A_TXT); }
        return;
    }
    int from = (ntok > 2 && tk[2][0] == '=' && !tk[2][1]) ? 3 : 2;
    if (from >= ntok) { t_say("Usage: alias NAME = command", A_WARN); return; }
    for (const char *p = tk[1]; *p; p++) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) { err("Alias names use letters, digits and _"); return; }
    char name[16], text[80]; scpy(name, tk[1], 16); join_tokens(text, 80, from);
    int i = alias_find(name); if (i < 0) { if (al_c >= 8) { err("Alias list is full (8)"); return; } i = al_c++; }
    scpy(al_n[i], name, 16); scpy(al_t[i], text, 80); t_puts("Alias set: ", A_OK); t_puts(name, A_ACC); t_puts(" = ", A_DIM); t_say(text, A_TXT);
}
static void cmd_if(void) {
    int i = 1, neg = 0, ok = 0;
    if (i < ntok && ci_eq(tl[i], "not")) { neg = 1; i++; }
    if (i + 1 < ntok && ci_eq(tl[i], "exists")) { ok = resolve(tk[i + 1]) >= 0; i += 2; }
    else if (i + 2 < ntok && (ci_eq(tk[i + 1], "==") || ci_eq(tk[i + 1], "!="))) { ok = ci_eq(tk[i], tk[i + 2]); if (tk[i + 1][0] == '!') ok = !ok; i += 3; }
    else { t_say("Usage: if A == B command   |   if exists FILE command   |   if not exists FILE command", A_WARN); return; }
    if (neg) ok = !ok;
    if (!ok || i >= ntok) return;
    char cmd[200]; join_tokens(cmd, 200, i);
    if (sh_depth > 6) { err("Too many nested commands"); return; }
    sh_depth++; exec_line(cmd); sh_depth--;
}
static void cmd_for(void) {                      /* for x in a b c do command   |   for n in 1..5 do command */
    int d = -1; for (int i = 3; i < ntok; i++) if (ci_eq(tl[i], "do")) { d = i; break; }
    if (ntok < 6 || !ci_eq(tl[2], "in") || d < 4 || d + 1 >= ntok) { t_say("Usage: for x in a b c do command   |   for n in 1..5 do command   (use %x% in the command)", A_WARN); return; }
    if (sh_depth > 3) { err("Loops are nested too deeply"); return; }
    char var[16], cmd[200], vals[24][24]; int nv = 0; scpy(var, tk[1], 16); join_tokens(cmd, 200, d + 1);
    for (int i = 3; i < d && nv < 24; i++) {
        const char *t = tk[i]; int a = 0, b = 0, p = 0;
        while (t[p] >= '0' && t[p] <= '9') a = a * 10 + (t[p++] - '0');
        if (p && t[p] == '.' && t[p + 1] == '.' && t[p + 2] >= '0' && t[p + 2] <= '9') { p += 2; while (t[p] >= '0' && t[p] <= '9') b = b * 10 + (t[p++] - '0'); for (int v = a; v <= b && nv < 24; v++) fmt_base((u32)v, 10, vals[nv++]); }
        else scpy(vals[nv++], t, 24);
    }
    sh_depth++;
    for (int i = 0; i < nv; i++) { int j = var_find(var); if (j < 0) for (int k = 0; k < 16; k++) if (!vname[k][0]) { j = k; break; } if (j < 0) { err("Variable table is full"); break; } scpy(vname[j], var, 16); scpy(vval[j], vals[i], 48); exec_line(cmd); }
    { int j = var_find(var); if (j >= 0) vname[j][0] = 0; }
    sh_depth--;
}
static void cmd_run(const char *name) {
    if (!*name) { t_say("Usage: run FILE   (one command per line; lines starting with # are comments; exit stops)", A_WARN); return; }
    int idx = resolve(name); if (idx < 0 || nd[idx].isdir) { err("Script not found"); return; }
    if (sh_depth > 3) { err("Scripts are nested too deeply"); return; }
    sh_depth++;
    int size = nd[idx].size, p = 0, lines = 0;
    while (p < size && lines < 200) {
        char line[200]; int n = 0;
        while (p < size && fdata[idx][p] != '\n') { char c = fdata[idx][p++]; if (c != '\r' && n < 199) line[n++] = c; }
        p++; line[n] = 0;
        char *l = line; while (*l == ' ' || *l == '\t') l++;
        if (!*l || *l == '#' || ((l[0] == 'r' || l[0] == 'R') && (l[1] == 'e' || l[1] == 'E') && (l[2] == 'm' || l[2] == 'M') && l[3] == ' ')) continue;
        if (ci_eq(l, "exit")) break;
        lines++; t_puts("+ ", A_DIM); t_say(l, A_DIM); exec_line(l);
    }
    sh_depth--;
}
static int ci_cmp(const char *a, const char *b) { while (*a && *b && lw(*a) == lw(*b)) { a++; b++; } return (int)lw(*a) - (int)lw(*b); }
static int pipe_try(void) {                      /* "cmd | grep x | sort | head 5 > out.txt": run the first command, filter its text, print it or save it */
    char rname[40]; int redir = 0, rapp = 0;
    { const char *f = xl; while (*f == ' ') f++;
      int is_echo = (f[0] | 32) == 'e' && (f[1] | 32) == 'c' && (f[2] | 32) == 'h' && (f[3] | 32) == 'o' && (f[4] == ' ' || !f[4]);   /* echo keeps its own "> file" */
      if (!is_echo) for (char *p = xl; *p; p++) if (p[0] == ' ' && p[1] == '>' && (p[2] == ' ' || (p[2] == '>' && p[3] == ' '))) {
          char *q = p + (p[2] == '>' ? 4 : 3); rapp = p[2] == '>'; while (*q == ' ') q++;
          int l = 0; while (*q && *q != ' ' && l < 39) rname[l++] = *q++; rname[l] = 0;
          if (l) { redir = 1; *p = 0; }
          break; } }
    int n = 0; for (const char *p = xl; *p; p++) if (*p == '|') n++;
    if (!n && !redir) return 0;
    static char seg[6][120]; int ns = 0, o = 0;
    for (const char *p = xl; ; p++) { if (*p == '|' || !*p) { seg[ns][o] = 0; ns++; o = 0; if (!*p || ns >= 6) break; } else if (o < 119) seg[ns][o++] = *p; }
    for (int k = 0; k < ns; k++) { char *a = seg[k]; while (*a == ' ') a++; int l = slen(a); while (l > 0 && a[l - 1] == ' ') a[--l] = 0; if (a != seg[k]) { int j = 0; while ((seg[k][j] = a[j])) j++; } }
    if (!seg[0][0]) { err("Nothing before the |"); return 1; }
    cap_n = 0; cap_on = 1; exec_line(seg[0]); cap_on = 0; cap_buf[cap_n] = 0;
    static char *ln[300]; static char wcb[48]; int nl = 0; char *p = cap_buf;
    while (*p && nl < 300) { ln[nl++] = p; while (*p && *p != '\n') p++; if (*p) *p++ = 0; }
    for (int k = 1; k < ns; k++) {
        char *w = seg[k], *arg = w; while (*arg && *arg != ' ') arg++; if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }
        int cnt = 10; if (*arg >= '0' && *arg <= '9') { cnt = 0; for (const char *q = arg; *q >= '0' && *q <= '9'; q++) cnt = cnt * 10 + (*q - '0'); }
        if (ci_eq(w, "grep")) {
            int inv = 0; if (arg[0] == '-' && arg[1] == 'v' && arg[2] == ' ') { inv = 1; arg += 3; while (*arg == ' ') arg++; }
            int m = 0; for (int i = 0; i < nl; i++) if (ci_has(ln[i], arg) != inv) ln[m++] = ln[i]; nl = m;
        } else if (ci_eq(w, "head")) { if (cnt < nl) nl = cnt; }
        else if (ci_eq(w, "tail")) { if (cnt < nl) { for (int i = 0; i < cnt; i++) ln[i] = ln[nl - cnt + i]; nl = cnt; } }
        else if (ci_eq(w, "sort")) { for (int i = 1; i < nl; i++) { char *t = ln[i]; int j = i - 1; while (j >= 0 && ci_cmp(ln[j], t) > 0) { ln[j + 1] = ln[j]; j--; } ln[j + 1] = t; } }
        else if (ci_eq(w, "uniq")) { int m = 0; for (int i = 0; i < nl; i++) if (!m || !ci_eq(ln[m - 1], ln[i])) ln[m++] = ln[i]; nl = m; }
        else if (ci_eq(w, "wc") || ci_eq(w, "count")) { int words = 0, bytes = 0; for (int i = 0; i < nl; i++) { int in = 0; for (const char *q = ln[i]; *q; q++) { bytes++; if (*q == ' ') in = 0; else if (!in) { in = 1; words++; } } bytes++; }
            char a[12], b2[12], c2[12]; fmt_base((u32)nl, 10, a); fmt_base((u32)words, 10, b2); fmt_base((u32)bytes, 10, c2);
            int q = 0; for (const char *s2 = a; *s2; s2++) wcb[q++] = *s2; for (const char *s2 = " lines  "; *s2; s2++) wcb[q++] = *s2; for (const char *s2 = b2; *s2; s2++) wcb[q++] = *s2; for (const char *s2 = " words  "; *s2; s2++) wcb[q++] = *s2; for (const char *s2 = c2; *s2; s2++) wcb[q++] = *s2; for (const char *s2 = " bytes"; *s2; s2++) wcb[q++] = *s2; wcb[q] = 0;
            ln[0] = wcb; nl = 1; }
        else { err("Pipe filters: grep TEXT, grep -v TEXT, head N, tail N, sort, uniq, wc"); return 1; }
    }
    if (redir) {
        int par; char leaf[24]; if (!resolve_parent(rname, &par, leaf) || !valid_name(leaf)) { err("Bad file name"); return 1; }
        add_txt(leaf); int idx = fs_find(par, leaf);
        if (idx >= 0 && nd[idx].isdir) { err("That is a folder"); return 1; }
        if (idx < 0) { idx = fs_new(par, leaf, 0); if (idx < 0) { err("Disk full"); return 1; } }
        int pos = rapp ? nd[idx].size : 0;
        for (int i = 0; i < nl; i++) { for (const char *q = ln[i]; *q && pos < FSIZE - 2; q++) fdata[idx][pos++] = *q; if (pos < FSIZE - 1) fdata[idx][pos++] = '\n'; }
        nd[idx].size = pos; t_puts(rapp ? "Added to " : "Saved to ", A_OK); t_say(leaf, A_TXT); return 1;
    }
    for (int i = 0; i < nl; i++) t_say(ln[i], A_TXT);
    return 1;
}

static void exec_line(const char *s) {
    expand_vars(s); if (pipe_try()) return; tokenize(xl); if (!ntok) return;
    { const Cmd *cm = cmd_lookup(tl[0]);
      if (cm && cm->cat >= C_M1) { cur_desc = cm->desc; cur_name = cm->name; if (!run_cmd(cm->cat, tl[0])) t_say("(not implemented)", A_ERR); return; } }
    if (ci_eq(tl[0], "disk")) { cmd_disk(); return; }
    if (net_cmd_dispatch()) return;
    if (ci_eq(tl[0], "run") || ci_eq(tl[0], "source") || ci_eq(tl[0], "sh")) { cmd_run(ntok > 1 ? tk[1] : ""); return; }
    if (ci_eq(tl[0], "if")) { cmd_if(); return; }
    if (ci_eq(tl[0], "for")) { cmd_for(); return; }
    if (ci_eq(tl[0], "alias")) { cmd_alias(0); return; }
    if (ci_eq(tl[0], "unalias")) { cmd_alias(1); return; }
    if (ci_eq(tl[0], "pipes")) { t_say(" cmd | grep TEXT | sort | uniq | head 5 | tail 3 | wc      (grep -v TEXT hides matches)", A_TXT); t_say(" Scripts: run FILE   (# comments, if, alias, exit)   Variables: set NAME VALUE, then %NAME%", A_DIM); return; }
    { int ai = alias_find(tl[0]); if (ai >= 0) { if (sh_depth > 6) { err("Alias loop"); return; } char cmd[200]; int o = 0; for (const char *q = al_t[ai]; *q && o < 190; q++) cmd[o++] = *q; if (ntok > 1) { cmd[o++] = ' '; for (const char *q = trest[1]; *q && o < 199; q++) cmd[o++] = *q; } cmd[o] = 0; sh_depth++; exec_line(cmd); sh_depth--; return; } }
    const char *c = tl[0]; int i = 1, kf = 0, kd = 0;
    for (int k = 0; k < 2 && i < ntok; k++) {              /* skip filler words: "make FOLDER x", "go TO x" */
        const char *w = tl[i];
        if (is_(w, "file", "files", "text", "note")) { kf = 1; i++; }
        else if (is_(w, "folder", "folders", "directory", "dir")) { kd = 1; i++; }
        else if (is_(w, "to", "the", "a", "into")) i++;
        else break;
    }
    const char *arg = i < ntok ? tk[i] : "";
    int has = i < ntok;

    if (is_(c, "help", "?", "commands", 0)) cmd_help(ci_eq(arg, "tools") || ci_eq(arg, "apps") || ci_eq(arg, "more") || ci_eq(arg, "2"));
    else if (is_(c, "dir", "ls", "list", 0) || (is_(c, "show", "see", 0, 0) && (kf || kd) && !has)) cmd_list();
    else if (is_(c, "cls", "clear", 0, 0)) { t_clear(); }
    else if (is_(c, "cd", "go", "goto", "chdir") || (is_(c, "open", 0, 0, 0) && has && resolve(arg) >= 0 && nd[resolve(arg)].isdir)) {
        if (!has) { char p[200]; path_str(p); t_say(p, A_ACC); return; }
        int d = ci_eq(arg, "back") ? resolve("..") : (ci_eq(arg, "root") || ci_eq(arg, "home")) ? 0 : resolve(arg);
        if (d < 0 && has) d = fs_find(home_node, arg);   /* "cd desktop" works from anywhere, like a real shell */
        if (d < 0 || !nd[d].isdir) err("Folder not found"); else cwd = d;
    }
    else if (is_(c, "md", "mkdir", 0, 0) || (is_(c, "make", "new", "create", "add") && kd)) {
        int par; char leaf[24];
        if (!has) { t_say("Name the folder, e.g. make folder docs", A_WARN); return; }
        if (!resolve_parent(arg, &par, leaf) || !valid_name(leaf)) { err("Bad folder name"); return; }
        if (fs_find(par, leaf) >= 0) { err("Already exists"); return; }
        if (fs_new(par, leaf, 1) < 0) { err("Disk full"); return; }
        t_puts("Folder created: ", A_OK); t_say(leaf, A_TXT); notify("Folder created", 1);
    }
    else if (is_(c, "edit", "notepad", "write", "append") || is_(c, "make", "new", "create", "add")) {
        int par; char leaf[24];
        if (!has) { t_say("Name the file, e.g. make file notes", A_WARN); return; }
        if (!resolve_parent(arg, &par, leaf) || !valid_name(leaf)) { err("Bad file name"); return; }
        int app = ci_eq(c, "append"), quick = ci_eq(c, "write") || app;
        add_txt(leaf);
        int idx = fs_find(par, leaf);
        if (idx >= 0 && nd[idx].isdir) { err("That is a folder"); return; }
        if (idx < 0) { idx = fs_new(par, leaf, 0); if (idx < 0) { err("Disk full"); return; } }
        if (quick) {                                          /* write NAME text... */
            const char *rest = i + 1 < ntok ? trest[i + 1] : ""; int n = app ? nd[idx].size : 0, q = 0;
            if (n > FSIZE - 2) { err("File is full"); return; }
            while (rest[q] && n < FSIZE - 2) fdata[idx][n++] = rest[q++];
            fdata[idx][n++] = '\n'; nd[idx].size = n;
            t_puts("Saved ", A_OK); t_say(leaf, A_TXT); notify("Saved file", 1);
        } else ed_begin(idx);
    }
    else if (is_(c, "type", "cat", "read", "show") || is_(c, "open", "view", 0, 0)) {
        if (!has) { t_say("Which file?  e.g. show notes.txt", A_WARN); return; }
        int idx = resolve(arg);
        if (idx < 0) { char tmp[24]; int par; if (resolve_parent(arg, &par, tmp)) { add_txt(tmp); idx = fs_find(par, tmp); } }
        if (idx <= 0) { err("File not found"); return; }
        if (nd[idx].isdir) { err("That is a folder"); return; }
        if (!nd[idx].size) t_say("(empty file)", A_DIM);
        for (int k = 0; k < nd[idx].size; k++) t_putc(fdata[idx][k], A_TXT);
        if (cx_) t_putc('\n', A_TXT);
        notify("Opened file", 0);
    }
    else if (is_(c, "del", "rm", "delete", "remove") || ci_eq(c, "erase")) {
        if (!has) { t_say("Delete what?", A_WARN); return; }
        int idx = resolve(arg);
        if (idx <= 0) { err("Not found"); return; }
        if (nd[idx].isdir) for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == idx) { err("Folder is not empty"); return; }
        nd[idx].used = 0; t_puts("Deleted ", A_OK); t_say(nd[idx].name, A_TXT); notify("Deleted", 1);
    }
    else if (is_(c, "ren", "rename", 0, 0)) {
        if (ntok < i + 2) { t_say("Usage: rename OLD NEW", A_WARN); return; }
        int idx = resolve(arg);
        if (idx <= 0) { err("Not found"); return; }
        if (!valid_name(tk[i + 1]) || fs_find(nd[idx].parent, tk[i + 1]) >= 0) { err("Bad or taken name"); return; }
        scpy(nd[idx].name, tk[i + 1], 24); t_say("Renamed", A_OK); notify("Renamed", 1);
    }
    else if (ci_eq(c, "echo")) {
        const char *p = trest[0] + 4; while (*p == ' ') p++;
        const char *gt = p; while (*gt && *gt != '>') gt++;
        if (!*gt) { t_say(p, A_TXT); return; }
        int appd = gt[1] == '>'; const char *t = gt + (appd ? 2 : 1); while (*t == ' ') t++;
        char tgt[64]; int n = 0; while (t[n] && t[n] != ' ' && n < 63) { tgt[n] = t[n]; n++; } tgt[n] = 0;
        int par; char leaf[24];
        if (!n || !resolve_parent(tgt, &par, leaf) || !valid_name(leaf)) { err("Echo into which file?"); return; }
        add_txt(leaf); int idx = fs_find(par, leaf);
        if (idx >= 0 && nd[idx].isdir) { err("That is a folder"); return; }
        if (idx < 0) { idx = fs_new(par, leaf, 0); if (idx < 0) { err("Disk full"); return; } }
        int len = (int)(gt - p); while (len > 0 && p[len - 1] == ' ') len--;
        int pos = appd ? nd[idx].size : 0;
        if (pos + len + 1 > FSIZE) { err("File is full"); return; }
        for (int q = 0; q < len; q++) fdata[idx][pos++] = p[q];
        fdata[idx][pos++] = '\n'; nd[idx].size = pos;
        t_puts("Saved ", A_OK); t_say(leaf, A_TXT); notify("Saved file", 1);
    }
    else if (is_(c, "copy", "cp", 0, 0) || ci_eq(c, "move")) {
        int mv = ci_eq(c, "move");
        if (ntok < i + 2) { t_say(mv ? "Usage: move FROM TO" : "Usage: copy FROM TO", A_WARN); return; }
        int src = resolve(arg);
        if (src <= 0 || (!mv && nd[src].isdir)) { err("Source not found"); return; }
        int dst = resolve(tk[i + 1]), par; char leaf[24];
        if (dst >= 0 && nd[dst].isdir) { par = dst; scpy(leaf, nd[src].name, 24); }
        else if (!resolve_parent(tk[i + 1], &par, leaf) || !valid_name(leaf)) { err("Bad destination"); return; }
        int e = fs_find(par, leaf);
        if (mv) {
            if (e >= 0) { err("Name already exists there"); return; }
            for (int a = par; a > 0; a = nd[a].parent) if (a == src) { err("Can't move a folder into itself"); return; }
            nd[src].parent = par; scpy(nd[src].name, leaf, 24); t_say("Moved", A_OK); notify("Moved", 1);
        } else {
            if (e == src) { err("Same file"); return; }
            if (e >= 0 && nd[e].isdir) { err("Destination is a folder"); return; }
            if (e < 0) { e = fs_new(par, leaf, 0); if (e < 0) { err("Disk full"); return; } }
            memcpy(fdata[e], fdata[src], (unsigned)nd[src].size); nd[e].size = nd[src].size;
            t_say("Copied", A_OK); notify("Copied", 1);
        }
    }
    else if (ci_eq(c, "tree")) {
        char p[200]; path_str(p); t_say(p, A_ACC); int any = 0;
        for (int k = 1; k < NODES; k++) if (nd[k].used && nd[k].parent == cwd) any = 1;
        if (!any) t_say("  (empty)", A_DIM); else tree_print(cwd, 0);
    }
    else if (ci_eq(c, "find") || ci_eq(c, "search")) {
        if (!has) { t_say("find TEXT  -  searches names everywhere", A_WARN); return; }
        int hit = 0;
        for (int k = 1; k < NODES; k++) if (nd[k].used && ci_has(nd[k].name, arg)) {
            char p[200]; node_path(k, p); t_say(p, nd[k].isdir ? A_ACC : A_TXT); hit++;
        }
        if (!hit) t_say("Nothing found", A_DIM);
    }
    else if (is_(c, "calc", "math", "calculate", 0)) {
        if (!has) { t_say("calc 2+3*(4-1)   -  whole numbers, + - * / % and ( )", A_WARN); return; }
        cp = trest[i]; cerr = 0; int v = c_expr(); c_ws(); if (*cp) cerr = 1;
        if (cerr == 2) err("Can't divide by zero");
        else if (cerr) err("That doesn't look like math");
        else { t_puts(" = ", A_DIM); t_snum(v); t_putc('\n', A_TXT); }
    }
    else if (is_(c, "date", "time", "clock", 0)) {
        int h, mi, sc, d, mo, y; rtc_read(&h, &mi, &sc, &d, &mo, &y);
        t_puts(" ", A_DIM); t_num((u32)y, 0); t_putc('-', A_DIM); t_pad2(mo); t_putc('-', A_DIM); t_pad2(d);
        t_puts("   ", A_DIM); t_pad2(h); t_putc(':', A_DIM); t_pad2(mi); t_putc(':', A_DIM); t_pad2(sc); t_putc('\n', A_TXT);
    }
    else if (ci_eq(c, "timer")) {
        if (has && is_(arg, "stop", "cancel", "off", "end")) {
            if (timer_on) { timer_on = 0; idle_size(); notify("Timer cancelled", 0); t_say("Timer cancelled", A_OK); } else t_say("No timer running", A_DIM);
        } else if (!has) {
            if (timer_on) { t_puts(" Time left: ", A_DIM); t_num((timer_end - cur_ms + 999) / 1000, 0); t_say(" sec", A_TXT); }
            else t_say("timer 90  |  timer 5m  |  timer stop", A_WARN);
        } else {
            int sec = parse_dur(arg);
            if (sec <= 0 || sec > 5999) { err(sec > 5999 ? "Max is 99 minutes" : "Try: timer 90 or timer 5m"); return; }
            timer_on = 1; timer_end = cur_ms + (u32)sec * 1000u; idle_size(); dirty = 1;
            t_puts(" Timer started: ", A_OK); t_num((u32)sec, 0); t_say(" sec  (watch the island)", A_DIM);
        }
    }
    else if (ci_eq(c, "theme") || ci_eq(c, "wallpaper")) {
        if (!has) {
            t_puts(" Themes:", A_DIM); for (int k = 0; k < THEME_COUNT; k++) { t_putc(' ', A_TXT); t_puts(themes[k].name, k == theme_i ? A_ACC : A_TXT); }
            t_putc('\n', A_TXT); return;
        }
        int f = -1; for (int k = 0; k < THEME_COUNT; k++) if (ci_eq(themes[k].name, arg)) f = k;
        if (f < 0) { err("Unknown theme"); return; }
        theme_i = f; build_scene(); dirty = 1; t_puts(" Theme: ", A_DIM); t_say(themes[f].name, A_ACC); notify("Theme applied", 1);
    }
    else if (ci_eq(c, "beep")) { if (alerts_enabled) { speaker_on(880); spk_active = 1; } spk_until = cur_ms + 250; t_say(" beep!", A_DIM); }
    else if (is_(c, "snake", "game", "play", 0)) { game_start(); }
    else if (ci_eq(c, "history")) {
        int from = hn > 8 ? hn - 8 : 0;
        if (hn == 0) t_say(" (nothing yet)", A_DIM);
        for (int k = from; k < hn; k++) { t_puts("  ", A_DIM); t_say(hist[k & 7], A_TXT); }
    }
    else if (is_(c, "sysinfo", "neofetch", "specs", 0)) {
        static const char *logo[8] = {"     /\\       ", "    /  \\  /\\  ", "   / /\\ \\/  \\ ", "  /_/  \\_\\__\\ ",
                                      "              ", "              ", "              ", "              "};
        int h, mi, sc, d, mo, y, files = 0; rtc_read(&h, &mi, &sc, &d, &mo, &y);
        for (int k = 1; k < NODES; k++) if (nd[k].used) files++;
        for (int k = 0; k < 8; k++) {
            t_puts(logo[k], A_ACC);
            switch (k) {
            case 0: t_say("PixelBeeOS 2.11", A_ACC); break;
            case 1: t_say("-----------------", A_DIM); break;
            case 2: t_puts("Screen  : ", A_DIM); t_num(W, 0); t_putc('x', A_TXT); t_num(H, 0); t_putc('\n', A_TXT); break;
            case 3: t_puts("Memory  : ", A_DIM); t_num(mem_kb / 1024, 0); t_say(" MB", A_TXT); break;
            case 4: t_puts("Uptime  : ", A_DIM); t_num(cur_ms / 60000, 0); t_say(" min", A_TXT); break;
            case 5: t_puts("Items   : ", A_DIM); t_num((u32)files, 0); t_say(" in RAM disk", A_TXT); break;
            case 6: t_puts("Theme   : ", A_DIM); t_say(themes[theme_i].name, A_TXT); break;
            default: t_puts("Time    : ", A_DIM); t_pad2(h); t_putc(':', A_TXT); t_pad2(mi); t_putc('\n', A_TXT); break;
            }
        }
    }
    else if (is_(c, "about", "ver", "info", "version")) {
        t_say(" PixelBeeOS 3.2  -  Liquid Glass Edition", A_ACC);
        t_say(" desktop icons, This PC, RAM drive, glass taskbar", A_DIM);
        notify("PixelBeeOS 2.11", 0);
    }
    else if (ci_eq(c, "uptime")) {
        u32 s = cur_ms / 1000; t_puts(" Up for ", A_DIM); t_num(s / 60, 0); t_puts(" min ", A_TXT); t_num(s % 60, 0); t_say(" sec", A_TXT);
    }
    else if (ci_eq(c, "tasks") || ci_eq(c, "tasklist")) {
        t_say("Interactive tasks (window manager):", A_ACC);
        t_say("  Shell / desktop  -  resident", A_TXT);
        for (int wi = 0; wi < nwin; wi++) {
            const App *ap = app_at(win_[wi].app - 1);
            t_puts("  ", A_DIM); t_say(ap->name, A_TXT); t_puts("  -  ", A_DIM);
            t_say(win_[wi].min ? "Minimized" : "Running", win_[wi].min ? A_WARN : A_OK);
        }
        t_say("Cooperative UI tasks only; preemptive user processes are not implemented.", A_DIM);
    }
    else if (ci_eq(c, "meminfo") || ci_eq(c, "memory")) {
        int used = 0, files = 0, dirs = 0;
        for (int fi = 1; fi < NODES; fi++) if (nd[fi].used) { if (nd[fi].isdir) dirs++; else { files++; used += nd[fi].size; } }
        t_puts("RAM reported by bootloader: ", A_DIM); t_num(mem_kb / 1024, 0); t_say(" MB", A_TXT);
        t_puts("RAM file store: ", A_DIM); t_num((u32)used, 0); t_say(" / 194560 bytes used", A_TXT);
        t_puts("Files: ", A_DIM); t_num((u32)files, 0); t_puts("  Folders: ", A_DIM); t_num((u32)dirs, 0); t_putc('\n', A_TXT);
        t_say("Paging / virtual memory allocator: not implemented in this kernel.", A_WARN);
    }
    else if (ci_eq(c, "devices") || ci_eq(c, "devmgr")) {
        t_say("Detected device classes (PCI scan):", A_ACC);
        t_puts("PCI functions: ", A_DIM); t_num((u32)pci_device_count, 0); t_putc('\n', A_TXT);
        t_say("PS/2 keyboard + mouse input: initialized", A_OK);
        t_say("VBE framebuffer + PIT clock: active", A_OK);
        t_say(pci_net_present ? "PCI network controller: detected; driver unavailable" : "PCI network controller: not detected", A_WARN);
        t_say(pci_storage_present ? "PCI storage controller: detected; driver unavailable" : "PCI storage controller: not detected", A_WARN);
        t_say(pci_usb_present ? "USB controller: detected; driver unavailable" : "USB controller: not detected", A_WARN);
        t_say(audio_device_present ? "PCI audio controller: detected; audio driver pending" : "PCI audio controller: not detected", A_WARN);
    }
    else if (ci_eq(c, "security")) {
        t_say("Security status:", A_ACC);
        t_say(pw_set ? "Screen-lock password: configured" : "Screen-lock password: not configured", pw_set ? A_OK : A_WARN);
        t_say("Per-user accounts and file ACLs: not implemented", A_WARN);
        t_say("Network firewall: unavailable (network stack/driver absent)", A_WARN);
    }
    else if (is_(c, "memory", "mem", "free", 0)) {
        int u = 0, b = 0; for (int k = 1; k < NODES; k++) if (nd[k].used) { u++; b += nd[k].size; }
        t_puts(" Items ", A_DIM); t_num((u32)u, 0); t_puts("/", A_DIM); t_num(NODES - 1, 0); t_puts("   bytes stored ", A_DIM); t_num((u32)b, 0); t_putc('\n', A_TXT);
    }
    else if (ci_eq(c, "island")) {
        if (!has) { t_say("island TEXT  -  shows TEXT in the notification card", A_WARN); return; }
        notify(trest[1], 0);
    }
    else if (is_(c, "shutdown", "off", "halt", "poweroff")) power_off();
    else if (is_(c, "restart", "reboot", 0, 0)) power_restart();
    else { t_puts("'", A_ERR); t_puts(tk[0], A_ERR); t_say("' is not a command. Try: help", A_ERR);
        const char *sg = suggest(tl[0]);
        if (sg) { t_puts(" Did you mean ", A_DIM); t_puts(sg, A_ACC); t_say(" ?", A_DIM); notify(sg, 0); } else notify("Unknown command", 2); }
}

static void ed_save(void) {
    memcpy(fdata[ed_node], edbuf, (unsigned)ed_len); nd[ed_node].size = ed_len; ed_on = 0;
    t_clear(); t_puts(" Saved ", A_OK); t_puts(nd[ed_node].name, A_TXT); t_puts("  (", A_DIM); t_num((u32)ed_len, 0); t_say(" bytes)", A_DIM);
    notify("Saved file", 1); prompt();
}

static void handle_key(int k) {
    dirty = 1;
    if (ux_key_gate(k)) return;
    if (folder_prompt) {
        if (k == 27) { folder_prompt = 0; new_folder_len = 0; new_folder_name[0] = 0; return; }
        if (k == '\b') { if (new_folder_len) new_folder_name[--new_folder_len] = 0; return; }
        if (k == '\n') {
            int node = -1;
            if (valid_name(new_folder_name) && folder_parent >= 0 && folder_parent < NODES && nd[folder_parent].used && nd[folder_parent].isdir && fs_find(folder_parent, new_folder_name) < 0) node = fs_new(folder_parent, new_folder_name, 1);
            if (node < 0) notify("Folder name invalid or disk full", 2); else notify("New folder created", 1);
            folder_prompt = 0; desk_menu_open = 0; new_folder_len = 0; new_folder_name[0] = 0; return;
        }
        if (k >= 32 && k < 127 && new_folder_len < 23) { new_folder_name[new_folder_len++] = (char)k; new_folder_name[new_folder_len] = 0; }
        return;
    }
    if (k == 27 && desk_menu_open) { desk_menu_open = 0; return; }
    if (gallery_open) { if (k == 27) gallery_open = 0; return; }
    if (k == KEY_SPOTLIGHT) {
        launcher_open = !launcher_open; launchpad_open = 0; control_open = 0;
        launcher_query[0] = 0; launcher_len = 0; return;
    }
    if (launchpad_open) {
        if (k == 27) { launchpad_open = 0; launchpad_query[0] = 0; launchpad_len = 0; return; }
        if (k == '\b') { if (launchpad_len) launchpad_query[--launchpad_len] = 0; return; }
        if (k == '\n') {
            int node = -1, app = -1;
            for (int i = 1; i < NODES; i++) if (nd[i].used && name_has_query(nd[i].name, launchpad_query)) { node = i; break; }
            if (node >= 0) { launchpad_open = 0; if (nd[node].isdir) desktop_open_node(node); else app_open_notes_node(node); return; }
            for (int i = 0; i < app_count(); i++) if (name_has_query(app_at(i)->name, launchpad_query)) { app = i; break; }
            if (app >= 0) { launchpad_open = 0; app_at(app)->open(); return; }
        }
        if (k >= 32 && k < 127 && launchpad_len < 46) { launchpad_query[launchpad_len++] = (char)k; launchpad_query[launchpad_len] = 0; }
        return;
    }
    if (control_open) { if (k == 27) control_open = 0; return; }
    if (launcher_open) {
        if (k == 27) { launcher_open = 0; launcher_query[0] = 0; launcher_len = 0; return; }
        if (k == '\b') { if (launcher_len) launcher_query[--launcher_len] = 0; return; }
        if (k == '\n') {
            int found = -1;
            for (int i = 0; i < app_count(); i++) {
                const char *nm = app_at(i)->name; int a = 0, matched = 0;
                while (launcher_query[a]) {
                    int j = 0; while (nm[j] && nm[j] != launcher_query[a] && nm[j] != (char)(launcher_query[a] - 32)) j++;
                    if (!nm[j]) break; a++; nm += j + 1; matched++;
                }
                if (matched == launcher_len) { found = i; break; }
            }
            launcher_open = 0;
            if (found >= 0) { int rebuild = terminal_visible; terminal_visible = 0; if (rebuild) build_scene(); app_at(found)->open(); }
            else if (launcher_len) {
                int cv; if (ux_calc_query(launcher_query, &cv)) { char rs[16], ln[24] = "= "; ux_int_str(cv, rs); int z = 2; for (int q = 0; rs[q]; q++) ln[z++] = rs[q]; ln[z] = 0; notify(ln, 1); }
                else if (ci_eq(launcher_query, "terminal")) focus_terminal();
                else { char cmd[64]; int z = 0; for (int j = 0; j < launcher_len && z < 62; j++) cmd[z++] = launcher_query[j]; cmd[z] = 0; run_in_terminal(cmd); }
            }
            launcher_query[0] = 0; launcher_len = 0; return;
        }
        if (k >= 32 && k < 127 && launcher_len < 46) { launcher_query[launcher_len++] = (char)k; launcher_query[launcher_len] = 0; }
        return;
    }
    if (k == KEY_ALTTAB) {                                   /* Alt+Tab: next window (or terminal <-> window) */
        win_cycle(); notify(app_on ? app_at(app_on - 1)->name : "Terminal", 0);
        return;
    }
    if (game_on) { game_key(k); return; }
    if (app_on) { app_key(k); return; }
    if (ed_on) {
        if (k == 27) ed_save();
        else if (k == '\b') { if (ed_len > 0) { ed_len--; ed_draw(); } }
        else if (k == '\n') { if (ed_len < FSIZE - 1) { edbuf[ed_len++] = '\n'; t_putc('\n', A_TXT); } }
        else if (k == '\t') { for (int n = 0; n < 4 && ed_len < FSIZE - 1; n++) { edbuf[ed_len++] = ' '; t_putc(' ', A_TXT); } }
        else if (k >= 32 && k < 127) { if (ed_len < FSIZE - 1) { edbuf[ed_len++] = (char)k; t_putc((char)k, A_TXT); } else notify("File is full", 2); }
        return;
    }
    if (!terminal_visible && !app_on && (k == '\n' || k == '\b' || (k >= 32 && k < 127))) focus_terminal();
    if (!terminal_visible && !app_on) return;
    if (k == '\n') {
        t_putc('\n', A_TXT); line[ll] = 0;
        if (ll) { scpy(hist[hn & 7], line, 256); hn++; }
        hpos = hn; exec_line(line);
        if (!ed_on) prompt();
    } else if (k == '\b') {
        if (ll > 0) { ll--; if (cx_ > 0) cx_--; else if (cy_ > 0) { cy_--; cx_ = cols - 1; } tc[cy_][cx_] = ' '; }
    } else if (k == KEY_UP || k == KEY_DOWN) {
        if (k == KEY_UP && hpos > 0 && hn - hpos < 8) hpos--; else if (k == KEY_DOWN && hpos < hn) hpos++; else return;
        while (ll > 0) { ll--; if (cx_ > 0) cx_--; else if (cy_ > 0) { cy_--; cx_ = cols - 1; } tc[cy_][cx_] = ' '; }
        if (hpos < hn) { const char *h = hist[hpos & 7]; for (; *h && ll < 250; h++) { line[ll++] = *h; t_putc(*h, A_TXT); } }
    } else if (k == '\t') { tab_complete();
    } else if (k >= 32 && k < 127 && ll < 250) { line[ll++] = (char)k; t_putc((char)k, A_TXT); }
}

/* ============================ frame ============================ */
static u32 prev[MAXW * MAXH]; static int prev_ok, cur_px = -1000, cur_py;

/* ---- anti-aliased pointer: arrow with dark outline and soft drop shadow, built once with 4x4 supersampling ---- */
static u8 cur_fill[CUR_H][CUR_W], cur_line[CUR_H][CUR_W], cur_shad[CUR_H][CUR_W]; static int cur_built;
static void cursor_build(void) {
    if (cur_built) return; cur_built = 1;
    static u8 hi[CUR_H * 4][CUR_W * 4], hd[CUR_H * 4][CUR_W * 4];
    static const int px_[7] = {1, 1, 5, 8, 10, 7, 12}, py_[7] = {1, 17, 13, 20, 19, 12, 12};  /* arrow polygon, hotspot at (1,1) */
    for (int y = 0; y < CUR_H * 4; y++) for (int x = 0; x < CUR_W * 4; x++) {
        int X = x * 2 + 1, Y = y * 2 + 1, in = 0;                                      /* sample centre, polygon scaled by 8 */
        for (int i = 0, j = 6; i < 7; j = i++) {
            int xi = px_[i] * 8, yi = py_[i] * 8, xj = px_[j] * 8, yj = py_[j] * 8;
            if (((yi > Y) != (yj > Y)) && (X < (xj - xi) * (Y - yi) / (yj - yi) + xi)) in = !in;
        }
        hi[y][x] = (u8)in;
    }
    for (int y = 0; y < CUR_H * 4; y++) for (int x = 0; x < CUR_W * 4; x++) {         /* dilate by 1 px (r = 4 samples) for the outline */
        int on = 0; for (int dy = -4; dy <= 4 && !on; dy++) for (int dx = -4; dx <= 4; dx++) {
            if (dx * dx + dy * dy > 18) continue; int yy = y + dy, xx = x + dx;
            if (yy >= 0 && yy < CUR_H * 4 && xx >= 0 && xx < CUR_W * 4 && hi[yy][xx]) { on = 1; break; } }
        hd[y][x] = (u8)on;
    }
    static u8 sh[CUR_H][CUR_W];
    for (int y = 0; y < CUR_H; y++) for (int x = 0; x < CUR_W; x++) {
        int f = 0, l = 0; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) { f += hi[y * 4 + j][x * 4 + i]; l += hd[y * 4 + j][x * 4 + i]; }
        cur_fill[y][x] = (u8)(f * 255 / 16); cur_line[y][x] = (u8)(l * 255 / 16); sh[y][x] = cur_line[y][x];
    }
    for (int y = 0; y < CUR_H; y++) for (int x = 0; x < CUR_W; x++) {                  /* shadow: outline shifted (+1,+2), blurred 5x5, ~35% strength */
        int s = 0, n = 0; for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) { int yy = y - 2 + dy, xx = x - 1 + dx; n++; if (yy >= 0 && yy < CUR_H && xx >= 0 && xx < CUR_W) s += sh[yy][xx]; }
        cur_shad[y][x] = (u8)(s / n * 90 / 255);
    }
}
static u32 cursor_px(u32 bg, int i, int j) {          /* pixel (i,j) of the pointer sprite over what the screen shows */
    u32 c = bg;
    if (cur_shad[j][i]) c = mix(c, 0x000000, cur_shad[j][i]);
    if (cur_line[j][i]) c = mix(c, 0x0A1020, cur_line[j][i]);
    if (cur_fill[j][i]) c = mix(c, 0xFFFFFF, cur_fill[j][i]);
    return c;
}
#include "display.h"
#ifndef HOSTTEST
static void fb_put(int x, int y, u32 c) { fb_store(fbp + (u32)y * PITCH + (u32)x * fb_bytes, &c, 1, x, y); }

static void cursor_restore(void) {       /* put back what the screen showed under the old cursor */
    if (cur_px < -500) return;
    for (int j = 0; j < CUR_H; j++) { int y = cur_py + j; if (y < 0) continue; if (y >= (int)H) break;
        int x0 = cur_px < 0 ? 0 : cur_px, x1 = cur_px + CUR_W > (int)W ? (int)W : cur_px + CUR_W;
        if (x1 > x0) fb_store(fbp + (u32)y * PITCH + (u32)x0 * fb_bytes, &prev[y * W + x0], x1 - x0, x0, y); }
    cur_px = -1000;
}
static void cursor_paint(void) {
    int ox = pmx - 1, oy = pmy - 1, x0 = ox < 0 ? 0 : ox, x1 = ox + CUR_W > (int)W ? (int)W : ox + CUR_W; u32 row[CUR_W];
    for (int j = 0; j < CUR_H; j++) { int y = oy + j; if (y < 0) continue; if (y >= (int)H) break;
        if (x1 <= x0) break;
        for (int x = x0; x < x1; x++) { int i = x - ox; u32 bg = prev[y * W + x]; row[x - x0] = (cur_shad[j][i] | cur_line[j][i] | cur_fill[j][i]) ? cursor_px(bg, i, j) : bg; }
        fb_store(fbp + (u32)y * PITCH + (u32)x0 * fb_bytes, row, x1 - x0, x0, y); }          /* one contiguous write per row */
    cur_px = ox; cur_py = oy;
}
static void cursor_sync(void) {          /* pointer moved: touch only a few hundred pixels, never wait for a frame */
    if (!mouse_seen || !prev_ok || in_flip || (pmx - 1 == cur_px && pmy - 1 == cur_py)) return;
    cursor_restore(); cursor_paint();
}
/* VGA vertical-retrace sync: used only if port 0x3DA really toggles (real VGA cores, QEMU, VirtualBox, VMware); otherwise off */
static void vsync_probe(void) {
    int lo = 0, hi = 0; u32 t0 = now_ms();
    while (now_ms() - t0 < 60) { u8 v = inb(0x3DA); if (v == 0xFF) { vsync_ok = 0; return; } if (v & 8) hi = 1; else lo = 1; }
    vsync_ok = lo && hi;
}
static void vsync_wait(void) {
    if (!vsync_ok || !vsync_on || flip_ms > 12) return;                  /* already running behind: do not stall the frame further */
    u32 t = now_ms();
    while ((inb(0x3DA) & 8) && now_ms() - t < 20) ps2_pump();           /* let a retrace in progress finish, then catch the next one */
    while (!(inb(0x3DA) & 8) && now_ms() - t < 40) ps2_pump();
}
static void flip(void) {                  /* write only the pixels that changed since the last frame */
    in_flip = 1; u32 t0 = now_ms(); int waited = 0;
    if (cur_px != pmx - 1 || cur_py != pmy - 1 || !mouse_seen) cursor_restore();   /* pointer sits still: leave it on screen (no flicker), just repaint it afterwards */
    for (u32 y = 0; y < H; y++) {
        u32 *p = back + y * W, *q = prev + y * W; u32 x0 = 0, x1 = W - 1;
        if (prev_ok) {
            while (x0 + 4 <= W && p[x0] == q[x0] && p[x0 + 1] == q[x0 + 1] && p[x0 + 2] == q[x0 + 2] && p[x0 + 3] == q[x0 + 3]) x0 += 4;
            while (x0 < W && p[x0] == q[x0]) x0++;
            if (x0 == W) continue;
            while (p[x1] == q[x1]) x1--;
        }
        if (!waited) { waited = 1; in_flip = 0; vsync_wait(); in_flip = 1; t0 = now_ms(); }   /* first changed row: start right after retrace to stay ahead of the beam */
        if (fb_native) memcpy(fbp + y * PITCH + x0 * 4, p + x0, (x1 - x0 + 1) * 4);
        else fb_store(fbp + y * PITCH + x0 * fb_bytes, p + x0, (int)(x1 - x0 + 1), (int)x0, (int)y);
        memcpy(q + x0, p + x0, (x1 - x0 + 1) * 4);
        if ((y & 63) == 63) ps2_pump();                                         /* keep reading mouse bytes (pointer is repainted below) */
    }
    flip_ms = (int)(now_ms() - t0);
    prev_ok = 1; in_flip = 0;
    if (mouse_seen) { if (cur_px != pmx - 1 || cur_py != pmy - 1) cursor_restore(); cursor_paint(); }   /* pointer may have moved during the flip: clear the old spot too */
}
#else
static void flip(void) { memcpy(prev, back, (unsigned)W * H * 4); }
static void cursor_sync(void) {}
#endif
/* PixelBeeOS menu bar: familiar desktop workflow, with its own mark, labels and status language. */
static int bar_day, bar_mon;   /* kept for the clock refresh; the top menu bar itself has been removed */

/* Desktop shortcuts: original PixelBeeOS vector artwork, with the wallpaper visible behind them. */
static void desk_folder_icon(u32 *b, int x, int y, u32 body) {
    draw_rr(b, x + 4, y + 5, 22, 10, 4, body, 255);
    draw_rr(b, x + 2, y + 10, 42, 29, 8, body, 248);
    draw_line(b, x + 7, y + 15, x + 38, y + 15, 0xFFFFFF, 68);
}
static void desk_pc_icon(u32 *b, int x, int y) {
    draw_rr(b, x + 3, y + 3, 42, 29, 7, 0x0A203A, 255);
    draw_rr(b, x + 6, y + 6, 36, 23, 4, 0x4BC2E8, 255);
    draw_line(b, x + 8, y + 23, x + 39, y + 10, 0xD6FBFF, 170);
    draw_rr(b, x + 21, y + 32, 7, 6, 2, 0xDCEBFA, 255);
    draw_rr(b, x + 13, y + 37, 23, 4, 2, 0xDCEBFA, 255);
}
static void desk_home_icon(u32 *b, int x, int y) {
    draw_line(b, x + 3, y + 20, x + 24, y + 3, 0xFFE3A0, 255);
    draw_line(b, x + 24, y + 3, x + 45, y + 20, 0xFFE3A0, 255);
    draw_rr(b, x + 8, y + 18, 32, 23, 5, 0xE9A95D, 255);
    draw_rr(b, x + 20, y + 26, 9, 15, 2, 0x6C4262, 255);
}
static void desk_drive_icon(u32 *b, int x, int y) {
    draw_rr(b, x + 2, y + 8, 44, 30, 8, 0xAFC9E8, 245);
    draw_rr(b, x + 5, y + 11, 38, 22, 5, 0x314A70, 255);
    draw_rr(b, x + 10, y + 26, 22, 3, 1, 0xAFC9E8, 210);
    draw_rr(b, x + 35, y + 25, 4, 4, 2, 0x7CFFC5, 255);
}
static void desk_file_icon(u32 *b, int x, int y, int kind) {
    desk_folder_icon(b, x, y, kind == 2 ? 0x4CACE9 : (kind == 3 ? 0x39B9A5 : 0x7D8FE7));
    if (kind == 2) {
        draw_rr(b, x + 17, y + 2, 16, 22, 3, 0xF3F7FF, 245);
        draw_line(b, x + 20, y + 8, x + 29, y + 8, 0x5284B8, 255);
        draw_line(b, x + 20, y + 13, x + 29, y + 13, 0x5284B8, 255);
    } else if (kind == 3) {
        draw_line(b, x + 24, y + 16, x + 24, y + 31, 0xFFFFFF, 255);
        draw_line(b, x + 18, y + 25, x + 24, y + 31, 0xFFFFFF, 255);
        draw_line(b, x + 24, y + 31, x + 30, y + 25, 0xFFFFFF, 255);
    } else {
        draw_line(b, x + 10, y + 29, x + 20, y + 20, 0xE7F7FF, 255);
        draw_line(b, x + 20, y + 20, x + 28, y + 26, 0xE7F7FF, 255);
        draw_line(b, x + 28, y + 26, x + 35, y + 18, 0xE7F7FF, 255);
    }
}
static void draw_desktop_icons(u32 *b) {
    static const char *labels[6] = {"This PC", "Home", "Documents", "Downloads", "Pictures", "RAM Disk (C:)"};
    int compact = (H < 560 || W < 760), cellw = compact ? 102 : 88, step = compact ? 88 : 76;
    int xbase = 18, ybase = 54;
    for (int i = 0; i < 6; i++) {
        int col = compact ? i / 3 : 0, row = compact ? i % 3 : i;
        int x = xbase + col * cellw, y = ybase + row * step;
        int over = hov(x, y, 66, 67);
        if (over) draw_rr(b, x - 5, y - 4, 76, 69, 12, 0xA9D9FF, (u32)hmix(0, 48, over));
        if (i == 0) desk_pc_icon(b, x + 8, y);
        else if (i == 1) desk_home_icon(b, x + 8, y);
        else if (i == 2) desk_file_icon(b, x + 8, y, 2);
        else if (i == 3) desk_file_icon(b, x + 8, y, 3);
        else if (i == 4) desk_file_icon(b, x + 8, y, 4);
        else desk_drive_icon(b, x + 8, y);
        if (i == 5) {
            text(b, x + 3, y + 46, "RAM Disk", 0x07172B); text(b, x + 3, y + 45, "RAM Disk", 0xF4F8FF);
            text(b, x + 23, y + 58, "(C:)", 0x07172B); text(b, x + 23, y + 57, "(C:)", 0xF4F8FF);
        } else {
            int tw = 8 * slen(labels[i]);
            text(b, x + (66 - tw) / 2, y + 47, labels[i], 0x07172B);
            text(b, x + (66 - tw) / 2, y + 46, labels[i], 0xF4F8FF);
        }
        hit_add(x - 4, y - 4, 74, 70, 3000 + i);
    }
    /* User-created desktop folders appear as real shortcuts. */
    { int slot = 0; for (int n = 1; n < NODES; n++) if (nd[n].used && nd[n].isdir && nd[n].parent == desktop_node) {
        int col = compact ? slot / 3 : 0, row = compact ? slot % 3 : 6 + slot;
        int x = xbase + col * cellw, y = ybase + row * step;
        desk_folder_icon(b, x + 8, y, 0xD7A95F);
        text(b, x + 3, y + 47, nd[n].name, 0x07172B); text(b, x + 3, y + 46, nd[n].name, 0xFFFFFF);
        hit_add(x - 4, y - 4, 74, 70, 3100 + n); slot++;
    } }
}

static void draw_control_center(u32 *b) {
    if (!control_open) return;
    int x = (int)W - 358, y = 46, w = 340, h = 332;
    liquid_panel(b, bgblur, x, y, w, h, 24, 14, 75, 1);
    text(b, x + 20, y + 16, "QUICK CONTROLS", 0xF1F4FF);
    text(b, x + w - 30, y + 16, "x", pal[A_DIM]); hit_add(x + w - 40, y + 8, 30, 28, 2104);
    /* Network status tile. */
    draw_rr(b, x + 14, y + 44, w - 28, 55, 14, 0xFFFFFF, 34);
    text(b, x + 28, y + 54, "NETWORK", pal[A_ACC]);
    { char ln[56]; if (net_cfg) { scpy(ln, "Connected  ", 14); ip_str(net_ip, ln + 11); } else scpy(ln, net_nic.up ? "Not connected - click to connect" : (pci_net_present ? "Card not supported - click for details" : "No network card - click for details"), 56);
      text(b, x + 28, y + 75, ln, net_cfg ? pal[A_OK] : pal[A_TXT]); hit_add(x + 14, y + 44, w - 28, 55, 2010); }
    /* Display control is a software framebuffer brightness adjustment. */
    draw_rr(b, x + 14, y + 109, w - 28, 74, 14, 0xFFFFFF, 34);
    text(b, x + 28, y + 120, "DISPLAY", pal[A_ACC]);
    { char v[8]; fmt_base((u32)display_level, 10, v); text(b, x + w - 72, y + 120, v, pal[A_TXT]); text(b, x + w - 72 + 8 * slen(v), y + 120, "%", pal[A_TXT]); }
    draw_rr(b, x + 28, y + 151, w - 120, 7, 4, 0x17284E, 180);
    draw_rr(b, x + 28, y + 151, (w - 120) * display_level / 100, 7, 4, 0x74DDF2, 245);
    draw_rr(b, x + w - 78, y + 141, 24, 24, 12, 0xFFFFFF, hmix(48, 100, hov(x + w - 78, y + 141, 24, 24)));
    text(b, x + w - 72, y + 148, "-", pal[A_TXT]); hit_add(x + w - 78, y + 141, 24, 24, 2100);
    draw_rr(b, x + w - 48, y + 141, 24, 24, 12, 0xFFFFFF, hmix(48, 100, hov(x + w - 48, y + 141, 24, 24)));
    text(b, x + w - 42, y + 148, "+", pal[A_TXT]); hit_add(x + w - 48, y + 141, 24, 24, 2101);
    /* Honest battery / charging status: never render a guessed percentage. */
    draw_rr(b, x + 14, y + 193, w - 28, 48, 14, 0xFFFFFF, 34);
    text(b, x + 28, y + 202, "BATTERY & CHARGING", pal[A_ACC]);
    text(b, x + 28, y + 220, acpi_present ? "ACPI found; battery driver not ready" : "Battery status unavailable", pal[A_TXT]);
    /* The sound driver currently drives the legacy PC speaker for short alerts only. */
    draw_rr(b, x + 14, y + 251, w - 28, 62, 14, 0xFFFFFF, 34);
    text(b, x + 28, y + 260, "SOUND ALERTS", pal[A_ACC]);
    text(b, x + 28, y + 279, alerts_enabled ? "PC speaker alerts enabled" : "PC speaker alerts muted", pal[A_TXT]);
    text(b, x + 28, y + 297, audio_device_present ? "PCI audio found; mixer driver pending" : "No PCI audio device detected", pal[A_DIM]);
    draw_rr(b, x + w - 82, y + 270, 54, 28, 12, alerts_enabled ? 0x318E9A : 0x647A91, 190);
    text(b, x + w - 70, y + 279, alerts_enabled ? "ON" : "OFF", 0xFFFFFF);
    hit_add(x + w - 82, y + 270, 54, 28, 2102);
}

/* Desktop glass dashboard: keeps the real desktop shortcuts visible while surfacing
 * the PixelBeeOS workspace, app shortcuts, Spotlight search and quick controls at a glance. */
static void draw_desktop_dashboard(u32 *b) {
    if (terminal_visible || launcher_open || launchpad_open || control_open) return;
    for (int i = 0; i < nwin; i++) if (!win_[i].min) return;
    int wide = W >= 900;
    int x = 24, y = 22;
    /* Compact floating clock/date glass, matching the reference desktop. */
    int topw = wide ? 420 : (int)W - 48;
    liquid_panel(b, bgblur, x, y, topw, 48, 18, 14, 75, 1);
    {   /* live time and date from the hardware clock (no hard-coded values) */
        static const char *dn[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        static const int tt[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int ch, cm, cs, cd, cmo, cy; rtc_read(&ch, &cm, &cs, &cd, &cmo, &cy);
        int yy_ = cmo < 3 ? cy - 1 : cy, wd = (yy_ + yy_ / 4 - yy_ / 100 + yy_ / 400 + tt[(cmo + 11) % 12] + cd) % 7;
        char tb[8], db[40]; int k = 0;
        tb[0] = (char)('0' + ch / 10 % 10); tb[1] = (char)('0' + ch % 10); tb[2] = ':'; tb[3] = (char)('0' + cm / 10 % 10); tb[4] = (char)('0' + cm % 10); tb[5] = 0;
        for (const char *q = dn[wd]; *q; q++) db[k++] = *q;
        db[k++] = ','; db[k++] = ' ';
        db[k++] = (char)('0' + cd / 10 % 10); db[k++] = (char)('0' + cd % 10); db[k++] = '-';
        db[k++] = (char)('0' + cmo / 10 % 10); db[k++] = (char)('0' + cmo % 10); db[k++] = '-';
        db[k++] = (char)('0' + cy / 1000 % 10); db[k++] = (char)('0' + cy / 100 % 10); db[k++] = (char)('0' + cy / 10 % 10); db[k++] = (char)('0' + cy % 10); db[k] = 0;
        text(b, x + 16, y + 9, tb, 0xFFFFFF);
        text(b, x + 72, y + 9, db, 0xEAF5FF);
    }
    hit_add(x, y, 60, 48, 2001); hit_add(x + 64, y, 72, 48, 2002);
    if (wide) {
        int wx0 = (int)W - 130;
        liquid_panel(b, bgblur, wx0, y, 106, 48, 18, 14, 75, 1);
        /* Wi-Fi, speaker and battery glyphs */
        { int wa = net_cfg ? 245 : 110; nt_wifi(b, wx0 + 22, y + 21, 9, 0xFFFFFF, wa); if (!net_cfg) draw_line(b, wx0 + 11, y + 32, wx0 + 33, y + 10, 0xFF8F8F, 240); }   /* bright = online, dim + slash = offline */
        draw_rr(b, wx0 + 40, y + 15, 5, 13, 2, 0xFFFFFF, 220);
        draw_line(b, wx0 + 45, y + 15, wx0 + 52, y + 11, 0xFFFFFF, 220);
        draw_rr(b, wx0 + 57, y + 14, 13, 18, 3, 0xEAF5FF, 190);
        draw_rr(b, wx0 + 70, y + 19, 3, 8, 1, 0xEAF5FF, 190);
        hit_add(wx0, y, 106, 48, 2004);   /* tray opens Control Center */
        hit_add(wx0, y, 40, 48, 2010);    /* the Wi-Fi glyph opens the Network app directly */
    }
    /* Media player intentionally omitted: no preselected song or track is shown. */
}

static int name_has_query(const char *name, const char *q) {
    if (!q[0]) return 1;
    for (int i = 0; name[i]; i++) {
        int a = i, j = 0;
        while (name[a] && q[j]) {
            char x = name[a], y = q[j];
            if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
            if (x != y) break;
            a++; j++;
        }
        if (!q[j]) return 1;
    }
    return 0;
}
static void draw_launchpad(u32 *b) {
    if (!launchpad_open) return;
    /* iOS-style launchpad: big vivid icons, no boxes, each icon springs in with a small stagger, hover lifts and enlarges the icon. */
    int panel_w = (int)W - 32; if (panel_w > 680) panel_w = 680;
    if (panel_w < 260) panel_w = (int)W - 8;
    int total = app_count() + 1;
    int grid_cols = W >= 760 ? 5 : (W >= 520 ? 4 : 3);
    int grid_rows = (total + grid_cols - 1) / grid_cols;
    int cell_w = (panel_w - 40) / grid_cols, isize = cell_w - 40; if (isize > 60) isize = 60;
    int rh_max = ((int)H - 32 - 124) / grid_rows; if (isize + 40 > rh_max) isize = rh_max - 40; if (isize < 30) isize = 30;
    int row_h = isize + 40;
    int desired_h = launchpad_len ? 560 : 98 + grid_rows * row_h + 30;
    int panel_h = (int)H - 32; if (panel_h > desired_h) panel_h = desired_h;
    if (panel_h < 220) panel_h = (int)H - 8;
    int x = ((int)W - panel_w) / 2, y = ((int)H - panel_h) / 2;
    liquid_panel(b, bgblur, x, y, panel_w, panel_h, 30, 12, 70, 1);

    int sx = x + 22, sy = y + 20, sw = panel_w - 44;                 /* search pill with a magnifier */
    draw_rr(b, sx, sy, sw, 38, 19, 0x0A1430, PA(70));
    draw_rr(b, sx + 1, sy + 1, sw - 2, 36, 18, 0xFFFFFF, 20);
    dcirc(b, sx + 22, sy + 18, 6, 0xFFFFFF, 200); dcirc(b, sx + 22, sy + 18, 4, 0x1B2A4A, 255); draw_line(b, sx + 26, sy + 22, sx + 31, sy + 27, 0xFFFFFF, 200);
    text(b, sx + 42, sy + 11, launchpad_query[0] ? launchpad_query : "Search", launchpad_query[0] ? 0xFFFFFF : 0xB4C4DA);
    if (launchpad_len) draw_rr(b, sx + 42 + launchpad_len * 8, sy + 11, 2, 16, 1, 0x6FE3FF, ((cur_ms / 500) & 1) ? 0 : 230);
    text(b, sx + sw - 8 * 3 - 14, sy + 11, "Esc", 0x93A8C4);
    hit_add(sx, sy, sw, 38, 2299);
    sy = sy - 2;                                                      /* search-mode rows below keep their original spacing */
    sy += 0;

    if (launchpad_len) {
        int ry = sy + 52, shown = 0;
        text(b, x + 24, ry, "SEARCH RESULTS", 0xA9DDFB); ry += 23;
        for (int i = 1; i < NODES && shown < 9; i++) if (nd[i].used && name_has_query(nd[i].name, launchpad_query)) {
            int yy = ry + shown * 34;
            draw_rr(b, x + 18, yy, panel_w - 36, 30, 10, 0xFFFFFF, hmix(25, 62, hov(x + 18, yy, panel_w - 36, 30)));
            text(b, x + 30, yy + 9, nd[i].isdir ? "FOLDER" : "FILE", nd[i].isdir ? 0xF5D58B : 0xA9DDFB);
            text(b, x + 105, yy + 9, nd[i].name, 0xFFFFFF);
            hit_add(x + 18, yy, panel_w - 36, 30, 2300 + i); shown++;
        }
        for (int i = 0; i < app_count() && shown < 12; i++) if (name_has_query(app_at(i)->name, launchpad_query)) {
            int yy = ry + shown * 34;
            draw_rr(b, x + 18, yy, panel_w - 36, 30, 10, 0xFFFFFF, hmix(25, 62, hov(x + 18, yy, panel_w - 36, 30)));
            text(b, x + 30, yy + 9, "APP", 0xA9E8C7); text(b, x + 105, yy + 9, app_at(i)->name, 0xFFFFFF);
            hit_add(x + 18, yy, panel_w - 36, 30, 2400 + i); shown++;
        }
        if (!shown) text(b, x + 26, ry + 8, "No matching files, folders or apps", 0xD7E4F2);
        text(b, x + 24, y + panel_h - 22, "Type to search  |  Enter opens the first match", 0x9AB6CE);
        return;
    }

    int grid_x = x + 20, grid_y = y + 78;
    u32 age0 = cur_ms - ov_t0[1]; int settling = 0;
    for (int i = 0; i < total; i++) {
        int col = i % grid_cols, row = i / grid_cols;
        int tx0 = grid_x + col * cell_w, ty0 = grid_y + row * row_h, tw = cell_w - 4, th = row_h - 4;
        int hv = hov(tx0, ty0, tw, th);
        int delay = 40 + (row * grid_cols + col) * 22; int t = (int)age0 - delay; if (t < 0) t = 0;
        int es = t >= 340 ? 255 : ease_back((u32)t, 340); if (t < 340) settling = 1;
        int sz = isize * (150 + es * 106 / 255) / 256 + hv * 7 / 255;          /* 58% -> 100% (small overshoot), grows a little under the pointer */
        int cx = tx0 + tw / 2, iy = ty0 + 4 + (isize - sz) / 2 - (255 - (es > 255 ? 255 : es)) * 10 / 255 - hv * 3 / 255;
        if (hv) draw_rr(b, tx0 + 2, ty0, tw - 4, th, 18, 0xFFFFFF, (u32)hmix(0, 30, hv));
        draw_rr(b, cx - sz / 2 + 2, iy + sz / 8 + 3, sz - 4, sz - 2, sz / 4, 0x050A1E, 52);              /* soft contact shadow */
        mac_icon(b, cx - sz / 2, iy, sz, i == 0 ? 1 : 10 + (i - 1));
        if (t > 120 || age0 > 600) {                                                                       /* labels appear once the icon has nearly landed */
            const char *nm = i == 0 ? "Terminal" : app_at(i - 1)->name; int max_chars = (tw - 4) / 8; if (max_chars < 4) max_chars = 4;
            int ly = ty0 + isize + 14;
            if (slen(nm) <= max_chars) text(b, cx - 4 * slen(nm), ly, nm, 0xF2F8FF);
            else { char sn[24]; int kk = max_chars - 1; if (kk > 22) kk = 22; for (int j = 0; j < kk; j++) sn[j] = nm[j]; sn[kk] = '.'; sn[kk + 1] = 0; text(b, cx - 4 * slen(sn), ly, sn, 0xF2F8FF); }
        }
        hit_add(tx0, ty0, tw, th, 1700 + i);
    }
    if (settling) anim_busy = 1;
}

static void apply_brightness(u32 *b) {
    if (display_level >= 100) return;
    int n = (int)(W * H);
    for (int i = 0; i < n; i++) {
        u32 c = b[i]; int r = ((c >> 16) & 255) * display_level / 100;
        int g = ((c >> 8) & 255) * display_level / 100, bl = (c & 255) * display_level / 100;
        b[i] = ((u32)r << 16) | ((u32)g << 8) | (u32)bl;
    }
}

static void draw_desktop_context(u32 *b) {
    if (desk_menu_open) {
        int x = desk_menu_x, y = desk_menu_y, w = 190, h = 126;
        if (x + w > (int)W) x = (int)W - w - 4; if (y + h > (int)H) y = (int)H - h - 4;
        liquid_panel(b, bgblur, x, y, w, h, 12, 14, 75, 1);
        text(b, x + 14, y + 12, "DESKTOP", 0xA9DDFB);
        text(b, x + 14, y + 36, "+  New Folder", 0xFFFFFF); hit_add(x + 4, y + 29, w - 8, 26, 2200);
        text(b, x + 14, y + 63, "  Gallery", 0xFFFFFF); hit_add(x + 4, y + 56, w - 8, 26, 2201);
        text(b, x + 14, y + 90, "  Personalize", 0xFFFFFF); hit_add(x + 4, y + 83, w - 8, 26, 2202);
    }
    if (folder_prompt) {
        int x = ((int)W - 340) / 2, y = ((int)H - 120) / 2;
        liquid_panel(b, bgblur, x, y, 340, 120, 18, 14, 75, 1);
        text(b, x + 20, y + 18, "CREATE NEW FOLDER", 0xC7ECFF);
        draw_rr(b, x + 18, y + 48, 304, 34, 9, 0xFFFFFF, 52);
        text(b, x + 30, y + 59, new_folder_name[0] ? new_folder_name : "Type a folder name...", 0xFFFFFF);
        text(b, x + 20, y + 94, "Enter: create     Esc: cancel", 0xB7C9E2);
    }
    if (gallery_open) {
        int x = 110, y = 76, w = (int)W - 220, h = (int)H - 150; if (w < 300) { x = 12; w = (int)W - 24; } if (h < 180) { y = 42; h = (int)H - 54; }
        liquid_panel(b, bgblur, x, y, w, h, 20, 14, 75, 1);
        text(b, x + 20, y + 18, "PIXELBEEOS GALLERY  -  Pictures", 0xDDF5FF); text(b, x + w - 110, y + 18, "Esc to close", 0xD3E5FA);
        int count = 0; for (int n = 1; n < NODES; n++) if (nd[n].used && nd[n].parent == pictures_node && !nd[n].isdir) {
            int cx = x + 20 + (count % 4) * ((w - 40) / 4), cy = y + 54 + (count / 4) * 82;
            draw_rr(b, cx, cy, 66, 54, 9, (count & 1) ? 0x55A9C9 : 0x806BC2, 220);
            draw_line(b, cx + 8, cy + 40, cx + 28, cy + 20, 0xFFFFFF, 190); draw_line(b, cx + 28, cy + 20, cx + 48, cy + 39, 0xFFFFFF, 190);
            text(b, cx, cy + 60, nd[n].name, 0xFFFFFF); count++;
        }
        if (!count) text(b, x + 22, y + 72, "Your Pictures folder is empty. Add images in Files to see them here.", 0xE4ECFA);
    }
}

static void compose(u32 ms) {
    anim_busy = 0; ps2_pump(); task_sync(); dock_sync(0);
    for (u32 y = 0; y < H; y += 96) { u32 n = H - y < 96 ? H - y : 96; memcpy(back + y * W, bgbuf + y * W, n * W * 4); ps2_pump(); }
    nhits = 0;
    if (!terminal_visible && desk_icons) draw_desktop_icons(back);
    if (terminal_visible) {
        text(back, wx + (ww - 8 * slen(wtitle)) / 2, wy + 9, wtitle, pal[A_DIM]);
        traffic_lights(back, wx, wy, !nwin || term_focus, 0);
        hit_add(wx + 10, wy + 5, 20, 22, 10); hit_add(wx + 32, wy + 5, 20, 22, 11);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) { char ch = tc[r][c]; if (ch > 32) glyph(back, tx + c * 8, ty + r * 16, ch, pal[ta_[r][c]]); }
            if ((r & 7) == 7) ps2_pump();
        }
        if ((!nwin || term_focus) && !hide_cur && !dock_hidden() && (ms / 500) % 2 == 0) draw_rr(back, tx + cx_ * 8, ty + cy_ * 16 + 1, 8, 14, 2, 0x6FE3FF, 210);
    }
    app_fast = 0;
    static int last_s = -1, rh, rm, rs, rd, rmo, ry;
    if ((int)(ms / 1000) != last_s) { last_s = (int)(ms / 1000); rtc_read(&rh, &rm, &rs, &rd, &rmo, &ry); bar_day = rd; bar_mon = rmo; }
    char clk[9] = "00:00:00";
    clk[0] = '0' + (rh / 10) % 10; clk[1] = '0' + rh % 10; clk[3] = '0' + (rm / 10) % 10; clk[4] = '0' + rm % 10;
    clk[6] = '0' + (rs / 10) % 10; clk[7] = '0' + rs % 10;

    draw_desktop_dashboard(back);
    wins_draw(back, ms);
    if (!dock_hidden() || dock_peek) dock_draw(back, ms);
    if (!kbd_alive && !mouse_alive && !input_seen) {         /* nothing answered on the PS/2 ports: most likely a PC with USB-only keyboard and mouse */
        int pw = 660, ph = 132, px = ((int)W - pw) / 2, py = ((int)H - ph) / 2 - 40; if (pw > (int)W - 20) { pw = (int)W - 20; px = 10; }
        liquid_panel(back, bgblur, px, py, pw, ph, 22, 14, 75, 1);
        text(back, px + 24, py + 22, "No keyboard or mouse detected", pal[A_WARN]);
        text(back, px + 24, py + 52, "This PC probably uses a USB keyboard/mouse; PixelBeeOS reads PS/2 input.", pal[A_TXT]);
        text(back, px + 24, py + 74, "In the BIOS turn on 'Legacy USB Support' (or boot in CSM/Legacy mode),", pal[A_DIM]);
        text(back, px + 24, py + 94, "or plug in a PS/2 keyboard/mouse, then restart.", pal[A_DIM]);
        dirty = 1;
    }
    int ov_l = ov_begin(0, launcher_open);
    ghost_draw(back, ms);
    if (launcher_open) {
        int pw = 520, ph = 460, px = ((int)W - pw) / 2, py = ((int)H - ph) / 2 - 25;
        draw_rr(back, px - 8, py - 8, pw + 16, ph + 16, 25, 0x061426, 116);
        liquid_panel(back, bgblur, px, py, pw, ph, 22, 14, 75, 1);
        draw_rr(back, px + 18, py + 18, pw - 36, 48, 14, 0xFFFFFF, 54);
        draw_rr(back, px + 18, py + 18, pw - 36, 48, 14, pal[A_ACC], 25);
        if (launcher_len) text(back, px + 34, py + 35, launcher_query, pal[A_TXT]); else text(back, px + 34, py + 35, "Search apps or enter a terminal command", pal[A_DIM]);
        draw_rr(back, px + 34 + launcher_len * 8, py + 35, 7, 14, 1, pal[A_ACC], 230);
        text(back, px + 22, py + 82, "APPLICATIONS", pal[A_ACC]);
        { int cv; if (launcher_len && ux_calc_query(launcher_query, &cv)) { char rs[16], ln[24] = "= "; ux_int_str(cv, rs); int z = 2; for (int q = 0; rs[q]; q++) ln[z++] = rs[q]; ln[z] = 0;
            draw_rr(back, px + 18, py + 76, pw - 36, 24, 8, 0x2C2C2E, 255); text(back, px + 22, py + 82, "CALCULATOR", pal[A_ACC]); text(back, px + pw - 40 - 8 * z, py + 82, ln, pal[A_TXT]); } }
        for (int i = 0; i < app_count() && i < 10; i++) {
            int yy = py + 104 + i * 30;
            draw_rr(back, px + 18, yy, pw - 36, 26, 8, 0xFFFFFF, hmix(12, 50, hov(px + 18, yy, pw - 36, 26)));
            app_at(i)->icon(back, px + 28, yy - 4);
            text(back, px + 76, yy + 6, app_at(i)->name, pal[A_TXT]);
            hit_add(px + 18, yy, pw - 36, 26, 1600 + i);
        }
        text(back, px + 22, py + ph - 24, "Search app names or commands  |  Enter to run  |  Esc to close", pal[A_DIM]);
    }
    ov_end(ov_l);
    { int a = ov_begin(1, launchpad_open); draw_launchpad(back); ov_end(a); }
    { int a = ov_begin(2, control_open); draw_control_center(back); ov_end(a); }
    { int a = ov_begin(3, desk_menu_open); draw_desktop_context(back); ov_end(a); }
    draw_mission_control(back);
    (void)rh; (void)rm;
    draw_island(back, ms);
    draw_lock_screen(back, ms, rh, rm);
    apply_brightness(back);
    flip();
}
static void frame(u32 ms) {
    static u32 last_phys, last_draw, last_blink = ~0u; static int started;
    cur_ms = ms;
    if (input_seen) { last_input_ms = ms; input_seen = 0; }
    else if (pw_set && autolock_min && !locked && !pw_ui && ms - last_input_ms > (u32)autolock_min * 60000u) ux_lock_now();
    if (!started) { started = 1; last_phys = ms; }
    int steps = 0;
    while (ms - last_phys >= frame_interval_ms) { last_phys += frame_interval_ms; isl_step(); if (++steps > 4) { last_phys = ms; break; } }
    if (isl_expand && ms >= isl_until) { isl_expand = 0; idle_size(); dirty = 1; }
    if (spk_active && ms >= spk_until) { speaker_off(); spk_active = 0; }
    if (pwr_arm && ms >= pwr_until) { pwr_arm = 0; dirty = 1; }
    if (timer_on) {
        static u32 last_tsec;
        if (ms >= timer_end) { timer_on = 0; idle_size(); notify("Timer finished!", 1); if (alerts_enabled) { speaker_on(880); spk_active = 1; } spk_until = ms + 600; }
        else { u32 sec = (timer_end - ms + 999) / 1000; if (sec != last_tsec) { last_tsec = sec; dirty = 1; } }
    }
    if (game_on && !gover && ms - game_last >= (u32)game_ms()) { game_last = ms; game_step(); game_draw(); }
    u32 blink = ms / 500;
    { static u32 last_asec; if (nwin && ms / 1000 != last_asec) { last_asec = ms / 1000; dirty = 1; } if (nwin && app_fast) dirty = 1; }
    if (mouse_seen && my >= dky - 20 && my < dky + dkh) dirty = 1; /* animate dock hover while pointer is nearby */
    if (anim_busy) dirty = 1;   /* keep drawing frames while something is animating */
    if ((dirty || isl_moving() || blink != last_blink) && ms - last_draw >= frame_interval_ms) {
        u32 ct0 = now_ms(); compose(ms); { int d = (int)(now_ms() - ct0); comp_ms = (comp_ms * 3 + d) / 4; anim_lowq = comp_ms > 20 || low_ram_mode; }
        dirty = 0; last_blink = blink; last_draw = ms; perf_frames++;
    }
    if (!perf_epoch) perf_epoch = ms;
    if (ms - perf_epoch >= 1000) { perf_fps = perf_frames; perf_frames = 0; perf_epoch = ms; }
}

/* ---- power: shutdown + restart (dock buttons and the terminal commands share these) ---- */
static void power_screen(const char *a, const char *b) {
    memset(back, 0, W * H * 4);
    text_s(back, ((int)W - 16 * slen(a)) / 2, (int)H / 2 - 40, a, 0xE6F4FF, 2);
    text(back, ((int)W - 8 * slen(b)) / 2, (int)H / 2 + 6, b, 0x7F9DB8);
    mouse_seen = 0; flip();
}
static void power_off(void) {
    power_screen("PixelBeeOS is shutting down", "It is now safe to turn off your computer.");
    acpi_shutdown();                                                         /* real hardware: firmware ACPI tables (acpi.h) */
    outw(0x604, 0x2000); outw(0xB004, 0x2000); outw(0x4004, 0x3400);      /* QEMU / Bochs / VirtualBox ACPI power-off */
#ifndef HOSTTEST
    for (;;) __asm__ volatile("cli; hlt");                                  /* real hardware without ACPI support: stay on the message screen */
#endif
}
static void power_restart(void) {
    power_screen("Restarting PixelBeeOS", "");
    acpi_reboot();                                                           /* FADT reset register, when the firmware has one */
    for (int t = 100000; (inb(0x64) & 2) && t; t--) {}
    outb(0x64, 0xFE);                                                        /* keyboard-controller reset line */
    outb(0xCF9, 0x02); outb(0xCF9, 0x06);                                    /* chipset reset register */
#ifndef HOSTTEST
    { struct { u16 lim; u32 base; } __attribute__((packed)) nul = {0, 0}; __asm__ volatile("lidt %0; int3" :: "m"(nul)); }   /* last resort: triple fault */
    for (;;) __asm__ volatile("cli; hlt");
#endif
}

static void kernel_init(void) {
    cursor_build();
    hardware_probe();
    apply_memory_profile(mem_kb);
    terminal_visible = 0; launchpad_open = 0; launcher_open = 0; control_open = 0;
    nd[0].used = 1; nd[0].isdir = 1; nd[0].parent = -1; cwd = 0;
    home_node = fs_new(0, "Home", 1);
    desktop_node = fs_new(home_node, "Desktop", 1);
    documents_node = fs_new(home_node, "Documents", 1);
    downloads_node = fs_new(home_node, "Downloads", 1);
    pictures_node = fs_new(home_node, "Pictures", 1);
    music_node = fs_new(home_node, "Music", 1);
    videos_node = fs_new(home_node, "Videos", 1);
    projects_node = fs_new(home_node, "Projects", 1);
    { int welcome = fs_new(home_node, "Welcome.txt", 0); if (welcome > 0) {
        const char *msg = "Welcome to PixelBeeOS. Your files are stored in the temporary C: RAM disk in this build.\nPhysical laptop drives will appear here after a storage driver is added.\n";
        scpy(fdata[welcome], msg, FSIZE); nd[welcome].size = slen(fdata[welcome]);
    } }
    { int demo = fs_new(home_node, "Demo-Script.txt", 0); if (demo > 0) {
        const char *msg = "# PixelBeeOS demo script - open the Home folder and run:  run Demo-Script.txt\nset NAME PixelBee\necho Hello from %NAME%\nlist | wc\nif exists Welcome.txt echo Welcome.txt is here\nalias ll = list\n";
        scpy(fdata[demo], msg, FSIZE); nd[demo].size = slen(fdata[demo]);
    } }
    build_scene();
    t_clear();
    t_say(" PixelBeeOS 3.2  -  Liquid Glass Edition", A_ACC);
    t_say(" Type help, tasks, meminfo, devices or security. Press Tab to complete.", A_DIM);
    if (!kbc_ok) { t_say(" No PS/2 keyboard/mouse controller found. This PC is USB-only:", A_WARN); t_say(" enable 'USB Legacy Support' in BIOS, or use a PS/2 or laptop keyboard.", A_WARN); }
    t_putc('\n', A_TXT);
    prompt();
    { static char dm[40]; int p = 0; const char *s = "PixelBeeOS  "; while (*s) dm[p++] = *s++; char nb[12];
      fmt_base(W, 10, nb); for (char *q = nb; *q; q++) dm[p++] = *q; dm[p++] = 'x'; fmt_base(H, 10, nb); for (char *q = nb; *q; q++) dm[p++] = *q; dm[p] = 0; notify(dm, 0); }
}

#ifndef HOSTTEST
static void vga_say(const char *s) {
    volatile u16 *v = (volatile u16 *)0xB8000;
    for (int i = 0; i < 80 * 25; i++) v[i] = 0x0720;
    for (int i = 0; s[i]; i++) v[i] = 0x4F00 | (u8)s[i];
}
static void vga_say2(const char *a, const char *b, const char *c) {
    vga_say(a); volatile u16 *v = (volatile u16 *)0xB8000;
    for (int i = 0; b[i]; i++) v[160 + i] = 0x0F00 | (u8)b[i];
    for (int i = 0; c[i]; i++) v[240 + i] = 0x0700 | (u8)c[i];
}
void kmain(u32 magic, u32 mbi) {
    u32 *m = (u32 *)mbi; u8 *mb = (u8 *)mbi;
    u32 bpp = 0;
    if (magic == 0x2BADB002 && (m[0] & (1 << 12))) bpp = mb[108];
    if (magic != 0x2BADB002 || !(m[0] & (1 << 12)) || mb[109] != 1 || m[23] != 0 || !(bpp == 32 || bpp == 24 || bpp == 16 || bpp == 15) || m[25] < 320 || m[26] < 240) {
        vga_say2(" PixelBeeOS could not get a usable graphics mode from the boot loader. ",
                 " Restart and pick 'Safe graphics 1024x768' or 'Safe graphics 800x600' in the boot menu.",
                 " Needs a linear RGB framebuffer with 15/16/24/32 bits per pixel (boot with GRUB; make iso).");
        for (;;) __asm__ volatile("hlt");
    }
    u8 *base = (u8 *)m[22]; PITCH = m[24]; W = m[25]; H = m[26];
    fb_bpp = bpp; fb_bytes = (bpp + 7) / 8;
    fb_rp = mb[110]; fb_rs = mb[111]; fb_gp = mb[112]; fb_gs = mb[113]; fb_bp = mb[114]; fb_bs = mb[115];
    if (bpp >= 24 && (fb_rs != 8 || fb_gs != 8 || fb_bs != 8 || fb_rp > 24 || fb_gp > 24 || fb_bp > 24)) { fb_rp = 16; fb_gp = 8; fb_bp = 0; fb_rs = fb_gs = fb_bs = 8; }
    if (bpp == 16 && (fb_rs != 5 || fb_gs != 6 || fb_bs != 5)) { fb_rp = 11; fb_gp = 5; fb_bp = 0; fb_rs = 5; fb_gs = 6; fb_bs = 5; }
    if (bpp == 15 && (fb_rs != 5 || fb_gs != 5 || fb_bs != 5)) { fb_rp = 10; fb_gp = 5; fb_bp = 0; fb_rs = fb_gs = fb_bs = 5; }
    fb_native = bpp == 32 && fb_rp == 16 && fb_gp == 8 && fb_bp == 0;
    if (m[0] & 1) mem_kb = m[2] + 1024;
    u32 ox = 0, oy = 0;
    if (W > MAXW || H > MAXH) memset(base, 0, PITCH * H);                         /* screen larger than the UI buffers: black borders, UI centred */
    if (W > MAXW) { ox = (W - MAXW) / 2; W = MAXW; }
    if (H > MAXH) { oy = (H - MAXH) / 2; H = MAXH; }
    fbp = base + oy * PITCH + ox * fb_bytes;
    mx = pmx = (int)W / 2; my = pmy = (int)H / 2;
    outb(0x43, 0x34); outb(0x40, 0); outb(0x40, 0);       /* PIT ch0: free-running 16-bit counter */
    mouse_init();
    vsync_probe();
    kernel_init();
    for (;;) {
        int k = poll_key(); if (k) handle_key(k);
        frame(now_ms()); cursor_sync(); net_ui_tick();
    }
}
#endif
