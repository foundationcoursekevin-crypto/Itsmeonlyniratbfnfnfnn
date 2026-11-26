/* PixelBeeOS 3.1 - `disk` terminal command: real persistent storage on ATA disks through FAT32.
 *   disk list | disk mount D [P] | disk unmount | disk info | disk ls [path] | disk cat path
 *   disk save ramfile [diskpath] | disk load diskpath [ramname] | disk mkdir path | disk rm path
 *   disk format D P ERASE-D-P | disk reset ERASE
 * Only a volume labelled PIXELBEEDAT is writable; everything else mounts read-only. */
#ifndef PIXELBEE_DISK_H
#define PIXELBEE_DISK_H
static F32 vol; static int vol_disk = -1, vol_part = -1;

static const char *dk_err(int r) {
    switch (r) {
    case F32_EIO: return "Disk I/O error"; case F32_ENOENT: return "Not found"; case F32_EEXIST: return "Already exists";
    case F32_ENOSPC: return "Disk is full"; case F32_ENAME: return "Invalid name"; case F32_ENOTDIR: return "Not a folder";
    case F32_EISDIR: return "That is a folder"; case F32_EROFS: return "Volume is read-only (only PIXELBEEDAT is writable)";
    case F32_ENOTEMPTY: return "Folder is not empty"; case F32_ECORRUPT: return "Not a valid FAT32 volume"; default: return "Error";
    }
}
static int dk_num(const char *s) { int n = 0; if (!*s) return -1; for (; *s; s++) { if (*s < '0' || *s > '9') return -1; n = n * 10 + (*s - '0'); if (n > 100000) return -1; } return n; }
static void dk_fail(int r) { t_say(dk_err(r), A_ERR); }
static void dk_clock(void) { int h, mi, sc, d, mo, y; rtc_read(&h, &mi, &sc, &d, &mo, &y); f32_set_clock(y, mo, d, h, mi, sc); }
static void dk_mb(u32 sectors) { t_num(sectors / 2048, 0); t_puts(" MB", A_TXT); }

static void dk_list(void) {
    ata_probe();
    if (!ata_n) { t_say("No ATA/IDE disks found.", A_WARN); t_say("Disks behind AHCI/NVMe and USB sticks need drivers that are not built yet.", A_DIM); return; }
    for (int i = 0; i < ata_n; i++) {
        t_puts(" disk ", A_DIM); t_num((u32)i, 0); t_puts(":  ", A_DIM); t_puts(ata_drv[i].model[0] ? ata_drv[i].model : "(unnamed)", A_ACC); t_puts("  ", A_TXT); dk_mb(ata_blk[i].sectors); t_putc('\n', A_TXT);
        for (int p = 0; p < 4; p++) {
            u32 lba, n; u8 ty; if (!mbr_part(&ata_blk[i], p, &lba, &n, &ty)) continue;
            t_puts("    part ", A_DIM); t_num((u32)(p + 1), 0); t_puts(":  ", A_DIM); dk_mb(n); t_puts("   type 0x", A_DIM);
            const char *hx = "0123456789ABCDEF"; t_putc(hx[ty >> 4], A_TXT); t_putc(hx[ty & 15], A_TXT);
            if (ty == 0x0B || ty == 0x0C) {
                u8 b[512]; if (!blk_read(&ata_blk[i], lba, 1, b) && b[82] == 'F' && b[83] == 'A' && b[84] == 'T' && b[85] == '3') {
                    char lab[12]; memcpy(lab, b + 71, 11); lab[11] = 0; for (int k = 10; k >= 0 && lab[k] == ' '; k--) lab[k] = 0;
                    t_puts("   FAT32 \"", A_DIM); t_puts(lab, A_TXT); t_puts("\"", A_DIM);
                }
            }
            t_putc('\n', A_TXT);
        }
    }
}
static void dk_mount(void) {
    if (!ata_probed) ata_probe();
    int d = ntok > 2 ? dk_num(tl[2]) : -1, p = ntok > 3 ? dk_num(tl[3]) : 1;
    if (d < 0 || d >= ata_n || p < 0 || p > 4) { t_say("Usage: disk mount DISK [PART]   (see: disk list)", A_WARN); return; }
    if (vol.mounted) { f32_unmount(&vol); vol_disk = -1; }
    u32 lba = 0, n = ata_blk[d].sectors; u8 ty = 0;
    if (p > 0 && !mbr_part(&ata_blk[d], p - 1, &lba, &n, &ty)) { t_say("No such partition", A_ERR); return; }
    if (p > 0 && ty != 0x0B && ty != 0x0C) { t_say("That partition is not FAT32", A_ERR); return; }
    int r = f32_mount(&vol, &ata_blk[d], lba, n); if (r) { dk_fail(r); return; }
    vol_disk = d; vol_part = p;
    t_puts("Mounted \"", A_OK); t_puts(vol.label, A_TXT); t_puts("\" as D:  ", A_OK); t_say(vol.ro ? "READ-ONLY (label is not PIXELBEEDAT)" : "read/write", vol.ro ? A_WARN : A_OK);
    if (vol.was_dirty) t_say("Warning: the volume was not cleanly unmounted last time. Check your files.", A_WARN);
    notify(vol.ro ? "D: mounted read-only" : "D: mounted", 1);
}
static int dk_need(void) { if (!vol.mounted) { t_say("No disk mounted. Use: disk mount 0 1", A_WARN); return 0; } return 1; }
static void dk_ls(void) {
    if (!dk_need()) return; const char *path = ntok > 2 ? tl[2] : "/"; F32Ent e; int r = f32_resolve(&vol, path, &e); if (r) { dk_fail(r); return; }
    if (!(e.attr & 0x10)) { dk_fail(F32_ENOTDIR); return; }
    u32 pos = 0, nf = 0, nd_ = 0; F32Ent t; t_puts(" D:", A_DIM); t_say(path, A_ACC);
    while ((r = f32_readdir(&vol, e.cl, &pos, &t)) == 1) {
        if (nf + nd_ >= 150) { t_say("  ... (list truncated)", A_DIM); break; }
        if (t.attr & 0x10) { t_puts("  <DIR>      ", A_ACC); t_say(t.name, A_ACC); nd_++; } else { t_puts("  ", A_TXT); t_num(t.size, 8); t_puts(" B  ", A_TXT); t_say(t.name, A_TXT); nf++; }
    }
    if (r < 0) dk_fail(r);
    t_puts("  ", A_DIM); t_num(nf, 0); t_puts(" file(s), ", A_DIM); t_num(nd_, 0); t_say(" folder(s)", A_DIM);
}
static void dk_cat(void) {
    if (!dk_need()) return; if (ntok < 3) { t_say("Usage: disk cat /path/file.txt", A_WARN); return; }
    F32Ent e; int r = f32_resolve(&vol, tl[2], &e); if (r) { dk_fail(r); return; }
    static char buf[1900]; r = f32_read(&vol, &e, 0, buf, sizeof buf); if (r < 0) { dk_fail(r); return; }
    for (int i = 0; i < r; i++) { char c = buf[i]; if (c == '\n') t_putc('\n', A_TXT); else if (c >= 32 && c < 127) t_putc(c, A_TXT); else if (c != '\r') t_putc('.', A_DIM); }
    t_putc('\n', A_TXT); if ((u32)r < e.size) t_say("... (showing the first 1900 bytes)", A_DIM);
}
static void dk_save(void) {
    if (!dk_need()) return; if (ntok < 3) { t_say("Usage: disk save ramfile [/diskpath]", A_WARN); return; }
    int idx = resolve(tl[2]); if (idx < 0 || nd[idx].isdir) { t_say("RAM file not found", A_ERR); return; }
    char dst[96]; if (ntok > 3) scpy(dst, tl[3], 96); else { dst[0] = '/'; scpy(dst + 1, nd[idx].name, 90); }
    dk_clock(); int r = f32_write_file(&vol, dst, fdata[idx], (u32)nd[idx].size); if (r) { dk_fail(r); return; }
    t_puts("Saved to D:", A_OK); t_say(dst, A_TXT); notify("Saved to disk", 1);
}
static void dk_load(void) {
    if (!dk_need()) return; if (ntok < 3) { t_say("Usage: disk load /diskpath [ramname]", A_WARN); return; }
    F32Ent e; int r = f32_resolve(&vol, tl[2], &e); if (r) { dk_fail(r); return; }
    if (e.attr & 0x10) { dk_fail(F32_EISDIR); return; }
    if (e.size > FSIZE - 2) { t_say("File is larger than the RAM file limit (2046 bytes)", A_ERR); return; }
    char nm[24]; if (ntok > 3) scpy(nm, tl[3], 24); else { if (slen(e.name) > 23) { t_say("Name too long for RAM disk: give a shorter name as 2nd argument", A_ERR); return; } scpy(nm, e.name, 24); }
    if (!valid_name(nm)) { t_say("Invalid RAM file name", A_ERR); return; }
    int idx = fs_find(cwd, nm); if (idx >= 0 && nd[idx].isdir) { dk_fail(F32_EISDIR); return; }
    if (idx < 0) { idx = fs_new(cwd, nm, 0); if (idx < 0) { t_say("RAM disk is full", A_ERR); return; } }
    r = f32_read(&vol, &e, 0, fdata[idx], e.size); if (r < 0) { dk_fail(r); return; }
    nd[idx].size = r; if (r < FSIZE) fdata[idx][r] = 0; t_puts("Loaded ", A_OK); t_say(nm, A_TXT);
}
static void dk_info(void) {
    if (!dk_need()) return; t_puts(" Volume: ", A_DIM); t_say(vol.label, A_ACC);
    t_puts(" Access: ", A_DIM); t_say(vol.ro ? "read-only" : "read/write", vol.ro ? A_WARN : A_OK);
    t_puts(" Size:   ", A_DIM); dk_mb(vol.nclus * vol.spc); t_putc('\n', A_TXT);
    t_puts(" Free:   ", A_DIM); dk_mb(f32_free_clusters(&vol) * vol.spc); t_putc('\n', A_TXT);
}
static void dk_format(int reset) {
    if (!ata_probed) ata_probe();
    int d, p; const char *tok;
    if (reset) { if (!vol.mounted || vol.ro || vol_part < 1) { t_say("Factory reset needs a mounted PIXELBEEDAT volume (disk mount D P)", A_WARN); return; } d = vol_disk; p = vol_part; tok = ntok > 2 ? tl[2] : ""; }
    else { d = ntok > 2 ? dk_num(tl[2]) : -1; p = ntok > 3 ? dk_num(tl[3]) : -1; tok = ntok > 4 ? tl[4] : ""; }
    u32 lba, n; u8 ty;
    if (d < 0 || d >= ata_n || p < 1 || p > 4 || !mbr_part(&ata_blk[d], p - 1, &lba, &n, &ty)) { t_say("Usage: disk format DISK PART ERASE-DISK-PART   (partition must exist: see disk list)", A_WARN); return; }
    if (ty != 0x0B && ty != 0x0C && ty != 0xDA) { t_say("Refused: only FAT32 (0B/0C) or data (DA) partitions can be formatted. Other types may hold your files.", A_ERR); return; }
    char want[24]; int k = 0; for (const char *s = reset ? "ERASE" : "ERASE-"; *s; s++) want[k++] = *s; if (!reset) { want[k++] = (char)('0' + d); want[k++] = '-'; want[k++] = (char)('0' + p); } want[k] = 0;
    if (!ci_eq(tok, want)) {
        t_puts("This ERASES everything on disk ", A_WARN); t_num((u32)d, 0); t_puts(" partition ", A_WARN); t_num((u32)p, 0); t_puts(" (", A_WARN); dk_mb(n); t_say(").", A_WARN);
        t_puts("Nothing else on the disk is touched. To continue type:  ", A_TXT); t_puts(reset ? "disk reset " : "disk format ", A_ACC);
        if (!reset) { t_num((u32)d, 0); t_putc(' ', A_ACC); t_num((u32)p, 0); t_putc(' ', A_ACC); } t_say(want, A_ACC); return;
    }
    if (vol.mounted) { f32_unmount(&vol); vol_disk = -1; }
    int h, mi, sc, dd, mo, y; rtc_read(&h, &mi, &sc, &dd, &mo, &y);
    int r = f32_format(&ata_blk[d], lba, n, F32_DATA_LABEL, (u32)(y * 10000 + mo * 100 + dd) ^ (u32)(h * 3600 + mi * 60 + sc));
    if (r) { dk_fail(r); return; }
    t_say("Formatted as FAT32 \"PIXELBEEDAT\". Mount it with: disk mount D P", A_OK); notify("Data partition formatted", 1);
}
static void cmd_disk(void) {
    const char *s = ntok > 1 ? tl[1] : "";
    if (ci_eq(s, "list") || ci_eq(s, "scan")) dk_list();
    else if (ci_eq(s, "mount")) dk_mount();
    else if (ci_eq(s, "unmount") || ci_eq(s, "eject")) { if (!dk_need()) return; int r = f32_unmount(&vol); vol_disk = -1; if (r) dk_fail(r); else t_say("D: unmounted. Safe to power off.", A_OK); }
    else if (ci_eq(s, "info")) dk_info();
    else if (ci_eq(s, "ls") || ci_eq(s, "dir")) dk_ls();
    else if (ci_eq(s, "cat") || ci_eq(s, "type")) dk_cat();
    else if (ci_eq(s, "save")) dk_save();
    else if (ci_eq(s, "load")) dk_load();
    else if (ci_eq(s, "mkdir")) { if (!dk_need()) return; if (ntok < 3) { t_say("Usage: disk mkdir /folder", A_WARN); return; } dk_clock(); int r = f32_mkdir(&vol, tl[2]); if (r) dk_fail(r); else t_say("Folder created", A_OK); }
    else if (ci_eq(s, "rm") || ci_eq(s, "del")) { if (!dk_need()) return; if (ntok < 3) { t_say("Usage: disk rm /path", A_WARN); return; } int r = f32_remove(&vol, tl[2]); if (r) dk_fail(r); else t_say("Deleted", A_OK); }
    else if (ci_eq(s, "format")) dk_format(0);
    else if (ci_eq(s, "reset")) dk_format(1);
    else {
        t_say(" disk list                      show disks and partitions", A_TXT); t_say(" disk mount D [P]               mount partition P of disk D as D:", A_TXT);
        t_say(" disk ls|cat|mkdir|rm PATH       work with files on D:", A_TXT); t_say(" disk save FILE [/PATH]         copy a RAM file to the disk", A_TXT);
        t_say(" disk load /PATH [NAME]          copy a disk file into RAM", A_TXT); t_say(" disk unmount                   flush and eject D:", A_TXT);
        t_say(" disk format D P ERASE-D-P       create the PIXELBEEDAT data partition", A_TXT); t_say(" disk reset ERASE               factory reset the data partition", A_TXT);
    }
}
#endif
