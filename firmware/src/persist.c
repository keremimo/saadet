/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What the synth build keeps in flash besides the presets: the device settings and its last state.
 *
 * The settings record (OBJ_SETTINGS) is the main firmware's: the same layout (settings_persist.c), and the
 * main firmware's template after it kept byte for byte as it was read (this build has no template, but must not
 * lose one), so either firmware can be installed over the other.
 *
 * The last state (OBJ_SYNTH, an A/B pair repurposed from the main firmware's first project): the track's sound (engine,
 * preset, user slot, parameters, its FM6 patch and functions, CZ-1 tone, Prophet program), the globals (BPM, the
 * effects, CLK ..) and the octave. Saved when it changed and the FM-1 rests (sound.c autosave_poll: untouched for
 * a few seconds, nothing sounding), restored at power-on (state_boot). There is no project: what you hear at
 * power-on is what you left. */
#include "settings_persist.c"
/* Bounded main-loop transfer scratch, shared by SysEx, backups and updater. */
static union { uint32_t align; uint8_t raw[8192]; } transfer_scratch __attribute__((section(".pool")));
static uint32_t transfer_gen;
static uint8_t *transfer_borrow(void) { transfer_gen++; return transfer_scratch.raw; }


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
#define SYN_MAGIC 0x31534453u                     /* "SDS1" */
typedef struct {
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
} synth_state_t;
/* the size of a state from before JUNO-106, its sum straight after the Prophet program */
#define SYN_SIZE_V1 ((((uint32_t)__builtin_offsetof(synth_state_t, j106) + 3u) & ~3u) + 4u)
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

static void state_capture(synth_state_t *s)
{
    const track_t *t = &trk[0];
    uint32_t i;
    memset(s, 0, sizeof *s);
    s->magic = SYN_MAGIC;
    s->size = sizeof *s;
    for (i = 0; i < G_COUNT; i++)
        s->g[i] = song.g[i];
    s->octave = song.octave;
    s->engine = t->eng_req;
    s->preset = t->preset;
    s->user = t->user;
    s->mpc_octave=t->mpc_octave;
    s->user_native = t->user_native;
    for (i = 0; i < P_COUNT; i++)
        s->p[i] = t->p[i];
    fm6_pack(fm6_patch[0], s->fm6);
    memcpy(s->fm6_fn, fm6_fn[0], FM6_NFN);
    s->cz = cz_patch[0];
    s->p5 = *p5_patch_of(t);
    s->j106 = *j106_patch_of(t);
    s->sum = syn_hash(s, sizeof *s - 4u);
}
/* len bytes of a stored state -> today's (an older one gains the INIT tone, as a fresh JUNO-106 part); 0: neither */
static int state_upgrade(synth_state_t *s, uint32_t len)
{
    uint32_t sum, at = (uint32_t)__builtin_offsetof(synth_state_t, j106);
    if (len == sizeof *s) return 1;
    if (len != SYN_SIZE_V1 || s->magic != SYN_MAGIC || s->size != SYN_SIZE_V1) return 0;
    memcpy(&sum, (const uint8_t *)s + SYN_SIZE_V1 - 4u, 4);
    if (sum != syn_hash(s, SYN_SIZE_V1 - 4u)) return 0;
    memset((uint8_t *)s + at, 0, sizeof *s - at);
    j106_patch_init(&s->j106);
    s->size = sizeof *s;
    s->sum = syn_hash(s, sizeof *s - 4u);
    return 1;
}

_Static_assert(sizeof(synth_state_t) <= ST_PAYLOAD_MAX, "last state fits its flash object");
static int state_valid(const synth_state_t *s)
{
    if (s->magic != SYN_MAGIC || s->size != sizeof *s || s->sum != syn_hash(s, sizeof *s - 4u) ||
        (s->user>UP_SLOTS || s->user_native>1u || s->rsv || s->octave < -3 || s->octave>3 || s->mpc_octave < -3 || s->mpc_octave>3) ||
        !eng_ok(s->engine) || !cz_patch_valid(s->cz.raw) || !p5_patch_valid(&s->p5) || !j106_patch_valid(&s->j106) ||
        !fm6_fn_ok(s->fm6_fn)) return 0;
    for (uint32_t i = 0; i < FM6_PACKED; i++) if (s->fm6[i] > 127u) return 0;
    return 1;
}

/* power-on, after melodee_init: the last state in place of the default sound. A boot that crashed or hung within
 * 30 s (bootguard) skips it: a state that cannot load does not lock the FM-1 out */
static void state_boot(void)
{
#if MELODEE_FLASH
    synth_state_t *s = &syn_scratch;
    track_t *t = &trk[0];
    uint32_t i, e;
    int n;
    if (!flash_ok || (n = st_load(OBJ_SYNTH, s, sizeof *s)) < 0 || !state_upgrade(s, (uint32_t)n) || !state_valid(s))
        return;
    if (bootguard.failed) {
        ui_message("LAST SOUND SKIPPED");
        return;
    }
    e = s->engine;
    if (!eng_ok(e)) return;
    fm1_irq_off();                                      /* the audio ISR must not see half a sound */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = (int16_t)clamp(s->g[i], GP[i].min, GP[i].max);
    song.octave = (int8_t)clamp(s->octave, -3, 3);
    t->eng_req = (uint8_t)e;
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(e, i);
        t->p[i] = (int16_t)clamp(s->p[i], d->min, d->max);
    }
    t->preset = (uint8_t)(ENGINES[e]->npresets ? s->preset % ENGINES[e]->npresets : s->preset);
    t->user = s->user;
    t->mpc_octave=(int8_t)clamp(s->mpc_octave,-3,3);
    t->user_native = s->user_native;
    cz_patch[0] = s->cz;
    p5_patch[0] = s->p5; p5_ready[0] = 1;
    j106_patch[0] = s->j106; j106_ready[0] = 1;
    memcpy(fm6_fn[0], s->fm6_fn, FM6_NFN);
    fm6_fn[0][FN_ENGINE] = FM6_MODERN; /* old renderer IDs load through MSFA */
    cz_track_accept(t);
    fm1_irq_on();
    {
        uint8_t v[FP_SIZE + 1u];
        fm6_unpack(s->fm6, v);
        fm6_set_patch(0, v);
    }
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
    state_capture(s);
    if (s->sum == syn_saved && !syn_retry)
        return 1;
    if (st_save(OBJ_SYNTH, s, sizeof *s) == 0) {
        syn_saved = s->sum;
        syn_retry = 0;
    } else
        syn_retry = 1;
    return !syn_retry;
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
#endif
}

static void settings_poll(void)
{
#if MELODEE_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || transport_busy() ||
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
