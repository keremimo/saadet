/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What the synth build keeps in flash besides the presets: the device settings and its last state.
 *
 * The settings record (OBJ_SETTINGS) is the main firmware's: the same layout (settings_persist.c), and the
 * main firmware's template after it kept byte for byte as it was read (this build has no template, but must not
 * lose one), so either firmware can be installed over the other.
 *
 * The last state (OBJ_SYNTH, an A/B pair repurposed from the main firmware's first project): both parts' sounds (engine,
 * preset, user slot, parameters, its FM6 patch and functions, CZ-1 tone, Prophet program, JUNO-106 tone), the globals
 * (BPM, CLK, the parts mode ..), the octave and the part selected. Saved when it changed and the FM-1 rests (sound.c autosave_poll: untouched for
 * a few seconds, nothing sounding), restored at power-on (state_boot). There is no project: what you hear at
 * power-on is what you left. */
#include "settings_persist.c"
static void perf_boot(void);                      /* perf.c, at the end */
/* Bounded main-loop transfer scratch, shared by SysEx, backups and updater. */
static union { uint32_t align; uint8_t raw[8192]; } transfer_scratch __attribute__((section(".pool")));
static uint32_t transfer_gen;
static uint8_t *transfer_borrow(void) { transfer_gen++; return transfer_scratch.raw; }
#include "hydra_bank.c"                           /* HYDRA's user waves (hydra_bank.h) */
#include "hydra_lab.c"                            /* .. and the panel's WAVE LAB */


/* the main firmware's template (its project.c tmpl_t) after the settings: kept as read. Its size and its magic
 * are its last eight bytes */
#define TMPL_MAGIC 0x464C5054u                    /* "TPLF" */
#define TMPL_MAX (ST_PAYLOAD_MAX - sizeof(persist_t))
static struct { persist_t p; uint8_t t[TMPL_MAX]; } set_rec __attribute__((section(".pool")));
static uint32_t tmpl_len;                         /* bytes of set_rec.t in use, 0 = none */
static uint8_t persist_old;
static int template_old(void) { return persist_old; }
static void settings_old_accept(void) { persist_old = 0; }

/* a stored template (len bytes): its size and magic say it is one */
static int tmpl_blob_valid(const uint8_t *b, uint32_t len)
{
    uint32_t size, magic;
    if (len < 8u || len > TMPL_MAX) return 0;
    memcpy(&size, b + len - 8u, 4); memcpy(&magic, b + len - 4u, 4);
    return size == len && magic == TMPL_MAGIC;
}
static int tmpl_take(const uint8_t *b, uint32_t len)
{
    tmpl_len = 0;
    if (!tmpl_blob_valid(b, len)) return 0;
    memcpy(set_rec.t, b, len);
    tmpl_len = len;
    return 1;
}

/* ----------------------------------------------------------- the last state --- */
#define SYN_MAGIC 0x33534453u                     /* "SDS3": both parts, each with its HYDRA patch */
#define SYN_MAGIC2 0x32534453u                    /* "SDS2": both parts, before HYDRA */
#define SYN_MAGIC1 0x31534453u                    /* "SDS1": one part, before the parts */
#define SYN_PART_FIELDS                                                       \
    uint8_t engine, preset, user_native; int8_t mpc_octave; uint16_t user, rsv; \
    int16_t p[P_COUNT];                                                       \
    uint8_t fm6[FM6_PACKED], fm6_fn[FM6_NFN];                                 \
    cz_patch_t cz;                                                            \
    p5_patch_t p5;                                                            \
    j106_patch_t j106;
typedef struct {                                  /* a part's whole sound and its place in the mix */
    SYN_PART_FIELDS
    hy_patch_t hy;
} synth_part_t;
typedef struct { SYN_PART_FIELDS } synth_part2_t;  /* a part in an SDS2 state or an SPF1 performance: without the patch */
#undef SYN_PART_FIELDS
_Static_assert(__builtin_offsetof(synth_part_t, hy) == __builtin_offsetof(synth_part2_t, j106) + sizeof(j106_patch_t),
               "an older part is the head of today's");
#define SYN_STATE_FIELDS(part_t)                                              \
    uint32_t magic, size;                                                     \
    int16_t g[G_COUNT];                                                       \
    int8_t octave;                                                            \
    uint8_t sel;                                  /* the part selected */     \
    uint8_t rsv[2];                                                           \
    part_t part[NPART];                                                       \
    uint32_t sum;
typedef struct { SYN_STATE_FIELDS(synth_part_t) } synth_state_t;
typedef struct { SYN_STATE_FIELDS(synth_part2_t) } synth_state2_t;
#undef SYN_STATE_FIELDS
typedef struct {                                  /* SDS1: what came before the parts */
    uint32_t magic, size;
    int16_t g[G_COUNT];
    int8_t octave;
    uint8_t engine, preset, user_native; int8_t mpc_octave; uint16_t user, rsv;
    int16_t p[P_COUNT];
    uint8_t fm6[FM6_PACKED], fm6_fn[FM6_NFN];
    cz_patch_t cz;
    p5_patch_t p5;
    j106_patch_t j106;                            /* appended with JUNO-106: an older state ends before it */
    uint32_t sum;
} synth_state1_t;
/* the size of a state from before JUNO-106, its sum straight after the Prophet program */
#define SYN_SIZE_V1 ((((uint32_t)__builtin_offsetof(synth_state1_t, j106) + 3u) & ~3u) + 4u)
static synth_state_t syn_scratch __attribute__((section(".pool")));
static uint32_t syn_saved;                        /* hash of the state as flash holds it (0: unknown) */
static uint8_t syn_retry;

static uint32_t syn_hash(const void *p, uint32_t n)   /* FNV-1a */
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}

/* part k's sound as it stands */
static void part_capture(uint32_t k, synth_part_t *s)
{
    const track_t *t = &trk[k];
    uint32_t i;
    memset(s, 0, sizeof *s);
    s->engine = t->eng_req;
    s->preset = t->preset;
    s->user = t->user;
    s->mpc_octave = t->mpc_octave;
    s->user_native = t->user_native;
    for (i = 0; i < P_COUNT; i++)
        s->p[i] = t->p[i];
    s->p[P_AHOLD] = 0;                                  /* REC's latch is a gesture, not the sound: never kept */
    fm6_pack(fm6_patch[k], s->fm6);
    memcpy(s->fm6_fn, fm6_fn[k], FM6_NFN);
    s->cz = cz_patch[k];
    s->p5 = *p5_patch_of(t);
    s->j106 = *j106_patch_of(t);
    s->hy = *hy_patch_of(t);
}
static int part_valid(const synth_part_t *s)
{
    if (s->user > UP_SLOTS || s->user_native > 1u || s->rsv || s->mpc_octave < -3 || s->mpc_octave > 3 ||
        !eng_ok(s->engine) || !cz_patch_valid(s->cz.raw) || !p5_patch_valid(&s->p5) || !j106_patch_valid(&s->j106) ||
        !hy_patch_valid(&s->hy) || !fm6_fn_ok(s->fm6_fn)) return 0;
    for (uint32_t i = 0; i < FM6_PACKED; i++) if (s->fm6[i] > 127u) return 0;
    return 1;
}
/* part k takes the sound s (valid). Audio IRQs off around the track, then the FM6 patch as the panel writes it */
static void part_apply(uint32_t k, const synth_part_t *s)
{
    track_t *t = &trk[k];
    uint32_t i, e = s->engine;
    uint8_t v[FP_SIZE + 1u];
    fm1_irq_off();                                      /* the audio ISR must not see half a sound */
    t->eng_req = (uint8_t)e;
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(e, i);
        t->p[i] = (int16_t)clamp(s->p[i], d->min, d->max);
    }
    t->p[P_AHOLD] = 0;                                  /* (an earlier build's state may hold it on) */
    t->preset = (uint8_t)(ENGINES[e]->npresets ? s->preset % ENGINES[e]->npresets : s->preset);
    t->user = s->user;
    t->mpc_octave = (int8_t)clamp(s->mpc_octave, -3, 3);
    t->user_native = s->user_native;
    cz_patch[k] = s->cz;
    p5_patch[k] = s->p5; p5_ready[k] = 1;
    j106_patch[k] = s->j106; j106_ready[k] = 1;
    hy_patch[k] = s->hy; hy_ready[k] = 1;
    memcpy(fm6_fn[k], s->fm6_fn, FM6_NFN);
    fm6_fn[k][FN_ENGINE] = FM6_MODERN; /* old renderer IDs load through MSFA */
    cz_track_accept(t);
    fm1_irq_on();
    fm6_unpack(s->fm6, v);
    fm6_set_patch(k, v);
}
static void state_capture(synth_state_t *s)
{
    uint32_t i;
    memset(s, 0, sizeof *s);
    s->magic = SYN_MAGIC;
    s->size = sizeof *s;
    for (i = 0; i < G_COUNT; i++)
        s->g[i] = song.g[i];
    s->octave = song.octave;
    s->sel = song.sel;
    for (i = 0; i < NPART; i++)
        part_capture(i, &s->part[i]);
    s->sum = syn_hash(s, sizeof *s - 4u);
}
/* the parts settings a state from before them starts with: one part, part A its sound, part B as at power-on */
static void state_parts_default(synth_state_t *s)
{
    s->g[G_PMODE] = GP[G_PMODE].def;
    s->g[G_PSPLIT] = GP[G_PSPLIT].def;
    s->g[G_PCH2] = GP[G_PCH2].def;
    s->sel = 0;
    part_capture(1u, &s->part[1]);
}
/* n bytes up to to, never below from, over themselves (the last first) */
static void bytes_up(void *to, const void *from, uint32_t n)
{
    uint8_t *t = to;
    const uint8_t *f = from;
    while (n--) t[n] = f[n];
}
/* older parts laid end to end at from, n of them -> today's at to, in place (to is never below from: from the last back),
 * each with HYDRA's INIT patch */
static void parts_upgrade(synth_part_t *to, const uint8_t *from, uint32_t n)
{
    while (n--) {
        bytes_up(&to[n], from + n * sizeof(synth_part2_t), sizeof(synth_part2_t));
        hy_patch_init(&to[n].hy);
    }
}
/* len bytes of a stored state -> today's (an older one's sound becomes part A; one from before JUNO-106 gains the INIT
 * tone, as a fresh JUNO-106 part; one from before HYDRA gains HYDRA's INIT patch); 0: neither. s holds a whole state */
static int state_upgrade(synth_state_t *s, uint32_t len)
{
    synth_state1_t old;
    synth_part_t *a = &s->part[0];
    uint32_t i, sum;
    if (len == sizeof *s && s->magic == SYN_MAGIC) return 1;
    if (len == sizeof(synth_state2_t) && s->magic == SYN_MAGIC2) {
        const synth_state2_t *o = (const synth_state2_t *)s;
        if (o->size != len || o->sum != syn_hash(o, len - 4u)) return 0;
        parts_upgrade(s->part, (const uint8_t *)o->part, NPART);
        s->magic = SYN_MAGIC;
        s->size = sizeof *s;
        s->sum = syn_hash(s, sizeof *s - 4u);
        return 1;
    }
    if (len != sizeof old && len != SYN_SIZE_V1) return 0;
    memcpy(&old, s, len);
    if (old.magic != SYN_MAGIC1 || old.size != len) return 0;
    memcpy(&sum, (const uint8_t *)&old + len - 4u, 4);
    if (sum != syn_hash(&old, len - 4u)) return 0;
    if (len == SYN_SIZE_V1)
        j106_patch_init(&old.j106);
    memset(s, 0, sizeof *s);
    s->magic = SYN_MAGIC;
    s->size = sizeof *s;
    for (i = 0; i < G_COUNT; i++)
        s->g[i] = old.g[i];
    s->octave = old.octave;
    a->engine = old.engine; a->preset = old.preset; a->user_native = old.user_native;
    a->mpc_octave = old.mpc_octave; a->user = old.user; a->rsv = old.rsv;
    memcpy(a->p, old.p, sizeof a->p);
    memcpy(a->fm6, old.fm6, sizeof a->fm6);
    memcpy(a->fm6_fn, old.fm6_fn, sizeof a->fm6_fn);
    a->cz = old.cz; a->p5 = old.p5; a->j106 = old.j106;
    hy_patch_init(&a->hy);
    state_parts_default(s);
    s->sum = syn_hash(s, sizeof *s - 4u);
    return 1;
}

_Static_assert(sizeof(synth_state_t) <= ST_PAYLOAD_MAX - 32u, "last state fits its flash object");
static int state_valid(const synth_state_t *s)
{
    if (s->magic != SYN_MAGIC || s->size != sizeof *s || s->sum != syn_hash(s, sizeof *s - 4u) ||
        s->octave < -3 || s->octave > 3 || s->sel >= NPART || s->rsv[0] || s->rsv[1]) return 0;
    for (uint32_t k = 0; k < NPART; k++) if (!part_valid(&s->part[k])) return 0;
    return 1;
}

/* power-on, after melodee_init: the last state in place of the default sounds. A boot that crashed or hung within
 * 30 s (bootguard) skips it: a state that cannot load does not lock the FM-1 out */
static void state_boot(void)
{
#if MELODEE_FLASH
    synth_state_t *s = &syn_scratch;
    uint32_t i;
    int n;
    if (!flash_ok || (n = st_load(OBJ_SYNTH, s, sizeof *s)) < 0 || !state_upgrade(s, (uint32_t)n) || !state_valid(s))
        return;
    if (bootguard.failed) {
        ui_message("LAST SOUND SKIPPED");
        return;
    }
    fm1_irq_off();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = (int16_t)clamp(s->g[i], GP[i].min, GP[i].max);
    song.octave = (int8_t)clamp(s->octave, -3, 3);
    song.sel = trk_n() > 1u ? s->sel : 0u;
    fm1_irq_on();
    for (i = 0; i < NPART; i++)
        part_apply(i, &s->part[i]);
    state_capture(s);                                   /* (what flash holds, as this build reads it) */
    syn_saved = s->sum;
    sync_reload = 1;
    ui.force = 1;
#endif
}

/* sound.c autosave_poll: the FM-1 rests. The state into flash when it differs from what is there */
static int state_autosave(void)
{
#if MELODEE_FLASH
    synth_state_t *s = &syn_scratch;
    if (!flash_ok) return 1;
    if (song.playing) return 0;
    int waves = hy_bank_store();                        /* (HYDRA's user waves changed since: 0 when flash has them) */
    state_capture(s);
    if (s->sum == syn_saved && !syn_retry)
        return waves == 0;
    if (st_save(OBJ_SYNTH, s, sizeof *s) == 0) {
        syn_saved = s->sum;
        syn_retry = 0;
    } else
        syn_retry = 1;
    return !syn_retry && waves == 0;
#else
    return 1;
#endif
}

#if MELODEE_FLASH
static persist_t persist_saved;
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if MELODEE_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == SD_FLASH_ID;     /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP */
    {   /* the settings, then the main firmware's template if one follows them */
        int n = st_load(OBJ_SETTINGS, &set_rec, sizeof set_rec), ns = n;
        persist_old = 0;
        tmpl_len = 0;
        if (n > (int)sizeof set_rec.p) {
            uint8_t *t = set_rec.t;
            persist_old = (uint8_t)!tmpl_take(t, (uint32_t)n - sizeof set_rec.p);
            ns = (int)sizeof set_rec.p;
        }
        memset(&p, 0, sizeof p);
        if (ns > 0)
            memcpy(&p, &set_rec.p, (uint32_t)ns < sizeof p ? (uint32_t)ns : sizeof p);
        uint32_t palette = p.palette;
        if (settings_import(&p, ns)) {
            persist_saved = p;
            persist_old = (uint8_t)(persist_old || !palette_stored_ok(palette));
        } else persist_old = (uint8_t)st_exists(OBJ_SETTINGS);
    }
    up_boot();                                     /* user presets */
    perf_boot();                                   /* performances */
#endif
}

static void settings_poll(void)
{
#if MELODEE_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || transport_busy() ||
#ifdef SAADET_SMK37
        sounding() ||                              /* a flash write stops the audio: 3 missed renders, measured on an Elite */
#endif
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    if (template_old()) {                          /* an older firmware's record: never overwritten unasked, and */
        persist_pending = 0;                       /* nothing asks yet (an editor restore accepts it) */
        return;
    }
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    set_rec.p = p;                                 /* the record: the settings, the template (kept) after them */
    if (st_save(OBJ_SETTINGS, &set_rec, sizeof set_rec.p + tmpl_len) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if MELODEE_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}

static int shared_preferences_save(void)
{
    settings_save();
#if MELODEE_FLASH
    return persist_pending || persist_old;
#else
    return 0;
#endif
}

/* Erase only live A/B objects. Retired project addresses alias current objects or
 * even offset zero; preflight every address before touching flash. */
static int persist_erase_all(void)
{
#if MELODEE_FLASH
    uint32_t obj, copy, total = 0, done = 0;
    if (!flash_ok || transport_busy()) return -1;
    for (obj = 0; obj < OBJ_COUNT; obj++) {
        if (st_retired(obj)) continue;
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = st_sector(obj, copy);
            if (SD_SECTOR_BAD(off, ST_SECTOR)) return -1;
            total++;
        }
    }
    BOOT_WORK_BEGIN("Erasing everything", total);
    for (obj = 0; obj < OBJ_COUNT; obj++) {
        if (st_retired(obj)) continue;
        for (copy = 0; copy < 2u; copy++) {
            uint32_t off = st_sector(obj, copy), at, i;
            if (st_erase(off)) { BOOT_WORK_END(1); return -1; }
            for (at = 0; at < ST_SECTOR; at += sizeof st_buf) {
                if (st_read(off + at, st_buf, sizeof st_buf)) { BOOT_WORK_END(1); return -1; }
                for (i = 0; i < sizeof st_buf; i++)
                    if (st_buf[i] != 255u) { BOOT_WORK_END(1); return -1; }
            }
            BOOT_WORK_PROGRESS(++done);
        }
    }
    panel.magic = settings.magic = 0;              /* .noinit must not restore calibration or settings */
    reset_intended();
    BOOT_WORK_END(0);
    fm1_reboot();
    return 0;
#else
    return -1;
#endif
}

#include "perf.c"
