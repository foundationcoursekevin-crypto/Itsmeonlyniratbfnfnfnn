/* PixelBeeOS 3.1 - ATA (IDE) PIO disk driver, LBA28.
 *
 * Works on QEMU/VirtualBox/VMware IDE disks and on PCs whose SATA controller is in IDE / "legacy" / "compatible" mode.
 * It does NOT see disks behind an AHCI-only or NVMe controller, and it does NOT see USB sticks (that needs the USB stack).
 * Every command polls with a timeout, so a missing or dead drive can delay the OS but never hang it.
 * Probing is read-only: IDENTIFY only. Nothing is ever written to a disk by this driver unless a caller asks for a write.
 */
#ifndef PIXELBEE_ATA_H
#define PIXELBEE_ATA_H
#if defined(ATA_HOST_EMU)
/* ata_inw/ata_outw supplied by the test's controller model */
#elif defined(HOSTTEST)
static u16 ata_inw(u16 p) { (void)p; return 0; }
static void ata_outw(u16 p, u16 v) { (void)p; (void)v; }
#else
static inline u16 ata_inw(u16 p) { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void ata_outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
#endif
#define ATA_MAX 4
typedef struct { u16 io, ctl; int slave, present; u32 sectors; char model[41]; } AtaDrv;
static AtaDrv ata_drv[ATA_MAX]; static BlockDev ata_blk[ATA_MAX]; static int ata_n, ata_probed;

static void ata_delay(const AtaDrv *d) { for (int i = 0; i < 4; i++) (void)inb(d->ctl); }      /* ~400ns */
static int ata_wait(const AtaDrv *d, int need_drq) {
    for (int t = 0; t < 4000000; t++) {
        u8 st = inb((u16)(d->io + 7));
        if (st & 0x21) return -1;                                   /* ERR or DF */
        if (!(st & 0x80) && (!need_drq || (st & 0x08))) return 0;   /* not busy (and DRQ when data is expected) */
    }
    return -1;                                                      /* timeout */
}
static int ata_rw(void *ctx, u32 lba, u32 count, void *buf, int wr) {
    AtaDrv *d = (AtaDrv *)ctx; u16 *w = (u16 *)buf;
    if (lba >= 0x10000000u || count > 0x10000000u - lba) return -1;                  /* LBA28 limit */
    for (u32 s = 0; s < count; s++, lba++) {
        if (ata_wait(d, 0)) return -1;
        outb((u16)(d->io + 6), (u8)(0xE0 | (d->slave << 4) | ((lba >> 24) & 0x0F)));
        ata_delay(d);
        outb((u16)(d->io + 2), 1); outb((u16)(d->io + 3), (u8)lba); outb((u16)(d->io + 4), (u8)(lba >> 8)); outb((u16)(d->io + 5), (u8)(lba >> 16));
        outb((u16)(d->io + 7), wr ? 0x30 : 0x20);
        ata_delay(d); if (ata_wait(d, 1)) return -1;
        if (wr) for (int i = 0; i < 256; i++) ata_outw(d->io, w[s * 256 + (u32)i]);
        else for (int i = 0; i < 256; i++) w[s * 256 + (u32)i] = ata_inw(d->io);
        if (wr) { ata_delay(d); if (ata_wait(d, 0)) return -1; }
    }
    if (wr) { outb((u16)(d->io + 7), 0xE7); if (ata_wait(d, 0)) return -1; }        /* flush the drive's write cache */
    return 0;
}
static int ata_read_cb(void *c, u32 lba, u32 n, void *b) { return ata_rw(c, lba, n, b, 0); }
static int ata_write_cb(void *c, u32 lba, u32 n, const void *b) { return ata_rw(c, lba, n, (void *)b, 1); }

static int ata_identify(AtaDrv *d) {
    u16 id[256];
    outb((u16)(d->io + 6), (u8)(0xA0 | (d->slave << 4))); ata_delay(d);
    outb((u16)(d->io + 2), 0); outb((u16)(d->io + 3), 0); outb((u16)(d->io + 4), 0); outb((u16)(d->io + 5), 0);
    outb((u16)(d->io + 7), 0xEC); ata_delay(d);
    u8 st = inb((u16)(d->io + 7)); if (st == 0 || st == 0xFF) return 0;                  /* no drive / floating bus */
    for (int t = 0; t < 4000000 && (inb((u16)(d->io + 7)) & 0x80); t++) {}
    if (inb((u16)(d->io + 4)) || inb((u16)(d->io + 5))) return 0;                       /* ATAPI / SATA-in-IDE signature: not a plain ATA disk */
    if (ata_wait(d, 1)) return 0;
    for (int i = 0; i < 256; i++) id[i] = ata_inw(d->io);
    d->sectors = (u32)id[60] | ((u32)id[61] << 16); if (d->sectors > 0x0FFFFFFFu) d->sectors = 0x0FFFFFFFu;
    for (int i = 0; i < 20; i++) { d->model[i * 2] = (char)(id[27 + i] >> 8); d->model[i * 2 + 1] = (char)(id[27 + i] & 255); } d->model[40] = 0;
    for (int i = 39; i >= 0 && (d->model[i] == ' ' || !d->model[i]); i--) d->model[i] = 0;
    return d->sectors > 0;
}
static void ata_probe(void) {
    static const u16 io[2] = {0x1F0, 0x170}, ctl[2] = {0x3F6, 0x376};
    ata_n = 0; ata_probed = 1;
    for (int bus = 0; bus < 2; bus++) {
        if (inb(io[bus] + 7) == 0xFF) continue;                                          /* floating bus: nothing on this channel */
        for (int sl = 0; sl < 2 && ata_n < ATA_MAX; sl++) {
            AtaDrv *d = &ata_drv[ata_n]; memset(d, 0, sizeof *d); d->io = io[bus]; d->ctl = ctl[bus]; d->slave = sl;
            if (!ata_identify(d)) continue;
            d->present = 1; BlockDev *b = &ata_blk[ata_n]; memset(b, 0, sizeof *b);
            b->sectors = d->sectors; b->read = ata_read_cb; b->write = ata_write_cb; b->ctx = d; b->name[0] = 'd'; b->name[1] = (char)('0' + ata_n);
            ata_n++;
        }
    }
}
#endif
