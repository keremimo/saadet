/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Command 97, shaped as JUNO-106's 96: operation, track, payload. 0 GET (the patch as low-first nibble pairs), 1 PUT the
 * same, 2 byte edit (byte, value as two 7-bit digits, low first: KEY reaches 200), 3 printable name, 4 send the native
 * message. A PUT or a native message auditions in RAM; a native message waits in SAVE for explicit user storage. */
#define ED_HYDRA 97u
static void hy_accept_patch(uint32_t tr, const hy_patch_t *patch)
{
    track_t *t = &trk[tr];
    panic_req |= (uint8_t)(1u << tr);
    fm1_irq_off();
    hy_patch[tr] = *patch; hy_ready[tr] = 1; t->eng_req = ENGI_HYDRA;
    t->preset = t->user = t->user_native = 0;
    for (uint32_t i = 0; i < 8u; i++) t->p[P_E0 + i] = ENG_HYDRA.edit[i].def;
    fm1_irq_on();
    sync_reload = 1; ui.force = 1;
}
static void hy_send(uint32_t tr)
{
#if MELODEE_OTA
    uint8_t frame[HY_FRAME];
    uint32_t n = hy_patch_encode(hy_patch_of(&trk[tr % NTRK]), frame, sizeof frame);
    if (n) ota_wire_send(frame, n);
#else
    (void)tr;
#endif
}
/* Command 98, HYDRA's user waves (hydra_bank.h): operation, then a wave by its id (hydra_patch.h). 0 LIST (each user wave:
 * written, its name, the CRC-32 of its points), 1 GET (any wave: its name as seven bytes, then its points, int16 low byte
 * first, seven-to-eight packed), 2 PUT (a user wave: the same; heard at once, kept at the next rest), 3 STORE (into
 * flash now), 4 REVERT (as flash holds it), 5 CLEAR (free again). Replies: operation, result, id */
#define ED_HYWAVE 98u
#define ED_HYW_PACKED (HY_PTS * 2u / 7u * 8u + 1u + HY_PTS * 2u % 7u)
_Static_assert(HY_PTS * 2u % 7u && ED_HYW_PACKED == 586u && HY_PTS * 2u <= sizeof ed_smp_buf, "a wave's points, packed");
static void ed_hyw_name(const char *s)                /* seven bytes, 0 past the name */
{
    uint32_t i, end = 0;
    for (i = 0; i < HY_UNAME; i++) {
        if (!end && !s[i])                             /* (nothing read past the name's end) */
            end = 1;
        ed_b(end ? 0u : (uint8_t)s[i]);
    }
}
static int ed_hywave_handle(const uint8_t *a, uint32_t n)
{
    uint32_t op = n ? a[0] : 127u, id = n > 1u ? a[1] : 0u, rc = 0, k = id - HY_USER0;
    int user = id >= HY_USER0 && id < HY_USER0 + HY_UWAVES;
    if (op == 0u || op == 3u) rc = n != 1u;
    else if (op == 1u) rc = n != 2u || !id || (id > HY_NWAVE && !user);
    else if (op == 2u) rc = n != 2u + HY_UNAME + ED_HYW_PACKED || !user;
    else if (op == 4u || op == 5u) rc = n != 2u || !user;
    else rc = 1;
    if (!rc && op == 2u) {
        char name[HY_UNAME + 1u];
        uint32_t i;
        for (i = 0; i < HY_UNAME && !rc; i++) {
            name[i] = (char)a[2u + i];
            rc = name[i] && (name[i] < 32 || name[i] > 126);
        }
        name[HY_UNAME] = 0;
        if (!rc && ed_unpack7(a + 2u + HY_UNAME, ED_HYW_PACKED, ed_smp_buf, sizeof ed_smp_buf) != HY_PTS * 2u) rc = 1;
        if (!rc) {
            for (i = 0; i < HY_PTS; i++) {                 /* (in place: low byte first, as the wire has it) */
                uint16_t v = (uint16_t)(ed_smp_buf[2u * i] | (uint32_t)ed_smp_buf[2u * i + 1u] << 8);
                memcpy(ed_smp_buf + 2u * i, &v, sizeof v);
            }
            hy_uset(k, (const int16_t *)(const void *)ed_smp_buf, name, 1);
        }
    } else if (!rc && op == 3u) {
        rc = ed_flash_stop() ? 3u : hy_bank_store() ? 4u : 0u;
    } else if (!rc && op == 4u) {
        rc = ed_flash_stop() ? 3u : (uint32_t)(hy_urevert(k) != 0);
    } else if (!rc && op == 5u) {
        hy_uclear(k);
    }
    ed_b(op); ed_b(rc); ed_b(id);
    if (!rc && op == 0u) {
        ed_b(HY_UWAVES); ed_b(HY_USER0);
        for (k = 0; k < HY_UWAVES; k++) {
            const hy_uwave_t *w = hy_uwave(k);
            ed_b(w->used);
            ed_hyw_name(w->name);
            ed_bk_u32(st_crc32(w->pts, sizeof w->pts));
        }
    } else if (!rc && op == 1u) {
        const int16_t *p = user ? hy_uwave(k)->pts : HY_WAVES[id - 1u];
        uint32_t i;
        ed_hyw_name(HY_ALL_NAMES[id]);
        for (i = 0; i < HY_PTS; i++) {
            ed_smp_buf[2u * i] = (uint8_t)((uint16_t)p[i] & 255u);
            ed_smp_buf[2u * i + 1u] = (uint8_t)((uint16_t)p[i] >> 8);
        }
        ed_bk_pack(ed_smp_buf, HY_PTS * 2u);
    }
    return 1;
}
static int ed_hydra_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_HYWAVE) return ed_hywave_handle(a, n);
    if (cmd != ED_HYDRA) return 0;
    uint32_t op = n ? a[0] : 127u, tr = n > 1u ? a[1] : 127u, rc = n < 2u || tr >= NTRK || op > 4u;
    if (!rc) {
        if (op == 0u) {
            if (n != 2u) rc = 1;
        } else if (op == 1u) {
            hy_patch_t patch;
            if (n != 2u + 2u * HY_BYTES) rc = 1;
            for (uint32_t i = 0; !rc && i < HY_BYTES; i++) {
                if (a[2u + 2u * i] > 15u || a[3u + 2u * i] > 15u) rc = 1;
                else patch.raw[i] = (uint8_t)(a[2u + 2u * i] | a[3u + 2u * i] << 4);
            }
            if (!rc && !hy_patch_valid(&patch)) rc = 1;
            if (!rc) hy_accept_patch(tr, &patch);
        } else if (op == 2u) {
            uint32_t v = n == 5u ? a[3] | (uint32_t)a[4] << 7 : 0u;
            if (n != 5u || a[2] >= NELEM(HY_PANEL) || !HY_PANEL[a[2]].label || trk[tr].eng_req != ENGI_HYDRA ||
                v > (uint32_t)(HY_PANEL[a[2]].max - HY_PANEL[a[2]].min)) rc = 1;
            else { hy_patch_of(&trk[tr])->raw[a[2]] = (uint8_t)v; ui.force = 1; }
        } else if (op == 3u) {
            if (n > 2u + HY_NAME_LEN || n < 3u || trk[tr].eng_req != ENGI_HYDRA) rc = 1;
            for (uint32_t i = 2u; !rc && i < n; i++) if (a[i] < 32u || a[i] > 126u) rc = 1;
            if (!rc) {
                char name[HY_NAME_LEN + 1u];
                memcpy(name, a + 2, n - 2u); name[n - 2u] = 0;
                hy_name_set(hy_patch_of(&trk[tr]), name);   /* (the audio never reads the name) */
                ui.force = 1;
            }
        } else if (n != 2u) rc = 1;
        else hy_send(tr);
    }
    ed_b(op == 0u || op == 1u ? 1u : op); ed_b(rc); ed_b(tr);
    if (!rc && op == 0u) {
        const uint8_t *r = hy_patch_of(&trk[tr])->raw;
        for (uint32_t i = 0; i < HY_BYTES; i++) { ed_b(r[i] & 15u); ed_b(r[i] >> 4); }
    }
    return 1;
}
/* the native message without F0/F7: it auditions and waits in SAVE. 0: not one */
static int hy_native_frame(const uint8_t *body, uint32_t n)
{
    hy_patch_t patch;
    if (n < 3u || body[0] != 0x7Du || body[1] != 0x48u || body[2] != 0x59u) return 0;
    if (!hy_patch_decode(&patch, body, n)) {
        ui_message("INVALID HYDRA");
        return 1;
    }
    sysex_native_apply(ENGI_HYDRA, (const uint8_t *)&patch);
    sysex_stage(ENGI_HYDRA, (const uint8_t *)&patch, 1, sizeof patch);
    return 1;
}
