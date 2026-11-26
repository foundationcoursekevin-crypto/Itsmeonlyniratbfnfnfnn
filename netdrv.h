/* PixelBeeOS 3.1 - network card drivers (polled, no interrupts, flat memory so DMA address == pointer):
 *   Intel 8254x "e1000" family (QEMU default, VirtualBox "Intel PRO/1000", VMware e1000)  - MMIO + descriptor rings
 *   Realtek RTL8139 (QEMU -nic model=rtl8139, old PCs)                                    - port I/O + one RX ring
 * NOT supported: Wi-Fi, Realtek RTL8168/8169 (most modern PCs), Intel e1000e/I2xx, virtio-net, USB adapters.
 * Needs net.h first, plus pci_rd/outl/inl/outb/inb/outw/inw from the kernel. */
#ifndef PIXELBEE_NETDRV_H
#define PIXELBEE_NETDRV_H
static void pci_wr32(int bus, int dev, int fn, int off, u32 v) { outl(0xCF8, 0x80000000u | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) | ((u32)off & 0xFC)); outl(0xCFC, v); }
#define NET_BARRIER() __asm__ volatile("" ::: "memory")
#ifndef NET_HOST_MODEL
static inline u16 net_inw(u16 p) { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
#endif
static u32 net_dma(const void *p) { return (u32)(unsigned long)p; }

/* ---------------------------------------------------------------- Intel e1000 */
typedef struct { u32 lo, hi; u16 len, csum; u8 status, err; u16 special; } __attribute__((packed)) E1kRx;
typedef struct { u32 lo, hi; u16 len; u8 cso, cmd, status, css; u16 special; } __attribute__((packed)) E1kTx;
#define E1K_RXN 16
#define E1K_TXN 8
static E1kRx e1k_rx[E1K_RXN] __attribute__((aligned(128))); static E1kTx e1k_tx[E1K_TXN] __attribute__((aligned(128)));
static u8 e1k_rxb[E1K_RXN][2048] __attribute__((aligned(16))), e1k_txb[E1K_TXN][2048] __attribute__((aligned(16)));
static int e1k_rcur, e1k_tcur; static u32 e1k_bar;
#ifdef NET_HOST_MODEL
static u32 e1k_r(u32 reg); static void e1k_w(u32 reg, u32 v);          /* supplied by the test's device model */
#else
static u32 e1k_r(u32 reg) { return *(volatile u32 *)(unsigned long)(e1k_bar + reg); }
static void e1k_w(u32 reg, u32 v) { *(volatile u32 *)(unsigned long)(e1k_bar + reg) = v; }
#endif
static int e1k_send(const u8 *f, int n) {
    if (n > 1514 || n < 14) return -1; int i = e1k_tcur;
    for (int t = 0; !(e1k_tx[i].status & 1) && t < 200000; t++) NET_BARRIER(); if (!(e1k_tx[i].status & 1)) return -1;
    memcpy(e1k_txb[i], f, (unsigned)n); e1k_tx[i].lo = net_dma(e1k_txb[i]); e1k_tx[i].hi = 0; e1k_tx[i].len = (u16)n; e1k_tx[i].cmd = 0x0B; e1k_tx[i].status = 0; NET_BARRIER();
    e1k_tcur = (i + 1) % E1K_TXN; e1k_w(0x3818, (u32)e1k_tcur);
    for (int t = 0; !(e1k_tx[i].status & 1) && t < 200000; t++) NET_BARRIER();
    return (e1k_tx[i].status & 1) ? 0 : -1;
}
static int e1k_recv(u8 *f, int max) {
    E1kRx *d = &e1k_rx[e1k_rcur]; NET_BARRIER(); if (!(d->status & 1)) return 0;
    int n = d->len, ok = (d->status & 2) && !d->err && n >= 14 && n <= max; if (ok) memcpy(f, e1k_rxb[e1k_rcur], (unsigned)n);
    d->status = 0; NET_BARRIER(); e1k_w(0x2818, (u32)e1k_rcur); e1k_rcur = (e1k_rcur + 1) % E1K_RXN; return ok ? n : 0;
}
static int e1k_init(int bus, int dev, int fn, u32 bar0) {
    pci_wr32(bus, dev, fn, 4, pci_rd(bus, dev, fn, 4) | 7u); e1k_bar = bar0 & ~0xFu; if (!e1k_bar) return 0;
    e1k_w(0x0D8, 0xFFFFFFFFu); e1k_w(0x000, e1k_r(0x000) | (1u << 26));                  /* interrupts off, device reset */
    for (int t = 0; (e1k_r(0x000) & (1u << 26)) && t < 1000000; t++) {} e1k_w(0x0D8, 0xFFFFFFFFu); (void)e1k_r(0x0C0);
    e1k_w(0x000, (e1k_r(0x000) | (1u << 6) | (1u << 5)) & ~(1u << 31));                  /* set link up, auto speed */
    u32 ral = e1k_r(0x5400), rah = e1k_r(0x5404);
    if (rah & 0x80000000u) { net_nic.mac[0] = (u8)ral; net_nic.mac[1] = (u8)(ral >> 8); net_nic.mac[2] = (u8)(ral >> 16); net_nic.mac[3] = (u8)(ral >> 24); net_nic.mac[4] = (u8)rah; net_nic.mac[5] = (u8)(rah >> 8); }
    else { for (int w = 0; w < 3; w++) { e1k_w(0x14, 1u | ((u32)w << 8)); u32 v = 0; for (int t = 0; t < 100000 && !((v = e1k_r(0x14)) & 0x10); t++) {} net_nic.mac[w * 2] = (u8)(v >> 16); net_nic.mac[w * 2 + 1] = (u8)(v >> 24); } }
    for (int i = 0; i < 128; i++) e1k_w(0x5200 + (u32)i * 4, 0);                           /* clear multicast table */
    memset(e1k_rx, 0, sizeof e1k_rx); memset(e1k_tx, 0, sizeof e1k_tx);
    for (int i = 0; i < E1K_RXN; i++) e1k_rx[i].lo = net_dma(e1k_rxb[i]);
    for (int i = 0; i < E1K_TXN; i++) { e1k_tx[i].lo = net_dma(e1k_txb[i]); e1k_tx[i].status = 1; }
    e1k_rcur = e1k_tcur = 0;
    e1k_w(0x2800, net_dma(e1k_rx)); e1k_w(0x2804, 0); e1k_w(0x2808, E1K_RXN * 16); e1k_w(0x2810, 0); e1k_w(0x2818, E1K_RXN - 1);
    e1k_w(0x3800, net_dma(e1k_tx)); e1k_w(0x3804, 0); e1k_w(0x3808, E1K_TXN * 16); e1k_w(0x3810, 0); e1k_w(0x3818, 0);
    e1k_w(0x100, (1u << 1) | (1u << 15) | (1u << 26));                                   /* RX enable, accept broadcast, strip CRC, 2048-byte buffers */
    e1k_w(0x400, (1u << 1) | (1u << 3) | (0x0Fu << 4) | (0x40u << 12)); e1k_w(0x410, 0x0060200Au);   /* TX enable, pad short packets */
    return 1;
}

/* ---------------------------------------------------------------- Realtek RTL8139 */
static u16 rtl_io; static u8 rtl_rxb[8192 + 16 + 1536 + 16] __attribute__((aligned(16))), rtl_txb[4][2048] __attribute__((aligned(16))); static u32 rtl_off; static int rtl_tcur;
static int rtl_send(const u8 *f, int n) {
    if (n > 1514 || n < 14) return -1; int i = rtl_tcur; memcpy(rtl_txb[i], f, (unsigned)n);
    outl((u16)(rtl_io + 0x20 + 4 * i), net_dma(rtl_txb[i])); outl((u16)(rtl_io + 0x10 + 4 * i), (u32)n);       /* writing the size starts the transmit */
    u32 st = 0; for (int t = 0; t < 300000; t++) { st = inl((u16)(rtl_io + 0x10 + 4 * i)); if (st & 0xA000) break; }
    rtl_tcur = (i + 1) & 3; return (st & 0x8000) ? 0 : -1;
}
static int rtl_recv(u8 *f, int max) {
    if (inb((u16)(rtl_io + 0x37)) & 1) return 0;                                         /* BUFE: ring empty */
    const u8 *h = rtl_rxb + rtl_off; u32 st = (u32)(h[0] | h[1] << 8), len = (u32)(h[2] | h[3] << 8); int n = (int)len - 4, ok = (st & 1) && len >= 18 && len <= 1522 && n <= max;
    if (ok) memcpy(f, h + 4, (unsigned)n);
    else rtl_off = (u32)(net_inw((u16)(rtl_io + 0x3A)) & 0x1FFF) , rtl_off &= ~3u;           /* garbage header: resynchronise with the chip's write pointer */
    if (ok) rtl_off = (rtl_off + len + 4 + 3) & ~3u;
    rtl_off %= 8192; outw((u16)(rtl_io + 0x38), (u16)(rtl_off - 16)); return ok ? n : 0;
}
static int rtl_init(int bus, int dev, int fn, u32 bar0) {
    pci_wr32(bus, dev, fn, 4, pci_rd(bus, dev, fn, 4) | 7u); rtl_io = (u16)(bar0 & ~3u); if (!rtl_io) return 0;
    outb((u16)(rtl_io + 0x52), 0); outb((u16)(rtl_io + 0x37), 0x10); for (int t = 0; (inb((u16)(rtl_io + 0x37)) & 0x10) && t < 1000000; t++) {}
    for (int i = 0; i < 6; i++) net_nic.mac[i] = inb((u16)(rtl_io + i));
    memset(rtl_rxb, 0, sizeof rtl_rxb); rtl_off = 0; rtl_tcur = 0; outl((u16)(rtl_io + 0x30), net_dma(rtl_rxb)); outw((u16)(rtl_io + 0x3C), 0);
    outl((u16)(rtl_io + 0x44), 0x8Eu);                                                    /* accept physical match, multicast, broadcast; WRAP */
    outb((u16)(rtl_io + 0x37), 0x0C); outw((u16)(rtl_io + 0x38), (u16)(0 - 16)); return 1;
}

/* ---------------------------------------------------------------- probing */
static u32 net_unsup_id; static int net_unsup_bus = -1;
static int net_probe(void) {
    net_nic.up = 0; net_nic.send = 0; net_nic.recv = 0; net_unsup_id = 0; net_unsup_bus = -1;
    for (int bus = 0; bus < 16; bus++) for (int dev = 0; dev < 32; dev++) for (int fn = 0; fn < 8; fn++) {
        u32 id = pci_rd(bus, dev, fn, 0); if (id == 0xFFFFFFFFu || id == 0) { if (!fn) break; continue; }
        u32 vendor = id & 0xFFFF, devid = id >> 16, cls = pci_rd(bus, dev, fn, 8) >> 16;
        int e1k = 0; if (vendor == 0x8086) { static const u16 ids[] = {0x100E, 0x100F, 0x1004, 0x1008, 0x1026, 0x1027, 0x1028, 0x1076, 0x1077, 0x1078, 0x1079, 0x107C}; for (unsigned k = 0; k < sizeof ids / sizeof ids[0]; k++) if (ids[k] == devid) e1k = 1; }
        u32 bar0 = pci_rd(bus, dev, fn, 0x10);
        if (e1k && !(bar0 & 1) && e1k_init(bus, dev, fn, bar0)) { net_nic.send = e1k_send; net_nic.recv = e1k_recv; net_nic.up = 1; memcpy(net_nic.name, "Intel e1000", 12); return 1; }
        if (vendor == 0x10EC && devid == 0x8139 && (bar0 & 1) && rtl_init(bus, dev, fn, bar0)) { net_nic.send = rtl_send; net_nic.recv = rtl_recv; net_nic.up = 1; memcpy(net_nic.name, "Realtek RTL8139", 16); return 1; }
        if (cls == 0x0200 && !net_unsup_id) { net_unsup_id = id; net_unsup_bus = bus; }
        if (!fn && !(pci_rd(bus, dev, 0, 0x0C) & 0x800000u)) break;
    }
    return 0;
}
#endif
