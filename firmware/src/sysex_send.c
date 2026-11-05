/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Library exports use each instrument's wire format. Copy a selected stored
 * record before another bank can take the shared cache. Exporting never loads
 * a sound or writes a slot; the playing selection includes its native edits. */
static int library_sysex_send(uint32_t e, uint32_t index, int user, int live)
{
#if MELODEE_OTA
    if (!usb.config || !native_limit(e)) return 1;
    if (user && (!up_used(index) || up_engine(index) != e)) return 1;
    if (!user && index >= ENGINES[e]->npresets) return 1;
    if (live && TSEL->eng_req != e) return 1;
    if (user && !live && !native_used(e, index)) return general_sysex_send(index);
    uint8_t raw[CZ_BYTES], frame[7u + 2u * CZ_BYTES];
    uint32_t n = 0, ch = trk_midi_ch(song.sel % NTRK) & 15u;
    _Static_assert(sizeof raw >= sizeof(p5_patch_t) && sizeof raw >= sizeof(j106_patch_t) &&
                   sizeof raw >= sizeof(hy_patch_t) && sizeof frame >= HY_FRAME &&
                   sizeof raw >= FM6_PACKED, "native export scratch");
    if (e == ENGI_PROPHET) {
        p5_patch_t patch;
        if (live) patch = *p5_patch_of(TSEL);
        else if (user) memcpy(&patch, native_raw(e, index), sizeof patch);
        else if (index) p5_factory(index - 1u, &patch);
        else p5_patch_init(&patch);
        n = p5_patch_encode(&patch, frame, sizeof frame);
    } else if (e == ENGI_J106) {
        j106_patch_t patch;
        if (live) patch = *j106_patch_of(TSEL);
        else if (user) memcpy(&patch, native_raw(e, index), sizeof patch);
        else j106_tone(index, &patch);
        n = j106_patch_encode(&patch, ch, frame, sizeof frame);
    } else if (e == ENGI_HYDRA) {
        hy_patch_t patch;
        if (live) patch = *hy_patch_of(TSEL);
        else if (user) memcpy(&patch, native_raw(e, index), sizeof patch);
        else if (index) hy_factory(index - 1u, &patch);
        else hy_patch_init(&patch);
        n = hy_patch_encode(&patch, frame, sizeof frame);
    } else if (e == ENGI_FM6) {
        uint8_t voice[FP_SIZE + 1u];
        if (live) memcpy(voice, fm6_patch[song.sel % NTRK], FP_SIZE);
        else {
            if (user) memcpy(raw, native_raw(e, index), FM6_PACKED);
            else if (index < FM6_NFAC) fm6_factory(index, raw);
            else memcpy(raw, FM6_INIT, FM6_PACKED);
            fm6_unpack(raw, voice);
        }
        memcpy(frame, (uint8_t[]){0xF0, 0x43, (uint8_t)ch, 0, 1, 0x1B}, 6);
        memcpy(frame + 6, voice, FP_SIZE);
        frame[161] = sx_checksum(voice, FP_SIZE); frame[162] = 0xF7; n = 163;
    } else if (e == ENGI_CZ) {
        if (live) memcpy(raw, cz_patch[song.sel % NTRK].raw, CZ_BYTES);
        else if (user) memcpy(raw, native_raw(e, index), CZ_BYTES);
        else if (index) cz_factory(index - 1u, raw);
        else cz_patch_init(raw);
        memcpy(frame, (uint8_t[]){0xF0, 0x44, 0, 0, (uint8_t)(0x70u | ch), 0x30}, 6);
        for (uint32_t i = 0; i < CZ_BYTES; i++) {
            frame[6u + 2u * i] = raw[i] & 15u; frame[7u + 2u * i] = raw[i] >> 4;
        }
        frame[6u + 2u * CZ_BYTES] = 0xF7; n = sizeof frame;
    }
    return !n || ota_wire_send(frame, n);
#else
    (void)e; (void)index; (void)user; (void)live; return 1;
#endif
}

/* A validated single native dump is also an edit buffer. Publish at an audio
 * boundary; subsequent edits of the same sound keep held notes. The separate
 * staged copy is only committed to a user slot by SAVE. */
static void sysex_native_apply(uint32_t e, const uint8_t *raw)
{
    track_t *t = TSEL;
    uint32_t tr = song.sel % NTRK;
    fm1_irq_off();
    if (t->eng_req != e) engine_load_of(t, e, 0);
    t->preset = t->user = t->user_native = 0;
    for (uint32_t i = 0; i < 8u; i++) t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
    if (e == ENGI_J106) {
        memcpy(&j106_patch[tr], raw, sizeof(j106_patch_t)); j106_ready[tr] = 1;
    } else if (e == ENGI_HYDRA) {
        memcpy(&hy_patch[tr], raw, sizeof(hy_patch_t)); hy_ready[tr] = 1;
    } else if (e == ENGI_PROPHET) {
        uint32_t mode = t->p[P_VOICE];
        memcpy(&p5_patch[tr], raw, sizeof(p5_patch_t)); p5_ready[tr] = 1;
        p5_track_accept(t);
        if (mode != (uint32_t)t->p[P_VOICE]) panic_req |= (uint8_t)(1u << tr);
    } else if (e == ENGI_CZ) {
        memcpy(cz_patch[tr].raw, raw, CZ_BYTES); cz_track_accept(t);
    } else if (e == ENGI_FM6) {
        uint8_t voice[FP_SIZE + 1u]; fm6_unpack(raw, voice); fm6_set_patch(tr, voice);
    }
    fm1_irq_on();
    lib.pending = 0;
    sync_reload = 1; ui.force = 1;
}

/* Parameter messages following a whole dump must update the pending SAVE
 * payload too, so confirming it stores the tone currently being auditioned. */
static void sysex_native_refresh(uint32_t e)
{
    if (!sx_staged() || sx_stage.e != e || sx_stage.count != 1u || sx_stage.size != native_size(e)) return;
    uint32_t tr = song.sel % NTRK;
    if (e == ENGI_FM6) fm6_pack(fm6_patch[tr], sx_stage.raw);
    else if (e == ENGI_J106) memcpy(sx_stage.raw, j106_patch_of(TSEL), sizeof(j106_patch_t));
}
