/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Command 102, PPG (SAADET_PPG builds), shaped as HYDRA's 97: operation, track, payload. The program is ppg_patch.h's
 * ppg_patch_t as bytes: group A's 50 (PG_*), group B's 50, KEYB, SPLIT, the selected group, the name's 12.
 * 0 GET (the program as low-first nibble pairs), 1 PUT the same (it auditions in RAM), 2 byte edit (byte, value as two
 * 7-bit digits, low first; the name's bytes are op 3's), 3 printable name, 4 (no native message: refused).
 * Replies: 1 for GET and PUT, else the operation; the result; the track; GET's program. */
#define ED_PPG 102u
#if SAADET_PPG
static void ppg_accept_patch(uint32_t tr, const ppg_patch_t *patch)
{
    track_t *t = &trk[tr];
    panic_req |= (uint8_t)(1u << tr);
    fm1_irq_off();
    *ppg_patch_of(t) = *patch; t->eng_req = ENGI_PPG;
    t->preset = t->user = t->user_native = 0;
    for (uint32_t i = 0; i < 8u; i++) t->p[P_E0 + i] = ENG_PPG.edit[i].def;
    fm1_irq_on();
    sync_reload = 1; ui.force = 1;
}
static int ed_ppg_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd != ED_PPG) return 0;
    uint32_t op = n ? a[0] : 127u, tr = n > 1u ? a[1] : 127u, rc = n < 2u || tr >= NTRK || op > 4u;
    if (!rc) {
        if (op == 0u) {
            if (n != 2u) rc = 1;
        } else if (op == 1u) {
            ppg_patch_t patch;
            uint8_t *raw = (uint8_t *)&patch;
            if (n != 2u + 2u * PPG_REC) rc = 1;
            for (uint32_t i = 0; !rc && i < PPG_REC; i++) {
                if (a[2u + 2u * i] > 15u || a[3u + 2u * i] > 15u) rc = 1;
                else raw[i] = (uint8_t)(a[2u + 2u * i] | a[3u + 2u * i] << 4);
            }
            if (!rc && !ppg_patch_valid(&patch)) rc = 1;
            if (!rc) ppg_accept_patch(tr, &patch);
        } else if (op == 2u) {
            uint32_t v = n == 5u ? a[3] | (uint32_t)a[4] << 7 : 256u;
            if (n != 5u || a[2] >= PPG_NAME || v > 255u || trk[tr].eng_req != ENGI_PPG) rc = 1;
            else {
                ppg_patch_t p = *ppg_patch_of(&trk[tr]);
                ((uint8_t *)&p)[a[2]] = (uint8_t)v;
                if (!ppg_patch_valid(&p)) rc = 1;
                else { ((uint8_t *)ppg_patch_of(&trk[tr]))[a[2]] = (uint8_t)v; ui.force = 1; }   /* (one byte: the
                                                     * audio reads it whole or not at all) */
            }
        } else if (op == 3u) {
            if (n > 2u + PG_NAME_LEN || n < 3u || trk[tr].eng_req != ENGI_PPG) rc = 1;
            for (uint32_t i = 2u; !rc && i < n; i++) if (a[i] < 32u || a[i] > 126u) rc = 1;
            if (!rc) {
                char *name = ppg_patch_of(&trk[tr])->name;   /* (the audio never reads the name) */
                memset(name, ' ', PG_NAME_LEN);
                memcpy(name, a + 2, n - 2u);
                ui.force = 1;
            }
        } else rc = 1;                                   /* (the 2.2 sends no single program: no native message) */
    }
    ed_b(op == 0u || op == 1u ? 1u : op); ed_b(rc); ed_b(tr);
    if (!rc && op == 0u) {
        const uint8_t *r = (const uint8_t *)ppg_patch_of(&trk[tr]);
        for (uint32_t i = 0; i < PPG_REC; i++) { ed_b(r[i] & 15u); ed_b(r[i] >> 4); }
    }
    return 1;
}
#else
static int ed_ppg_handle(uint32_t cmd, const uint8_t *a, uint32_t n) { (void)cmd; (void)a; (void)n; return 0; }
#endif
