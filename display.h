/* PixelBeeOS 3.2 - pixel format conversion for the framebuffer (0x00RRGGBB -> 15/16/24/32 bpp, any channel order).
 * Needs u8/u16/u32 and the fb_* format variables from kernel.c; tests/display_test.c checks it on the host. */
static inline u32 fb_px(u32 c) {                      /* 0x00RRGGBB -> the panel's native pixel layout */
    u32 r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    return ((r >> (8 - fb_rs)) << fb_rp) | ((g >> (8 - fb_gs)) << fb_gp) | ((b >> (8 - fb_bs)) << fb_bp);
}
static void fb_store(u8 *d, const u32 *s, int n, int x0, int y) {     /* convert + write n pixels starting at screen x0 on row y */
    if (fb_bytes == 4) { u32 *o = (u32 *)d; for (int i = 0; i < n; i++) o[i] = fb_px(s[i]); }
    else if (fb_bytes == 3) { for (int i = 0; i < n; i++) { u32 v = fb_px(s[i]); d[0] = (u8)v; d[1] = (u8)(v >> 8); d[2] = (u8)(v >> 16); d += 3; } }
    else {                                                              /* 15/16 bpp: 4x4 ordered dither so gradients do not band */
        static const u8 bay[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5}; u16 *o = (u16 *)d;
        for (int i = 0; i < n; i++) {
            u32 c = s[i], t = bay[((y & 3) << 2) | ((x0 + i) & 3)];
            u32 r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            r += (t << (8 - fb_rs)) >> 4; g += (t << (8 - fb_gs)) >> 4; b += (t << (8 - fb_bs)) >> 4;   /* dither amplitude stays below one quantisation step: black and white are exact */
            if (r > 255) { r = 255; }
            if (g > 255) { g = 255; }
            if (b > 255) { b = 255; }
            o[i] = (u16)fb_px((r << 16) | (g << 8) | b);
        }
    }
}
