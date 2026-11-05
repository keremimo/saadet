/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol) and the parts'
 * sounds at power-on. */
#include "dsp.c"
#include "audio_batch.h"
#if MELODEE_PROPHET
#include "eng_prophet_test.c"
#endif
#include "eng_cz.c"
#if MELODEE_LEGACY_EXTRAS
#include "eng_formant.c"
#endif
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#if MELODEE_LEGACY_EXTRAS
#include "eng_noise.c"
#endif
#include "eng_fm6.c"            /* FM6: 6-operator FM rendered as Dexed renders it (fm6_core.c) */
#include "eng_j106.c"           /* JUNO-106 (MPL-2.0) */
#include "eng_hydra.c"          /* HYDRA: wave scanning, mutants, two filters (hydra_patch.h, tools/gen_hy_waves.py) */
#if MELODEE_SLICE
#include "eng_slice.c"
#else
static void slice_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void slice_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_RETIRED = {         /* 13 without MELODEE_SLICE: reserved, never offered (eng_ok) */
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = slice_gone_note_on,
    .render = slice_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#endif

/* Each track owns only its selected engine's working state. Simple engines
 * use voice_t alone. State is released after the old engine's switch fade. */
#include "resources.c"
_Static_assert(sizeof(fm6_part_t) <= 16384u, "FM6 state exceeds the linker engine budget");
_Static_assert(sizeof(cz_part_t) <= 16384u, "CZ state exceeds the linker engine budget");
#if MELODEE_PROPHET
_Static_assert(sizeof(p5_part_t) <= 16384u, "Prophet state exceeds the linker engine budget");
#endif
_Static_assert(sizeof(j106_part_t) <= 16384u, "JUNO-106 state exceeds the linker engine budget");
_Static_assert(HY_BYTES_OF(HY_ROWS) <= 16384u, "HYDRA state exceeds the linker engine budget");
static uint32_t eng_state_size(uint32_t e)
{
    switch (e) {
#if MELODEE_PROPHET
    case ENGI_PROPHET: return sizeof(p5_part_t);
#if MELODEE_PROPHET_PROTOTYPE
    case 0: return sizeof(p5_part_t);
#endif
#endif
#if MELODEE_LEGACY_EXTRAS
    case ENGI_PHYS: return sizeof(phys_slot_t) * PHYS_POLY;
    case 7: return sizeof(drw_part_t);
#endif
    case ENGI_CZ: return sizeof(cz_part_t);
    case ENGI_FM6: return sizeof(fm6_part_t);
    case ENGI_J106: return sizeof(j106_part_t);
    case ENGI_HYDRA: return HY_BYTES_OF(HY_ROWS);   /* (the most: both oscillators scanning; hy_bytes the patch's) */
#if MELODEE_SLICE
    case ENGI_SLICE: return sizeof(slc_rb_t);
#endif
    default: return 0;
    }
}
static void eng_state_clear(uint32_t part)
{
    if (part < NPART) resource_release(RES_ENGINE0 + part);
}
static void eng_state_reset(void) { for (uint32_t k = 0; k < NPART; k++) eng_state_clear(k); }
static int hy_prepare(const track_t *t);
static int eng_state_prepare(const track_t *t)
{
    if (t->engine == ENGI_HYDRA)
        return hy_prepare(t);
    uint32_t size = eng_state_size(t->engine);
    return !size || resource_get(RES_ENGINE0 + (uint32_t)(t - trk), size) != 0;
}
static phys_slot_t *phys_slots(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(phys_slot_t) * PHYS_POLY); }
static drw_part_t *drw_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), sizeof(drw_part_t)); }
static fm6_part_t *fm6_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(fm6_part_t)); }
static cz_part_t *cz_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(cz_part_t)); }
static j106_part_t *j106_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(j106_part_t)); }
/* HYDRA's state: the patch's size (hy_bytes) when the arena has room; else it keeps what it holds (at least the
 * HY_ROWS_MIN wave copies, which play each oscillator's first wave: hy_base's fallback) and asks for a refused size
 * again every 64 blocks (~46 ms; a failed ask is a search of the arena). A new size is asked at once, as is a shrink */
static uint8_t hy_retry[NPART];
static uint16_t hy_refused[NPART];                     /* the size the arena last refused (asked again on the beat) */
static uint32_t hy_held(uint32_t part)
{
    uint32_t have = resource[RES_ENGINE0 + part % NPART].size, base = HY_BYTES_OF(0);
    return have > base ? (have - base) / (uint32_t)(HY_PTS * sizeof(int16_t)) : 0u;
}
static int hy_prepare(const track_t *t)
{
    uint32_t part = (uint32_t)(t - trk) % NPART, id = RES_ENGINE0 + part, want = (hy_bytes(t) + 3u) & ~3u, have = resource[id].size;
    if (have == want)
        return 1;
    if (have < HY_BYTES_OF(HY_ROWS_MIN) || want < have || want != hy_refused[part] || !hy_retry[part]) {
        hy_retry[part] = 64;
        if (resource_get(id, want)) {
            hy_refused[part] = 0;
            return 1;
        }
        hy_refused[part] = (uint16_t)want;
        if (!resource[id].size && resource_get(id, HY_BYTES_OF(HY_ROWS_MIN)))
            return 1;
    } else {
        hy_retry[part]--;
    }
    return resource[id].size >= HY_BYTES_OF(HY_ROWS_MIN);
}
static hy_part_t *hy_part(uint32_t part)               /* (the state as held: hy_prepare sizes it) */
{
    uint32_t id = RES_ENGINE0 + part % NPART;
    return resource[id].size >= HY_BYTES_OF(HY_ROWS_MIN) ? resource[id].ptr : 0;
}
#if MELODEE_PROPHET
static p5_part_t *p5_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(p5_part_t)); }
#endif
#if MELODEE_SLICE
static int16_t (*slc_rbuf(uint32_t part))[SLC_RB] { return resource_get(RES_ENGINE0 + part % NPART, sizeof(slc_rb_t)); }
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
#if MELODEE_PROPHET_PROTOTYPE
    &ENG_P5_TEST,                /* test firmware only: RAM patches, NEVER persist */
#else
    &ENG_RETIRED,                /* 0: retired ANALOG; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 1: retired DIGITAL; stored ID reserved */
    &ENG_RETIRED,                /* 2: retired PHASE; stored ID reserved */
    &ENG_RETIRED,                /* 3: retired SID; stored ID reserved */
    &ENG_RETIRED,                /* 4: retired SAMPLE; reserved to preserve stored indices */
#if MELODEE_LEGACY_EXTRAS
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
#else
    &ENG_RETIRED,                /* 5: retired VOICE; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_TRIO,
#else
    &ENG_RETIRED,                /* 6: retired TRIO; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_WHEEL,
#else
    &ENG_RETIRED,                /* 7: retired WHEEL; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 8: retired GRAIN; stored index reserved */
#if MELODEE_LEGACY_EXTRAS
    &ENG_PHYS,
#else
    &ENG_RETIRED,                /* 9: retired PHYS; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 10: DRUM retired in Saadet; stored ID reserved. */
#if MELODEE_LEGACY_EXTRAS
    &ENG_NOISE,                  /* 11 */
#else
    &ENG_RETIRED,                /* 11: retired NOISE; stored ID reserved */
#endif
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if MELODEE_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_RETIRED,             /* 13: reserved (MELODEE_SLICE=0 builds without SLICE) */
#endif
    &ENG_RETIRED,                /* 14: retired OBXF; stored index reserved */
    &ENG_CZ,                     /* 15: native Casio CZ-1 */
    &ENG_RETIRED, &ENG_RETIRED, &ENG_RETIRED, /* 16..18 reserved */
    &ENG_P5_TEST,                /* 19: Prophet (ANALOG's ID 0 stays reserved) */
    &ENG_RETIRED,                /* 20: reserved: the Prophet's user namespace (USER_NATIVE_P5) */
    &ENG_J106,                   /* 21: JUNO-106 */
    &ENG_RETIRED,                /* 22: reserved: the JUNO-106's user namespace (USER_NATIVE_J106) */
    &ENG_RETIRED, &ENG_RETIRED,  /* 23, 24 reserved */
    &ENG_HYDRA,                  /* 25: HYDRA */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* preset k (< npresets) of e: its row, or one made on demand (Prophet, CZ-1: their names live in the native data).
 * Made presets take turns in two slots: two can be held at once (main loop only) */
static const preset_t *eng_preset(const engine_t *e, uint32_t k)
{
    static preset_t made[2];
    static char made_name[2][21];
    static uint8_t turn;
    if (e->presets)
        return &e->presets[k];
    turn ^= 1u;
    memset(&made[turn], 0, sizeof made[turn]);
    made_name[turn][0] = 0;
    made[turn].name = made_name[turn];
    e->preset_make(k, &made[turn], made_name[turn]);
    return &made[turn];
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the editor's list):
 * engine indices, never a retired one. The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    ENGI_PROPHET,                /* Prophet replaces ANALOG in new-sound browsing */
    12,                          /* FM6 */
#if MELODEE_LEGACY_EXTRAS
    ENGI_CZ, ENGI_J106, ENGI_HYDRA, 5,   /* CZ-1 JUNO-106 HYDRA VOICE */
    6, 7, 9,
    11,                          /* NOISE */
#else
    ENGI_CZ,                      /* CZ-1 */
    ENGI_J106,                    /* JUNO-106 (the library's lists follow this order: ui_pages.c eng_tint) */
    ENGI_HYDRA,                   /* HYDRA */
#endif
#if MELODEE_SLICE
    13,                          /* SLICE */
#endif
};

/* Retired synth records (ANALOG and DIGITAL since 2.0 too) retain their stored identity and render silence.
 * Never relabel retired records FM6. */
static int eng_extra_retired(uint32_t e)
{
#if !MELODEE_PROPHET_PROTOTYPE
    if (e == ENGI_ANALOG) return 1;
#endif
    return e == ENGI_DIGITAL || e == 2u || e == 3u || (!MELODEE_LEGACY_EXTRAS && (e == 5u || e == 6u || e == 7u || e == 9u || e == 11u));
}
/* the engines one can pick, in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e != ENGI_DRUM && e < NENGINES && !(e >= 16u && e <= 18u) && e != USER_NATIVE_P5 && !(e >= USER_NATIVE_J106 && e <= 24u) && !eng_extra_retired(e) && e != 4u && e != 8u && e != 14u && (MELODEE_SLICE || e != ENGI_SLICE);
}
static uint32_t eng_sound_idx(uint32_t e)
{
    return eng_extra_retired(e) || eng_ok(e) ? e : ENGI_FM6;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGI_FM6;                            /* (a retired or reserved engine: FM6's place) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* the parts at power-on and in a new song (engine, preset; the steps empty): poly, bass, lead, drums. IT'S A
 * PROPHET 5, FM6 SOLID BASS, CZ-1 SYNTH.LEAD 1, 808 KIT (until 2026-10-16: PROPHET FUNK BASS II, FM6 PAD, SID INIT
 * SID, 808 KIT) */
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_PROPHET, 1}, {ENGI_J106, 1}};
