/* PixelBeeOS 3.2 - ACPI power control: real shutdown (S5) and reboot on actual PCs.
 *
 * Before 3.2 only the QEMU / Bochs / VirtualBox power-off ports were poked, so on a real
 * laptop "shutdown" just sat on the message screen. This reads the firmware tables:
 *   RSDP -> RSDT -> FADT ("FACP") -> DSDT -> the \_S5_ object (sleep type for "soft off"),
 * switches ACPI mode on if the firmware has not, and writes SLP_TYP | SLP_EN to PM1a/PM1b.
 * Reboot uses the FADT reset register when the firmware offers one.
 * Needs: u8/u16/u32 typedefs and outb/outw/inb. ACPI_PHYS(a) maps a physical address to a pointer
 * (identity in the kernel; tests/acpi_test.c maps it onto a synthetic table blob).
 * Limits: BIOS/CSM boot only for table discovery (GRUB's Multiboot 1 handoff does not pass the UEFI
 * RSDP; the legacy E0000-FFFFF / EBDA scan finds it on BIOS and most CSM firmware). 64-bit-only
 * XSDT firmware is handled through the RSDT that nearly all firmware still provides. */
#ifndef PIXELBEE_ACPI_H
#define PIXELBEE_ACPI_H

#ifndef ACPI_PHYS
#define ACPI_PHYS(a) ((const volatile u8 *)(unsigned long)(a))
#endif

static u32 acpi_pm1a, acpi_pm1b, acpi_smi, acpi_enable_val, acpi_slp_a, acpi_slp_b;
static u32 acpi_reset_addr, acpi_reset_val; static int acpi_reset_space = -1;   /* GAS space: 0 memory, 1 I/O */
static int acpi_s5_ok, acpi_tables_ok;

static volatile u32 acpi_bda = 0x40E;   /* BIOS data area: EBDA segment pointer (volatile so the compiler never treats low memory as a null access) */
static u32 acpi_rd16(u32 a) { const volatile u8 *p = ACPI_PHYS(a); return (u32)p[0] | ((u32)p[1] << 8); }
static u32 acpi_rd32(u32 a) { const volatile u8 *p = ACPI_PHYS(a); return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static int acpi_sum(u32 a, u32 n) { const volatile u8 *p = ACPI_PHYS(a); u8 s = 0; for (u32 i = 0; i < n; i++) s = (u8)(s + p[i]); return s == 0; }
static int acpi_sig(u32 a, const char *s) { const volatile u8 *p = ACPI_PHYS(a); for (int i = 0; i < 4; i++) if (p[i] != (u8)s[i]) return 0; return 1; }

/* find a checksummed RSDP: first KB of the EBDA, then the BIOS ROM area (16-byte aligned) */
static u32 acpi_find_rsdp(void) {
    u32 ebda = acpi_rd16(acpi_bda) << 4;
    u32 lo[2] = {ebda, 0xE0000}, hi[2] = {ebda + 1024, 0x100000};
    for (int r = 0; r < 2; r++) {
        if (r == 0 && (ebda < 0x80000 || ebda >= 0xA0000)) continue;          /* implausible EBDA pointer */
        for (u32 a = lo[r]; a + 20 <= hi[r]; a += 16) {
            const volatile u8 *p = ACPI_PHYS(a);
            if (p[0] != 'R' || p[1] != 'S' || p[2] != 'D' || p[3] != ' ' || p[4] != 'P' || p[5] != 'T' || p[6] != 'R' || p[7] != ' ') continue;
            if (!acpi_sum(a, 20)) continue;
            if (p[15] >= 2) { u32 len = acpi_rd32(a + 20); if (len < 36 || len > 4096 || !acpi_sum(a, len)) continue; }
            return a;
        }
    }
    return 0;
}

/* \_S5_ in the DSDT: NameOp [RootChar] "_S5_" PackageOp PkgLength NumElements  SLP_TYPa  SLP_TYPb ... */
static int acpi_parse_s5(u32 dsdt, u32 len) {
    const volatile u8 *d = ACPI_PHYS(dsdt);
    for (u32 i = 4; i + 8 < len; i++) {
        if (d[i] != '_' || d[i + 1] != 'S' || d[i + 2] != '5' || d[i + 3] != '_') continue;
        if (!(d[i - 1] == 0x08 || (i >= 2 && d[i - 1] == '\\' && d[i - 2] == 0x08))) continue;
        if (d[i + 4] != 0x12) continue;                                         /* PackageOp */
        u32 p = i + 5; p += 1 + ((d[p] >> 6) & 3);                             /* skip PkgLength */
        if (p + 3 >= len) return 0;
        p++;                                                                    /* NumElements */
        u32 v[2];
        for (int k = 0; k < 2; k++) {
            if (p >= len) return 0;
            if (d[p] == 0x0A) { p++; v[k] = d[p++]; }                           /* BytePrefix */
            else if (d[p] == 0x0B) { v[k] = (u32)d[p + 1] | ((u32)d[p + 2] << 8); p += 3; }
            else if (d[p] <= 1) v[k] = d[p++];                                  /* ZeroOp / OneOp */
            else return 0;
        }
        acpi_slp_a = v[0] & 7; acpi_slp_b = v[1] & 7; return 1;
    }
    return 0;
}

static int acpi_init_tables(u32 rsdp) {
    acpi_tables_ok = acpi_s5_ok = 0; acpi_pm1a = acpi_pm1b = acpi_smi = acpi_enable_val = 0; acpi_reset_space = -1;
    if (!rsdp) return 0;
    u32 rsdt = acpi_rd32(rsdp + 16);
    if (!rsdt || rsdt >= 0xFFFFF000u) return 0;
    u32 rlen = acpi_rd32(rsdt + 4);
    if (!acpi_sig(rsdt, "RSDT") || rlen < 36 || rlen > 0x10000 || !acpi_sum(rsdt, rlen)) return 0;
    u32 fadt = 0;
    for (u32 o = 36; o + 4 <= rlen; o += 4) { u32 t = acpi_rd32(rsdt + o); if (t && t < 0xFFFFF000u && acpi_sig(t, "FACP")) { fadt = t; break; } }
    if (!fadt) return 0;
    u32 flen = acpi_rd32(fadt + 4);
    if (flen < 80 || flen > 0x1000 || !acpi_sum(fadt, flen)) return 0;
    const volatile u8 *f = ACPI_PHYS(fadt);
    u32 dsdt = acpi_rd32(fadt + 40);
    acpi_smi = acpi_rd32(fadt + 48); acpi_enable_val = f[52];
    acpi_pm1a = acpi_rd32(fadt + 64); acpi_pm1b = acpi_rd32(fadt + 68);
    acpi_tables_ok = acpi_pm1a != 0 && acpi_pm1a < 0x10000;
    if (flen >= 129 && (acpi_rd32(fadt + 112) & (1u << 10)) && (f[116] == 0 || f[116] == 1) && acpi_rd32(fadt + 124) == 0) {
        acpi_reset_space = f[116]; acpi_reset_addr = acpi_rd32(fadt + 120); acpi_reset_val = f[128];
        if (acpi_reset_space == 1 && acpi_reset_addr >= 0x10000) acpi_reset_space = -1;
    }
    if (dsdt && dsdt < 0xFFFFF000u && acpi_sig(dsdt, "DSDT")) {
        u32 dlen = acpi_rd32(dsdt + 4);
        if (dlen >= 40 && dlen <= 0x400000 && acpi_sum(dsdt, dlen)) acpi_s5_ok = acpi_parse_s5(dsdt, dlen);
    }
    return acpi_tables_ok;
}

/* returns only if the firmware ignored the request */
static void acpi_shutdown(void) {
    if (!acpi_tables_ok || !acpi_s5_ok) return;
    if (!(inw((u16)acpi_pm1a) & 1) && acpi_smi && acpi_enable_val) {            /* SCI_EN clear: ask the firmware to switch ACPI on */
        outb((u16)acpi_smi, (u8)acpi_enable_val);
        for (int t = 0; t < 300000 && !(inw((u16)acpi_pm1a) & 1); t++) {}
    }
    outw((u16)acpi_pm1a, (u16)((acpi_slp_a << 10) | (1 << 13)));
    if (acpi_pm1b) outw((u16)acpi_pm1b, (u16)((acpi_slp_b << 10) | (1 << 13)));
}
static void acpi_reboot(void) {
    if (acpi_reset_space == 1) outb((u16)acpi_reset_addr, (u8)acpi_reset_val);
    else if (acpi_reset_space == 0 && acpi_reset_addr) *(volatile u8 *)(unsigned long)acpi_reset_addr = (u8)acpi_reset_val;
}
#endif
