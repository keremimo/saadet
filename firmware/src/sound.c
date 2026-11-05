/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The selected part's sound, apart from any interface: an engine or a preset loaded into it, the parameters a load
 * keeps, the scale every part shares, and the last state saved once the instrument rests. The editor, MIDI and the
 * stores use these. */
static int state_autosave(void);                 /* persist.c */
static int up_used(uint32_t k);                  /* user presets: upreset.c */
static uint32_t up_engine(uint32_t k);
static int up_favorite(uint32_t k);
static int up_favorite_set(uint32_t k, int on);

static uint8_t sync_reload;                      /* engine / preset / user preset loaded: editor RELOAD push */
static uint32_t up_gen;                          /* bumped on every user bank change */

static uint32_t user_of(const track_t *t)        /* user preset slot its sound came from, USER_NONE = none */
{
    return t->user && !t->user_native && up_used(t->user-1u) && up_engine(t->user-1u)==t->eng_req ? t->user-1u : USER_NONE;
}
#include "favorites.c"

/* Main loop only: the external transport runs (a flash write would stall the audio it times) */
static int transport_busy(void) { return song.playing; }

/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump) leaves alone.
 * The mix (LEVEL, PAN, MUTE), the arpeggiator, the scale and key map, the retired pattern parameters (LEN, DIV,
 * SWING, GATE) */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           i == P_CHRD || i == P_VOIC || i == P_STRUM || i == P_STROKE || i == P_MPCDEG || i == P_RECQ;
}

/* SCL, QNT and the MPC degree are the song's: one scale for every part, kept the same in each track's own slots
 * (the formats and the editor keep them per track). ROOT and TRN stay the track's */
static int scale_shared(uint32_t id) { return id == P_SCALE || id == P_QUANT || id == P_MPCDEG; }
static void scale_share(const track_t *from)
{
    uint32_t k;
    for (k = 0; k < NPART; k++) {
        trk[k].p[P_SCALE] = from->p[P_SCALE];
        trk[k].p[P_QUANT] = from->p[P_QUANT];
        trk[k].p[P_MPCDEG] = (int16_t)clamp(from->p[P_MPCDEG], 1, (int32_t)scale_count(from));
    }
}

/* preset pi of the engine the track asked for: the sound only (not param_kept) */
static void apply_preset_to(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i;
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0; t->user_native=0;
    if (t == TSEL)
        sync_reload = 1;
    if (!e->npresets)
        return;
    pi %= e->npresets;
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)                        /* the rest of the sound to its defaults: a preset */
        if (!param_kept(i))                           /* sounds the same after any edit */
            t->p[i] = TP[i].def;
    const preset_t *pr = eng_preset(e, pi);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;   /* mono presets keep the legato feel */
    if(t->eng_req==ENGI_PROPHET)p5_preset_loaded(t,pi);
    if(t->eng_req==ENGI_J106)j106_preset_loaded(t,pi);
    cz_factory_loaded(t);
    cz_track_accept(t);
    fm6_track_loaded(t);                              /* FM6: the preset's patch */
}

/* the engine's defaults and its preset pi. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's */
static void engine_load_of(track_t *t, uint32_t ei, uint32_t pi)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    fm1_irq_off();
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, pi);
    fm1_irq_on();
}
/* the engine's defaults and its first preset (PROPHET: Sequential's first factory program, JUNO-106: Roland's
 * A11, not INIT) */
static void set_engine_of(track_t *t, uint32_t ei)
{
    engine_load_of(t, ei, ei % NENGINES == ENGI_PROPHET || ei % NENGINES == ENGI_J106 ? 1u : 0u);
}

static void apply_preset(uint32_t pi) { apply_preset_to(TSEL, pi); }
static void set_engine(uint32_t ei) { set_engine_of(TSEL, ei); }

static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
}

/* select part i (the panel, the editor): its sound from now on; with one part, part A stays */
static void track_select(uint32_t i)
{
    if (i >= trk_n() || i == song.sel)
        return;
    song.sel = (uint8_t)i;
    sync_reload = 1;
}

/* ------------------------------------------------------- SysEx dumps --- */
static uint8_t sx_checksum(const uint8_t *p,uint32_t n)
{
    uint32_t sum=0;while(n--)sum+=*p++;return (uint8_t)(-sum&127u);
}
/* An incoming dump (FM6: one voice or a bank of 32, a CZ-1 tone, a Prophet program, a user preset) waits here for the
 * player to choose its user slots: SAVE shows it and a press stores it (ui_lib.c, shared_presets.c sx_commit); a newer
 * dump takes its place, and no user slot is written before that press. Single
 * native tones also audition in RAM. 0: not staged (too large) */
#define SX_STAGE_MAX 4096u
static struct { uint8_t on, arrived, e; uint16_t count, size; uint8_t raw[SX_STAGE_MAX]; } sx_stage;
static int sx_staged(void) { return sx_stage.on; }
static void sx_drop(void) { sx_stage.on = sx_stage.arrived = 0; }
static int sysex_stage(uint32_t e, const uint8_t *raw, uint32_t count, uint32_t size)
{
    if (!count || !size || count * size > SX_STAGE_MAX)
        return 0;
    uint32_t refresh = sx_stage.on && sx_stage.e == e && sx_stage.count == count && sx_stage.size == size;
    memcpy(sx_stage.raw, raw, count * size);
    sx_stage.e = (uint8_t)e;
    sx_stage.count = (uint16_t)count;
    sx_stage.size = (uint16_t)size;
    sx_stage.on = 1;
    if (!refresh) sx_stage.arrived = 1;          /* live updates retain the chosen import destination */
    return 1;
}

/* -------------------------------------------------------- autosave --- */
/* Untouched for AUTOSAVE_MS (no key, button, knob or MIDI) and nothing sounding, the last state into flash when it
 * changed (persist.c state_autosave); once per rest */
#define AUTOSAVE_MS 3000u
static int sounding(void)                               /* a voice of any part, an ARP note, a latched chord */
{
    uint32_t k, i;
    for (k = 0; k < NPART; k++) {
        if (trk[k].arp_note || trk[k].nheld)
            return 1;
        for (i = 0; i < NVOICE; i++)
            if (trk[k].v[i].active)
                return 1;
    }
    return 0;
}
static void autosave_poll(uint32_t active)
{
    static uint32_t t0, rx;
    static uint8_t done;
#if MELODEE_UART
    uint32_t r = usb.rx_pkts + um.bytes;
#else
    uint32_t r = usb.rx_pkts;
#endif
    if (active || r != rx || transport_busy() || sounding()) {
        t0 = fm1_ms;
        rx = r;
        done = 0;
        return;
    }
    if (!done && fm1_ms - t0 >= AUTOSAVE_MS) {
        done = (uint8_t)state_autosave();
        if (!done) t0 = fm1_ms;                 /* retry a failed write after another idle interval */
    }
}
