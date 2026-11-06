/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The two parts for the editor (EDITOR_PROTOCOL.md "Performances", "Pushes", "Voices"):
 *   99 PERF: operation, slot [, name]. 0 LIST (slot 0): the count, the one loaded or stored last (0 none), then per slot
 *      used, part A's engine and the name; 1 LOAD; 2 STORE the setup as it stands under a name; 3 RENAME; 4 DELETE.
 *      Replies: operation, slot, result (0 done, 1 refused, 2 stop playback first, 3 the flash write failed).
 *   100 PUSH as a request: the parts' engines and native hashes, and HYDRA's user wave generation, so an editor that
 *      polls instead of watching sees a change at the price of one short reply.
 *   101 VOICES: the shared voice budget as it plays now, in quarter steps (VQ a voice of cost 1). */

static int ed_perf_name(const uint8_t *a, uint32_t n, char *name)   /* 1..PERF_NAME-1 printable letters */
{
    if (!n || n >= PERF_NAME)
        return 0;
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] < 32u || a[i] > 126u)
            return 0;
        name[i] = (char)a[i];
    }
    name[n] = 0;
    return 1;
}
static uint32_t ed_perf_rc(int rc) { return !rc ? 0u : rc == 1 ? 1u : rc == 2 ? 2u : 3u; }

static void ed_u14(uint32_t v) { ed_b(v); ed_b(v >> 7); }
static void ed_u32(uint32_t v) { for (uint32_t k = 0; k < 5u; k++) ed_b(v >> (7u * k)); }

static int ed_parts_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_PERF) {
        uint32_t op = n ? a[0] : 127u, k = n > 1u ? a[1] : 127u, rc = n < 2u || op > 4u || k >= PERF_SLOTS;
        char name[PERF_NAME];
        if (!rc) {
            if (op == 0u) rc = n != 2u;
            else if (op == 1u) rc = n != 2u ? 1u : (uint32_t)(perf_load(k) != 0);
            else if (op == 4u) rc = n != 2u ? 1u : ed_flash_stop() ? 2u : ed_perf_rc(perf_delete(k));
            else if (!ed_perf_name(a + 2, n - 2u, name)) rc = 1;
            else if (ed_flash_stop()) rc = 2;
            else rc = ed_perf_rc(op == 2u ? perf_store(k, name) : perf_rename(k, name));
        }
        ed_b(op); ed_b(k); ed_b(rc);
        if (!rc && op == 0u) {
            ed_b(PERF_SLOTS); ed_b(perf_now);
            for (k = 0; k < PERF_SLOTS; k++) {
                perf_name(k, name);
                ed_b(perf_used(k)); ed_b(perf_engine(k));
                ed_str(perf_used(k) ? name : "", PERF_NAME - 1u);
            }
        }
        return 1;
    }
    if (cmd == ED_PUSH) {
        if (n)
            return 0;
        ed_b(NTRK);
        for (uint32_t k = 0; k < NTRK; k++) {
            ed_b(ed_eng(&trk[k]));
            ed_u32(ed_native_hash(k));
        }
        ed_u32(hy_ugen);
        return 1;
    }
    if (cmd == ED_VOICES) {                       /* -> VQ, budget, ceiling, parts sounding, then per part: per-note cost,
                                                     voices playing, fading, notes waiting, cost claimed, share */
        uint32_t v[3 + NPART * 6u], j = 0;
        if (n)
            return 0;
        fm1_irq_off();                            /* (one consistent picture) */
        v[j++] = voice_budget();
        v[j++] = voice_ceiling();
        v[j++] = trk_n();
        for (uint32_t p = 0; p < NPART; p++) {
            const track_t *t = &trk[p];
            uint32_t play = 0, fade = 0;
            for (uint32_t i = 0; i < NVOICE; i++) {
                play += t->v[i].active && t->v[i].stage != 4u;
                fade += t->v[i].active && t->v[i].stage == 4u;
            }
            v[j++] = voice_units_now(t);
            v[j++] = play;
            v[j++] = fade;
            v[j++] = t->wait_n;
            v[j++] = part_busy(t);
            v[j++] = v[0] / (v[2] ? v[2] : 1u);
        }
        fm1_irq_on();
        ed_b(VQ); ed_u14(v[0]); ed_u14(v[1]); ed_b(v[2]); ed_b(NPART);
        for (uint32_t p = 0; p < NPART; p++) {
            const uint32_t *q = v + 3 + p * 6u;
            ed_u14(q[0]); ed_b(q[1]); ed_b(q[2]); ed_b(q[3]); ed_u14(q[4]); ed_u14(q[5]);
        }
        return 1;
    }
    return 0;
}
