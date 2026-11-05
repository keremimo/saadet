/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Editor protocol: SysEx for the web editor (web/EDITOR_PROTOCOL.md; v2 = user presets + live sync,
 * v3 = four tracks: the v1 / v2 commands act on the selected track, cmds 27-30 reach any track;
 * v4 = TRACK_PARAM (31) and the TRACK_CHANGED push (32), enabled by WATCH bit 1).
 * The synth build: two parts, no sequencer: the step, pattern, song, motion and project commands get no reply;
 * INFO says 0 steps and carries tag 0x53 ('S', the synth build).
 *   F0 7D 46 4C cmd args.. F7     (7D = non-commercial ID, "FL")
 * Values are 14 bit, two 7-bit bytes LSB first, offset by 8192 (so -8192..8191).
 * Every request gets a reply with the same cmd; 23/24/26 are also pushed
 * while watched. Frames arrive through sx_frame (usb.c), replies leave
 * through ota_wire_send(). */
#define ED_HDR0 0x7D
#define ED_HDR1 0x46
#define ED_HDR2 0x4C
enum { ED_INFO = 1, ED_GET, ED_SET, ED_DUMP, ED_DESC, ED_STEP_GET, ED_STEP_SET, ED_PRESET, ED_PROJECT, ED_NAMES,
       ED_SMP_BEGIN, ED_SMP_WRITE, ED_SMP_END, ED_SMP_ERASE, ED_SMP_INFO,
       ED_UP_LIST, ED_UP_GET, ED_UP_PUT, ED_UP_STORE, ED_UP_LOAD, ED_UP_ERASE,   /* v2: user presets */
       ED_WATCH, ED_CHANGED, ED_RELOAD, ED_PING, ED_STEP_CHANGED,              /* v2: live sync */
       ED_TRACK, ED_TRACK_MIX, ED_TRACK_DUMP, ED_TRACK_STEP,                    /* v3: tracks */
       ED_TRACK_PARAM, ED_TRACK_CHANGED, ED_SONG,
       ED_UI_STATE, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET,
       ED_MOTION = 64, ED_BACKUP_LIST, ED_BACKUP_GET, ED_BACKUP_PUT,                               /* v6: song chain */
       ED_AUDIO_STATS = 72, ED_PATTERN, ED_BANK_SONG, ED_BATTERY = 81 };                        /* USB audio diagnostics (68..71: FM6, editor_fm6.c) */
#if MELODEE_CORE1_TEST && MELODEE_DUAL_CORE
enum { ED_CORE1_TEST = 79 };                         /* test builds only: CPU1 failure injection */
static void core1_test_stall(void *unused) { (void)unused; for (;;) ; }
#endif

static uint8_t ed_out[1024];                        /* the longest: a NAMES page (PROPHET pages its 201) */
static uint32_t ed_n;

static void ed_begin(uint32_t cmd)
{
    ed_out[0] = 0xF0;
    ed_out[1] = ED_HDR0;
    ed_out[2] = ED_HDR1;
    ed_out[3] = ED_HDR2;
    ed_out[4] = (uint8_t)cmd;
    ed_n = 5;
}
static void ed_b(uint32_t v)
{
    if (ed_n < sizeof ed_out - 1u)
        ed_out[ed_n++] = (uint8_t)(v & 0x7Fu);
}
static void ed_v(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    ed_b(u);
    ed_b(u >> 7);
}
static void ed_str(const char *s, uint32_t max)   /* ASCII, 0-terminated */
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static void ed_send(void)
{
    ed_out[ed_n++] = 0xF7;
    ota_wire_send(ed_out, ed_n);
}
static int32_t ed_rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }

/* 7-bit-safe payload decoding for project/archive transfers. */
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;                                 /* groups: msb byte, then up to 7 bytes */
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;                              /* incomplete group, overflow, or unused mask bits */
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
/* the engine byte of DUMP / RELOAD / TRACK */
static uint32_t ed_eng(const track_t *t) { return t->eng_req % NENGINES; }

/* ---- live sync (v2): while the editor WATCHes, device-side changes are pushed.
 * Shadows of the selected track's p[] + song.g[] and of its steps are kept in step
 * with what the editor knows (its own SET / STEP_SET update them); the main loop
 * compares and pushes CHANGED / STEP_CHANGED, or RELOAD after a load or when another
 * track was selected (v3: RELOAD and STEP_CHANGED carry the selected track). Pushes
 * go out only into a half-empty SysEx ring, so they never wait. */
#define ED_PUSH_MAX 4u                                   /* frames per pass */
#define ED_NV (P_COUNT + G_COUNT)
/* v4: the parameters of the tracks that are not selected which TRACK_CHANGED follows (the mixer) */
static const uint8_t ED_TIDS[3] = {P_LEVEL, P_PAN, P_MUTE};
#define ED_NT (NTRK * 3u)
static struct {
    uint8_t on, eng, preset, sel;
    uint8_t v4;                                          /* WATCH bit 1: TRACK_CHANGED pushes too */
    uint32_t pos, last_ms, run_ms, resets;
    int16_t v[ED_NV];                                    /* TSEL->p[], then song.g[] */
    uint16_t t[ED_NV];                                   /* ms (low 16 bits) of the last push */
    int16_t tv[ED_NT];                                   /* v4: trk[k].p[ED_TIDS[j]] at k * 3 + j */
    uint16_t tt[ED_NT];
    uint32_t tpos;
} ed_w;

static int16_t *ed_val(uint32_t i)
{
    return i < P_COUNT ? &TSEL->p[i] : i == P_COUNT + G_A4 ? &tuning_a4 : &song.g[i - P_COUNT];
}
static void ed_shadow(void)                              /* the editor is in sync */
{
    uint32_t i;
    for (i = 0; i < ED_NV; i++)
        ed_w.v[i] = *ed_val(i);
    for (i = 0; i < ED_NT; i++)
        ed_w.tv[i] = trk[i / 3u].p[ED_TIDS[i % 3u]];
    ed_w.eng = (uint8_t)ed_eng(TSEL);
    ed_w.preset = TSEL->preset;
    ed_w.sel = song.sel;
    sync_reload = 0;
}
static int ed_room(void) { return so_w - so_r + 8u <= SXQ / 2u; }
static void ed_known(uint32_t k, uint32_t id)            /* the editor's own change of trk[k].p[id]: no push */
{
    uint32_t j;
    if (k == song.sel) {
        ed_w.v[id] = trk[k].p[id];
        return;
    }
    for (j = 0; j < 3u; j++)
        if (ED_TIDS[j] == id)
            ed_w.tv[k * 3u + j] = trk[k].p[id];
}

static void ed_sync(void)                                /* main loop */
{
    uint32_t i, n = 0, now = fm1_ms;
    if (!ed_w.on || now - ed_w.run_ms < 5u)
        return;
    ed_w.run_ms = now;
    if (!usb.config || usb.resets != ed_w.resets || now - ed_w.last_ms > 3000u) {
        ed_w.on = 0;                                     /* no host, USB reset, or 3 s without a request */
        return;
    }
    if (sync_reload || ed_eng(TSEL) != ed_w.eng || TSEL->preset != ed_w.preset || song.sel != ed_w.sel) {
        if (!ed_room())
            return;
        ed_shadow();
        ed_begin(ED_RELOAD);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        ed_b(song.sel);
        ed_b(TSEL->preset >> 7);                     /* presets past 127 (PROPHET): high bits last */
        ed_send();
        return;
    }
    for (i = 0; i < ED_NV && n < ED_PUSH_MAX; i++) {    /* round robin: nothing starves */
        uint32_t k = (ed_w.pos + i) % ED_NV;
        int16_t v = *ed_val(k);
        if (v == ed_w.v[k] || (uint16_t)(now - ed_w.t[k]) < 20u)
            continue;                                    /* coalesced: the latest value goes out later */
        if (!ed_room())
            return;
        ed_w.v[k] = v;
        ed_w.t[k] = (uint16_t)now;
        ed_begin(ED_CHANGED);
        ed_b(k < P_COUNT ? 0u : 1u);
        ed_b(k < P_COUNT ? k : k - P_COUNT);
        ed_v(v);
        ed_send();
        n++;
        ed_w.pos = k + 1u;
    }
    for (i = 0; ed_w.v4 && i < ED_NT && n < ED_PUSH_MAX; i++) {   /* v4: the other tracks' mix */
        uint32_t k = (ed_w.tpos + i) % ED_NT, tr = k / 3u, id = ED_TIDS[k % 3u];
        int16_t v = trk[tr].p[id];
        if (tr == song.sel || v == ed_w.tv[k] || (uint16_t)(now - ed_w.tt[k]) < 20u)
            continue;                                    /* (the selected track: CHANGED above) */
        if (!ed_room())
            return;
        ed_w.tv[k] = v;
        ed_w.tt[k] = (uint16_t)now;
        ed_begin(ED_TRACK_CHANGED);
        ed_b(tr);
        ed_b(id);
        ed_v(v);
        ed_send();
        n++;
        ed_w.tpos = k + 1u;
    }
}

/* descriptor of parameter id of track t: the engine parameters of the engine it asked for
 * (t->engine follows in the audio ISR, after a short fade) */
static const param_desc_t *ed_tdesc(const track_t *t, uint32_t id) { return param_desc_of(t->eng_req % NENGINES, id); }
/* descriptor and value slot of (scope, id): scope 0 = the selected track, 1 = global */
static const param_desc_t *ed_desc(uint32_t scope, uint32_t id, int16_t **vp)
{
    if (scope == 0 && id < P_COUNT) {
        *vp = &TSEL->p[id];
        return ed_tdesc(TSEL, id);
    }
    if (scope == 1 && id < G_COUNT) {
        *vp = ed_val(P_COUNT + id);
        return &GP[id];
    }
    return 0;
}

/* a flash write from the editor while the external transport runs: refused (the erase would stall the audio it
 * times; the device's own SAVE says "STOP TO SAVE") */
static int ed_flash_stop(void) { return transport_busy(); }

#include "editor_preferences.c"
#include "editor_backup.c"
#include "editor_fm6.c"
#include "editor_cz.c"
#include "editor_native.c"
#include "editor_library.c"
#include "editor_prophet.c"
#include "editor_j106.c"
#include "editor_hydra.c"

static int ed_args_ok(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    switch (cmd) {
    case ED_STEP_GET: case ED_STEP_SET: case ED_PROJECT: case ED_TRACK_STEP: case ED_MOTION:
    case ED_PATTERN: case ED_BANK_SONG: case ED_SONG:
        return 0;                                  /* (no sequencer, no projects: no reply) */
    case ED_INFO: case ED_DUMP: case ED_SMP_INFO: case ED_PING:
    case ED_UI_STATE: case ED_UI_PALETTES:
        return !n;
    case ED_GET: case ED_DESC: case ED_PRESET: case ED_UP_LIST:
    case ED_TRACK_PARAM:
        return n == 2u || (cmd == ED_TRACK_PARAM && n == 4u) || (cmd == ED_PRESET && n == 3u);
    case ED_SET:
        return n == 4u;
    case ED_NAMES: case ED_SMP_BEGIN: case ED_SMP_ERASE:
    case ED_UP_GET: case ED_UP_LOAD: case ED_UP_ERASE: case ED_WATCH: case ED_TRACK_DUMP:
        return n == 1u || (cmd == ED_NAMES && n == 3u);
    case ED_TRACK:
        return n <= 1u;
    case ED_TRACK_MIX:
        return n == 1u || n == 4u;
    default:
        return 1;                                  /* variable payloads validate in their handler */
    }
}

/* minsize: the largest function in flash, main loop only and in no hurry; -O2 cost it 4 KB the FM-1 BLE image does not have */
static __attribute__((minsize)) void ed_handle(const uint8_t *f, uint32_t n)   /* f: the bytes between F0 and F7 */
{
    if (n < 4u)
        return;
    uint32_t cmd = f[3], i;
    const uint8_t *a = f + 4;
    uint32_t na = n - 4u;
    int16_t *vp;
    const param_desc_t *d;
    if (!ed_args_ok(cmd, a, na))
        return;
    ed_begin(cmd);
    if(ed_prophet_handle(cmd,a,na)){ed_send();return;}
    if(ed_j106_handle(cmd,a,na)){ed_send();return;}
    if(ed_hydra_handle(cmd,a,na)){ed_send();return;}

    if (ed_ui_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_library_handle(cmd,a,na)) { ed_send(); return; }
    if (ed_backup_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_fm6_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_cz_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_native_handle(cmd, a, na)) { ed_send(); return; }
    switch (cmd) {
#if MELODEE_USB_AUDIO || defined(SAADET_FM1_BASE)
    case ED_AUDIO_STATS: {                         /* flags: 1 resets maxima, 2 voices, 4 cores, 8 memory, 16 part victims */
        uint32_t snapshot[55] = {0}, k, count = na && (a[0] & 16u) ? 55u : na && (a[0] & 8u) ? 47u : na && (a[0] & 4u) ? 32u : na && (a[0] & 2u) ? 26u : 20u;
        fm1_irq_off();
#if MELODEE_USB_AUDIO
        snapshot[0] = ua.play_alt;
        snapshot[1] = ua.cap_alt;
        snapshot[2] = UA_RATE;
        snapshot[3] = UA_RATE;
        snapshot[4] = ua.pw - ua.pr;
        snapshot[5] = ua.cw - ua.cr;
        snapshot[6] = ua.play_underruns;
        snapshot[7] = ua.play_overruns;
        snapshot[8] = ua.cap_underruns;
        snapshot[9] = ua.cap_overruns;
        snapshot[10] = ua.bad_packets;
        snapshot[11] = ua.rx_packets;
        snapshot[12] = ua.tx_packets;
        snapshot[13] = ua.missed_frames;
        snapshot[14] = ua.poll_max_ticks / FM1_TICKS_PER_US;
        snapshot[15] = ua.service_max_ticks / FM1_TICKS_PER_US;
        snapshot[16] = melodee_dbg.late;
        snapshot[17] = ua_feedback();
        snapshot[18] = melodee_dbg.max_us;            /* render time excludes TIMER5; shedding includes it */
#else   /* fm1-base owns USB audio (0..15, 17 stay 0): its missed renders and the render time it measures */
        snapshot[16] = sd_xruns();
        snapshot[18] = sd_render_max_us;
#endif
        snapshot[19] = song.cpu_q8;
        if (count >= 26u) {
            for (uint32_t p = 0; p < NPART; p++) {
                for (uint32_t v = 0; v < NVOICE; v++) {
                    snapshot[20] += trk[p].v[v].active && trk[p].v[v].stage != 4u;
                    snapshot[21] += trk[p].v[v].active && trk[p].v[v].gate && trk[p].v[v].stage != 4u;
                }

            }
            snapshot[24] = shed_count;
            snapshot[25] = voice_kills;
        }
#if MELODEE_DUAL_CORE
        if (count >= 32u) {
            snapshot[26] = audio_worker_online;
            snapshot[27] = audio_worker.jobs;
            snapshot[28] = audio_worker.max_job_ticks / FM1_TICKS_PER_US;
            snapshot[29] = audio_worker.max_wait_ticks / FM1_TICKS_PER_US;
            snapshot[30] = audio_worker.timeouts;
            snapshot[31] = fm6_pairs;
            snapshot[22] = audio_worker.rejected;    /* (22, 23: OBXF's, retired) misread requests, */
            snapshot[23] = fm1_core1_fault.count;     /* CPU1 faults: either retires the worker */
        }
#endif
        if (count >= 47u) {
            snapshot[32] = fm1_cache.status;
            snapshot[33] = fm1_cache.con_before;
            snapshot[34] = fm1_cache.data_before;
            snapshot[35] = fm1_cache.instruction_before;
            snapshot[36] = fm1_cache.con_after;
            snapshot[37] = fm1_cache.data_after;
            snapshot[38] = fm1_cache.instruction_after;
            snapshot[39] = fm1_cache.failure_address;
            snapshot[40] = fm1_cache.test_ticks / FM1_TICKS_PER_US;
            snapshot[41] = RESOURCE_CAPACITY;
            snapshot[42] = resource_used();
            snapshot[43] = resource_peak;
            snapshot[44] = resource_failures;
            snapshot[45] = RESOURCE_CACHE_AVAILABLE ? 28672u : 0u;
            for (uint32_t r = 0; r < RES_COUNT; r++)
                if ((uintptr_t)resource[r].ptr >= (uintptr_t)_resource_cache_start &&
                    (uintptr_t)resource[r].ptr < (uintptr_t)_resource_cache_end)
                    snapshot[46] += resource[r].size;
        }
        if (count == 55u)
            for (uint32_t p = 0; p < NPART; p++) {
                snapshot[47u+p] = shed_part[p];
                snapshot[51u+p] = voice_kills_part[p];
            }
        if (na && (a[0] & 1u)) {
#if MELODEE_USB_AUDIO
            ua.poll_max_ticks = ua.service_max_ticks = melodee_dbg.max_us = 0;
#else
            sd_render_max_us = 0;
#endif
#if MELODEE_DUAL_CORE
            audio_worker.max_job_ticks = audio_worker.max_wait_ticks = 0;
#endif
        }
        fm1_irq_on();
        ed_b(count == 55u ? 6u : count == 47u ? 5u : count == 32u ? 4u : count == 26u ? 3u : 2u);
        for (i = 0; i < count; i++)
            for (k = 0; k < 5u; k++)
                ed_b(snapshot[i] >> (7u * k));
        break;
    }
#endif
#if MELODEE_CORE1_TEST && MELODEE_DUAL_CORE
    case ED_CORE1_TEST: {        /* mode: 0 report, 1 misread request, 2 null job, 3 stalled job, 4 reboot */
        uint32_t mode = na ? a[0] : 0u, rc = 0, waited = 0, v[16];
        if (mode == 4u) {                           /* (a[1] 1: keep fm1_core1_bar, else clear it as power-off does) */
            reset_intended();
            if (!(na > 1u && a[1]))
                fm1_core1_bar.magic = 0;
            fm1_reboot();
        }
        if (mode == 10u) {                          /* a CPU0 crash (the master_out report): fetch from 0 */
            void (*volatile crash)(void) = 0;
            crash();
        }
        if (mode == 9u) {                           /* 9: raw P33 0x00..0x3F, then 0x72 0x74 0x90 0x92 0x9B, 7 bits + 1 */
            static const uint8_t extra[5] = { 0x72, 0x74, 0x90, 0x92, 0x9B };
            uint8_t r[69];
            fm1_irq_off();
            for (i = 0; i < 64u; i++)
                r[i] = fm1_p33_read(i);
            for (i = 0; i < 5u; i++)
                r[64 + i] = fm1_p33_read(extra[i]);
            fm1_irq_on();
            ed_b(mode); ed_b(rc);
            for (i = 0; i < 69u; i++) {
                ed_b(r[i] & 0x7Fu);
                ed_b(r[i] >> 7);
            }
            break;
        }
        if (mode == 5u || mode == 7u || mode == 8u) {   /* 5 rails + clock (read only), 7 count misreads only */
            fm1_irq_off();                          /* (a[1]), 8 hold (a[1]) / release eight notes on track 0 */
            if (mode == 8u)
                for (i = 0; i < 8u; i++) {
                    if (na > 1u && a[1])
                        trk_note_on(&trk[0], 41u + 3u * i, 100u);
                    else
                        trk_note_off(&trk[0], 41u + 3u * i);
                }
            if (mode == 7u)
                core1_test_count_only = na > 1u && a[1];
            for (i = 0; i < FM1_RAILS; i++)
                v[i] = fm1_power_get(i);
            v[3] = (uint32_t)fm1_core1_barred(); v[4] = bootguard.failed;
            v[5] = (uint32_t)fm1_clock_undo;   /* 1: the boot retune timed out or met an unknown clock: left there */
            fm1_clock_regs(v + 6);
            v[11] = audio_worker.misread; v[12] = fm1_core1_fault.count;
            v[13] = fm1_cpu_ticks();
            fm1_irq_on();
            v[14] = audio_worker.rejected; v[15] = (uint32_t)audio_worker_online;
            ed_b(mode); ed_b(rc);
            for (i = 0; i < 16u; i++)
                for (uint32_t k = 0; k < 5u; k++)
                    ed_b(v[i] >> (7u * k));
            break;
        }
        if (mode >= 1u && mode <= 3u && !audio_worker_online)
            rc = 1;                                 /* no worker (any more) */
        else if (mode >= 1u && mode <= 3u) {
            uint32_t t0 = fm1_ticks();
            fm1_irq_off();                          /* the audio ISR neither submits nor checks meanwhile */
            if (mode == 1u) {                       /* what an affected unit's CPU1 reads (#17) */
                uint32_t r = fm1_aw_load(&audio_worker.request);
                fm1_aw_store(&audio_worker.request, r + 0x00200000u);
                while (fm1_ticks() - t0 < 1000u * FM1_TICKS_PER_US)
                    ;
                fm1_aw_store(&audio_worker.request, r);
                audio_worker_check();
            } else {                                /* a fetch from 0 (#18's idle crash) or a hung job */
                audio_worker_submit(mode == 2u ? (audio_worker_fn)0 : core1_test_stall, 0);
                rc = audio_worker_join() ? 2u : 0u; /* 2: it "completed" */
            }
            waited = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
            fm1_irq_on();                           /* (a masked exception would be taken here) */
        }
        v[0] = (uint32_t)audio_worker_online; v[1] = audio_worker.rejected; v[2] = fm1_core1_fault.count;
        v[3] = fm1_core1_fault.cpu; v[4] = fm1_core1_fault.dbg; v[5] = fm1_core1_fault.pc;
        v[6] = fm1_core1_fault.rets; v[7] = audio_worker.timeouts; v[8] = melodee_dbg.halves; v[9] = waited;
        v[10] = fm1_core1_fault.emu;
        ed_b(mode); ed_b(rc);
        for (i = 0; i < 11u; i++)
            for (uint32_t k = 0; k < 5u; k++)
                ed_b(v[i] >> (7u * k));
        break;
    }
#endif
    case ED_INFO:
        ed_str("SAADET " MELODEE_VERSION, 24);
        ed_b(NENGINES);
        ed_b(P_COUNT);
        ed_b(G_COUNT);
        ed_b(0);                                          /* NSTEP: no steps in the synth build */
        ed_b(P_E0);
        for (i = 0; i < NENGINES; i++)
            ed_str(ENGINES[i]->name, 8);
        ed_b(NTRK);                                       /* v3 */
        ed_b(0);                                          /* project-based SONG retired: tagged bank SONG below */
        ed_b(0x55); ed_b(1); ed_b(ed_ui_caps());             /* tagged preferences v1: commands 34..38 */
        ed_b(0x42); ed_b(1); ed_b(3); /* bounded full-backup read + restore */
        ed_b(0x53); ed_b(1);                              /* the synth build: no sequencer, no projects */
        ed_b(0x46); ed_b(1); ed_b(FM6_NFAC); ed_b(0);   /* FM6 patches: cmds 68..71 */
        ed_b(0x43); ed_b(1); ed_b(CZ_BYTES & 127u); ed_b(CZ_BYTES >> 7); ed_b(0); ed_b(0); /* old CZ bank controls retired */
        ed_b(0x35);ed_b(1);ed_b(ENGI_PROPHET);ed_b(USER_GENERAL);ed_b(USER_NATIVE_FM);ed_b(USER_NATIVE_CZ);ed_b(USER_NATIVE_P5);ed_b(0);ed_b(1); /* Prophet/native stable namespaces */
        ed_b(0x4e); ed_b(1); ed_b(NATIVE_FM_SLOTS); ed_b(0); ed_b(NATIVE_CZ_SLOTS&127u); ed_b(NATIVE_CZ_SLOTS>>7); /* native user pools, command 78 */
        ed_b(0x55);ed_b(1);ed_b(UP_SLOTS&127u);ed_b(UP_SLOTS>>7); /* shared library */
        ed_b(0x4a);ed_b(1);ed_b(ENGI_J106);ed_b(USER_NATIVE_J106);ed_b(ED_J106);ed_b(J106_FIELDS); /* JUNO-106: command 96 */
        ed_b(0x48);ed_b(1);ed_b(ENGI_HYDRA);ed_b(USER_NATIVE_HYDRA);ed_b(ED_HYDRA);ed_b(HY_BYTES&127u);ed_b(HY_BYTES>>7); /* HYDRA: command 97 */
        break;
    case ED_GET:
    case ED_SET:
        if (na < 2u || !(d = ed_desc(a[0], a[1], &vp)))
            return;
        if (cmd == ED_SET && na >= 4u) {
            if (a[0] == 1 && a[1] == G_ENGSEL) {          /* engine change: the safe path */
                set_engine((uint32_t)clamp(ed_rv(a + 2), 0, NENGINES - 1));
            } else if (d->max > d->min) {
                *vp = (int16_t)enum_orig(d, clamp(ed_rv(a + 2), d->min, d->max));
                if (a[0] == 1 && a[1] == G_A4)
                    settings_save();
                if (a[0] == 0 && scale_shared(a[1]))
                    scale_share(TSEL);
            }
            ui.force = 1;
            ed_w.v[a[0] ? P_COUNT + a[1] : a[1]] = *vp;   /* the editor's own change: no push */
        }
        ed_b(a[0]);
        ed_b(a[1]);
        ed_v(*vp);
        break;
    case ED_DUMP:
        for (i = 0; i < ED_NV; i++)                       /* the editor gets them all here */
            ed_w.v[i] = *ed_val(i);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(TSEL->p[i]);
        for (i = 0; i < G_COUNT; i++)
            ed_v(*ed_val(P_COUNT + i));
        ed_b(TSEL->preset >> 7);
        break;
    case ED_DESC:
        if (na < 2u || !(d = ed_desc(a[0], a[1], &vp)))
            return;
        ed_b(a[0]);
        ed_b(a[1]);
        ed_b(d->fmt);
        ed_v(d->min);
        ed_v(d->max);
        ed_v(d->def);
        ed_str(d->label, 8);
        ed_str(d->unit, 8);
        if (d->fmt == F_ENUM && d->names)                  /* the full SCL catalogue fits the 1024-byte reply */
            for (i = 0; i <= (uint32_t)(d->max - d->min) && i < 96u; i++)
                ed_str(d->names[i+d->min], 8);
        break;
    case ED_PRESET:                                        /* engine, preset */
        if (na < 2u || a[0] >= NENGINES)
            return;
        {
            if (a[0] != TSEL->eng_req)
                set_engine(a[0]);
            apply_preset(a[1] | (na >= 3u ? (uint32_t)a[2] << 7 : 0u));   /* [, high bits]: PROPHET has 201 */
        }
        ui.force = 1;
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        ed_b(TSEL->preset >> 7);
        break;
    case ED_NAMES: {                                       /* engine [, first lo, hi] -> preset names of an engine */
        const engine_t *en;
        uint32_t s0, n = 0, room;
        if (na < 1u || a[0] >= NENGINES)
            return;
        en = ENGINES[a[0]];
        s0 = na >= 3u ? a[1] | (uint32_t)a[2] << 7 : 0u;
        /* the names that fit (at most 127): engine, count, names, titles (2 x 9), total and first (4), F7 */
        room = sizeof ed_out - ed_n - 2u - 18u - 4u - 1u;
        for (i = s0; i < en->npresets && n < 127u; i++, n++) {
            uint32_t len = 0;
            const char *nm = eng_preset(en, i)->name;
            while (len < 20u && nm[len])
                len++;
            if (len + 1u > room)
                break;
            room -= len + 1u;
        }
        ed_b(a[0]);
        ed_b(n);
        for (i = 0; i < n; i++)
            ed_str(eng_preset(en, s0 + i)->name, 20);
        for (i = 0; i < 2u; i++)                           /* then the two edit-page titles */
            ed_str(en->page_title[i], 8);
        ed_b(en->npresets);                                /* then all presets and this page's first: the */
        ed_b(en->npresets >> 7);                           /* editor asks again from s0 + n until it has them */
        ed_b(s0);
        ed_b(s0 >> 7);
        break;
    }
    case ED_SMP_BEGIN: case ED_SMP_WRITE: case ED_SMP_END: case ED_SMP_ERASE:
        return;                                           /* retired: never write pattern storage */
    case ED_SMP_INFO:
        ed_b(0); ed_b(0);                                  /* no sample slots/material */
        break;
    case ED_UP_LIST: {                                     /* start, count -> start, count, total, per slot: used, engine, name */
        uint32_t s0, cnt;
        if (na < 2u)
            return;
        s0 = a[0];
        cnt = a[1] > 16u ? 16u : a[1];
        if (s0 >= 128u)
            cnt = 0;
        else if (s0 + cnt > 128u)
            cnt = 128u - s0;
        ed_b(s0);
        ed_b(cnt);
        ed_b(128u);
        for (i = s0; i < s0 + cnt; i++) {
            char nm[13] = {0};
            uint32_t j, u = (uint32_t)up_used(i);
            for (j = 0; u && j < 12u; j++)
                nm[j] = up_rec(i)->name[j];
            ed_b(u);
            ed_b(u ? up_rec(i)->engine : 0u);
            ed_str(nm, 12);
        }
        break;
    }
    case ED_UP_GET: {                                      /* slot -> slot, used, engine, name, P_COUNT x v14, 16 x (note,
                                                            * flags), (v5) kind [, 16 x hi] (upreset.c up_parse) */
        int16_t v[P_COUNT];
        char nm[13] = {0};
        const up_rec_t *r;
        uint32_t u;
        if (na < 1u || a[0] >= 128u)
            return;
        r = up_rec(a[0]);
        u = (uint32_t)up_used(a[0]);
        if (u)
            up_values(r, v);                               /* today's P_* order, clamped */
        for (i = 0; u && i < 12u; i++)
            nm[i] = r->name[i];
        ed_b(a[0]);
        ed_b(u);
        ed_b(u ? up_engine(a[0]) : 0u);
        ed_str(nm, 12);
        for (i = 0; i < P_COUNT; i++)
            ed_v(u ? v[i] : 0);
        for (i = 0; i < 16u; i++) {
            ed_b(u ? r->note[i] & 0x7Fu : 0u);
            ed_b(u ? r->flags[i] & 0x7Fu : 0u);
        }
        ed_b(u && up_grid(r));                  /* kind: 1 = a drum grid, then bit 7 of each step's */
        for (i = 0; u && up_grid(r) && i < 16u; i++)   /* hits (bit 0) and accents (bit 1) */
            ed_b((uint32_t)(r->note[i] >> 7) | (uint32_t)(r->flags[i] >> 7) << 1);
        break;
    }
    case ED_UP_PUT: {                                      /* slot, engine, name, values, pattern -> slot, rc */
        static up_rec_t r;
        uint32_t slot = 0, rc;
        if (na < 1u)
            return;
        rc = (uint32_t)up_parse(a, na, &r, &slot);
        if (!rc && ed_flash_stop())
            rc = 2;
        if (!rc) {
            int16_t v[P_COUNT];
            up_values(&r, v);                              /* each value inside its range */
            for (i = 0; i < P_COUNT; i++)
                up_set_value(&r, i, v[i]);                 /* the record's own form (v4: a byte each) */
            rc = up_put(slot, &r) ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_STORE: {                                    /* slot, name -> slot, rc */
        char nm[13] = {0};
        uint32_t n0, rc = 1;
        if (na < 1u)
            return;
        for (n0 = 0; 1u + n0 < na && a[1 + n0] && n0 < 13u; n0++)
            ;
        if (a[0] < 128u && (!n0 || up_name_ok(a + 1, n0)) &&
            (1u + n0 == na || (2u + n0 == na && !a[1u + n0]))) {   /* "" = automatic name */
            for (i = 0; i < n0; i++)
                nm[i] = (char)a[1 + i];
            int r;
            r = ed_flash_stop() ? 2 : up_store(a[0], nm);
            rc = r ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_LOAD:                                       /* slot -> slot, rc */
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] < 128u && !up_load(a[0]) ? 0u : 1u);
        break;
    case ED_UP_ERASE:
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] >= 128u ? 1u : ed_flash_stop() || up_put(a[0], 0) ? 2u : 0u);
        break;
    case ED_WATCH:                                         /* on -> on */
        if (na < 1u)
            return;
        ed_w.on = a[0] & 1u;
        ed_w.v4 = (uint8_t)(ed_w.on && (a[0] & 2u));     /* v4: also TRACK_CHANGED; the reply says it is known */
        ed_w.resets = usb.resets;
        if (ed_w.on)
            ed_shadow();
        ed_b(ed_w.on | ed_w.v4 << 1);
        break;
    case ED_PING:
        ed_b(0);
        break;
    case ED_TRACK:                                         /* [track] -> selected, NTRK, per track: engine, preset, level, mute, armed */
        if (na >= 1u && a[0] < NTRK && a[0] != song.sel) {
            track_select(a[0]);
            if (ed_w.on)
                ed_shadow();                               /* the editor re-reads it: no RELOAD for this */
        }
        ed_b(song.sel);
        ed_b(NTRK);
        for (i = 0; i < NTRK; i++) {
            ed_b(ed_eng(&trk[i]));
            ed_b(trk[i].preset);
            ed_v(trk[i].p[P_LEVEL]);
            ed_b(trk[i].p[P_MUTE] != 0);
            ed_b(0);                                      /* (armed: never) */
        }
        for (i = 0; i < NTRK; i++)                         /* the presets' high bits, last */
            ed_b(trk[i].preset >> 7);
        break;
    case ED_TRACK_MIX: {                                   /* track [, level v14, mute] -> track, level, mute */
        track_t *t;
        int16_t *lv;
        if (na < 1u || a[0] >= NTRK)
            return;
        t = &trk[a[0]];
        lv = &t->p[P_LEVEL];
        if (na >= 4u) {
            *lv = (int16_t)clamp(ed_rv(a + 1), 0, 127);
            t->p[P_MUTE] = (int16_t)(a[3] ? 1 : 0);
            ed_known(a[0], P_LEVEL);                       /* the editor's own change: no push */
            ed_known(a[0], P_MUTE);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_v(*lv);
        ed_b(t->p[P_MUTE] != 0);
        break;
    }
    case ED_TRACK_DUMP:                                    /* track -> track, engine, preset, P_COUNT x v14 */
        if (na < 1u || a[0] >= NTRK)
            return;
        ed_b(a[0]);
        ed_b(ed_eng(&trk[a[0]]));
        ed_b(trk[a[0]].preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(trk[a[0]].p[i]);
        ed_b(trk[a[0]].preset >> 7);
        break;
    case ED_TRACK_PARAM: {                                 /* track, id [, v14] -> track, id, v14 */
        track_t *t;
        if (na < 2u || a[0] >= NTRK || a[1] >= P_COUNT)
            return;
        t = &trk[a[0]];
        d = ed_tdesc(t, a[1]);
        if (na >= 4u) {
            if (d->max > d->min)                           /* as SET: clamped; a fixed value stays */
                t->p[a[1]] = (int16_t)enum_orig(d, clamp(ed_rv(a + 2), d->min, d->max));
            if (scale_shared(a[1]))
                scale_share(t);
            ed_known(a[0], a[1]);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_b(a[1]);
        ed_v(t->p[a[1]]);
        break;
    }
    default:
        return;
    }
    ed_send();
}

/* main loop: editor frames first; anything else stays for ota_service() */
static void ed_service(void)
{
    const uint8_t *p;
    uint32_t n;
    ed_sync();                                             /* v2 pushes (while watched) */
    if(!ota_frame_get(&p,&n))return;
    if(general_sysex_frame(p,n)){ota_frame_done();return;}
    if(p5_native_frame(p,n)){ota_frame_done();return;}
    if(j106_native_frame(p,n)){ota_frame_done();return;}
    if(hy_native_frame(p,n)){ota_frame_done();return;}
    if (n < 4u || p[0] != ED_HDR0 || p[1] != ED_HDR1 || p[2] != ED_HDR2)
        return;
    ed_w.last_ms = fm1_ms;                                 /* any request keeps WATCH alive */
    if (p[3] >= ED_SMP_BEGIN) {                            /* large frames: handled in place, then freed */
        ed_handle(p, n);
        ota_frame_done();
        return;
    }
    {
        static uint8_t f[64];
        uint32_t k = n > sizeof f ? sizeof f : n, i;
        for (i = 0; i < k; i++)
            f[i] = p[i];
        ota_frame_done();                                  /* free the frame buffer before replying */
        ed_handle(f, k);
    }
}
