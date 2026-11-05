/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Saadet: one polyphonic synth part, performance controls and stereo effects.
 * Stored parameter/engine IDs remain compatible with Melodee sound libraries. */
#include <stdint.h>
#include "audio_pcm.h"
#include "performance_options.h"
#include "tuning.h"
#include "audio_profile.h"
#include "wire_bits.h"

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 16                /* voice slots per part (FM6 plays Dexed's 16) */
#define NPOLY 8                  /* the voices of an engine without its own cap (its per-voice state: engines.c eng_state) */
#define NPART 2                  /* synth parts: A and B; B plays only in a multitimbral mode (G_PMODE, trk_n) */
#ifndef VBUDGET
#define VBUDGET (MELODEE_RAM_AUDIO ? 18u : 16u) /* shared weighted voices; RAM profile uses 18 with CPU1 online
                                                 * at 360 MHz (voice.c voice_budget: scaled with a kept 320) */
#endif
#ifndef MELODEE_ADMISSION
#define MELODEE_ADMISSION 0 /* enable only with a calibrated performance contract */
#endif
#define VOICE_BUDGET_DEFAULT (MELODEE_ADMISSION ? 16u : VBUDGET)
#ifndef MELODEE_AUDIO_BENCH
#define MELODEE_AUDIO_BENCH 0 /* laboratory fixed-voice admission bypass; never a release default */
#endif
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
/* SLICE retired. Keep its stored engine number reserved. */
#ifdef MELODEE_SLICE
#undef MELODEE_SLICE
#endif
#define MELODEE_SLICE 0
#ifndef P5_OVERSAMPLE
#define P5_OVERSAMPLE 1 /* prototype: benchmark base rate before enabling optional 2x */
#endif
#ifndef MELODEE_LEGACY_EXTRAS
#define MELODEE_LEGACY_EXTRAS 0 /* reference tests only: retired VOICE, TRIO, WHEEL, PHYS and NOISE;
                                 * PHASE and SID implementations have been removed */
#endif
#define MELODEE_PROPHET 1
#define ENGI_FM6 12u
#define ENGI_PROPHET 19u /* 16..18 reserved: historical preset namespaces */
#define ENGI_J106 21u    /* JUNO-106; 20 is reserved: the Prophet's user namespace */
#define ENGI_HYDRA 25u   /* HYDRA, wave scanning. 22 is reserved: the JUNO-106's user namespace; 23 and 24 stay refused,
                          * as tests/preferences_compat_test.c pins them */
#define USER_GENERAL 16u
#define USER_NATIVE_P5 20u
#define USER_NATIVE_J106 22u
#define USER_NATIVE_HYDRA 26u
#define NENGINES 26               /* 13 SLICE and 14 OBXF reserved; 15 native CZ-1; 20, 22, 23 and 24 reserved */
#define USER_NATIVE_FM 17u
#define USER_NATIVE_CZ 18u
#define USER_NONE 65535u
#define ENGI_ANALOG 0u           /* retired in 1.2: never selectable (eng_ok), its stored sounds play silent */
#define ENGI_DIGITAL 1u          /* retired: never selectable (eng_ok), its stored sounds play silent */
#define NENG_SHOWN (NENGINES - 15u - !MELODEE_SLICE - 5u * !MELODEE_LEGACY_EXTRAS)   /* the engines one can pick: PRESETS, the EDIT
                                                * layer, the editor, in the display order of engines.c ENGINE_ORDER */
#define ENGI_SLICE 13u           /* reserved without MELODEE_SLICE: never selectable (eng_ok) */
#ifdef SMP_USER_SLOTS
#undef SMP_USER_SLOTS
#endif
#define SMP_USER_SLOTS 0u        /* sample material and its upload slots are retired */
#define UP_SLOTS 384u            /* shared user slots: any engine, including native SysEx */
#define PERF_SLOTS 12u           /* performances: named setups of both parts (perf.c) */
#define PERF_NAME 13u
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_OFS, F_FMNOTE, F_FMFRQ,   /* FM6's pages: 0 at the middle of the range, the DX7 break point, an operator's frequency */
    F_TONE,                     /* retired with the inserts' TONE; the id stays (the editor's format numbers) */
    F_KEY                       /* a MIDI note by name and octave: 60 = C4 */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_RECQ,     /* P_RECQ: reserved field now used by end-of-recording QNT */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_RESERVED33, P_RESERVED34, P_RESERVED35, P_RESERVED36,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_RESERVED45, P_RESERVED46, P_RESERVED47, P_RESERVED48,      /* retired effect format padding */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 4 slots of SRC, DST, AMT; */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* new common parameters go just before P_E0 (user presets */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* and projects map by count) */
    P_M4SRC, P_M4DST, P_M4AMT,
    P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL, P_FM1_LEVEL,
    P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL, P_FM2_LEVEL,
    P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL, P_FM3_LEVEL,
    P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL, P_FM4_LEVEL,
    P_CHRD, P_VOIC,                            /* chord keys (chord.c): one key plays a chord; its voicing */
    P_MPCDEG,                                  /* QNT MPC: the scale degree pad 02 of each bank plays, 1 = root */
    P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7, /* DRUM lane levels, 127 = original gain */
    P_LSYNC, P_LTRIG, P_LPOL, P_SPRD, /* Felucca 1.4 common LFO controls and stereo spread */
    P_FXTYPE, P_FXMIX, P_FXRATE, P_FXCHAR, /* the retired inserts, P_FXTYPE..P_FX3TONE: stored layouts keep */
    P_FX2TYPE, P_FX2MIX, P_FX2RATE, P_FX2CHAR, /* their places; nothing reads them (param_reserved) */
    P_FX3TYPE, P_FX3MIX, P_FX3RATE, P_FX3CHAR,
    P_FXSEND, P_FXSEND2, P_FXSEND3,
    P_FXTONE, P_FX2TONE, P_FX3TONE,
    P_STRUM, P_STROKE, /* chord sweep duration and direction; appended for stored-layout compatibility */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_RESERVED4, G_RESERVED5, G_RESERVED6, G_RESERVED7,
    G_RESERVED8, G_RESERVED9, G_RESERVED10, G_RESERVED11,
    G_MIDI, G_DRUMCH, G_ROUTE, G_INFO, /* G_ROUTE: MIDI IN, 0 CH1-4 (channels 1..4 -> parts 1..4), 1 SEL, 2 CH5-8, 3 CH9-12, 4 CH13-16 (seq.c);
                                        * G_DRUMCH (was G_SYNC, unused): a device setting (settings_drumch) */
    G_SLOT, G_BOOT, G_LOAD, G_SAVE,   /* G_BOOT (was G_NAME, unused): a device setting (settings_boot), not song.g */
    G_ENGSEL, G_A4,            /* the editor switches the engine with a SET of G_ENGSEL; G_A4 reuses the unused
                                 * G_ENGGO id, reads tuning_a4 instead of song.g (ids/count stay fixed) */
    G_CLRSEQ, G_INITSND,
    G_RESERVED24, G_RESERVED25, G_RESERVED26, /* retired global format padding */
    G_MFXTYPE, G_MFXMIX, G_MFXRATE, G_MFXCHAR, /* the retired shared inserts, G_MFXTYPE..G_FXMODE and the */
    G_MFX2TYPE, G_MFX2MIX, G_MFX2RATE, G_MFX2CHAR, /* TONEs: kept places, read by nothing (global_reserved) */
    G_MFX3TYPE, G_MFX3MIX, G_MFX3RATE, G_MFX3CHAR,
    G_FXRETURN,
    G_FXMODE,
    G_BUDGET, /* device-wide shared workload allowance; appended, not a song field */
    G_MFXTONE, G_MFX2TONE, G_MFX3TONE,
    G_COUNT
};
#define G_DRLVL G_RESERVED25         /* (their meaning in a project from before 1.0: proj_drums_to_part) */
/* GLO > WEIGHTS: device settings shown on unused ids (params.c page_desc). Never song.g, never in a project
 * or the editor's tables: global_reserved() stays true for them. */
#define G_WP5 G_RESERVED4
#define G_WCZ G_RESERVED5
#define G_WFM6 G_RESERVED6
#define G_WSID G_RESERVED7
#define G_WDRUM G_RESERVED8
#define G_SAFETY G_RESERVED9
#define G_DRREV G_RESERVED26
/* The parts: how the keys and MIDI reach A and B, kept with the state and a performance. G_PMODE: PT_ONE plays part A
 * alone and costs part B nothing; PT_MULTI: a MIDI channel for each, the keys play the selected part; PT_SPLIT: notes
 * below G_PSPLIT play B, the rest A; PT_LAYER: every note plays both. G_PCH2: part B's MIDI channel */
#define G_PMODE G_RESERVED10
#define G_PSPLIT G_RESERVED11
#define G_PCH2 G_RESERVED24
enum { PT_ONE, PT_MULTI, PT_SPLIT, PT_LAYER, PT_N };

static int param_reserved(uint32_t id)
{ return id==P_RECQ || (id>=P_SLEN && id<=P_SGATE) || (id>=P_LN0 && id<=P_LN7) || (id>=P_RESERVED33 && id<=P_RESERVED36) || (id>=P_RESERVED45 && id<=P_RESERVED48) ||
  (id>=P_FXTYPE && id<=P_FX3TONE); }
static int global_reserved(uint32_t id)
{ return (id>=G_SLOT && id<=G_SAVE) || id==G_CLRSEQ || (id>=G_RESERVED4 && id<=G_RESERVED9) || (id>=G_RESERVED25 && id<=G_RESERVED26) ||
  (id>=G_MFXTYPE && id<=G_FXMODE) || (id>=G_MFXTONE && id<=G_MFX3TONE); }

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
/* PHYS MODEL DRUM (MODEL 4, before 1.0) -> the DRUM engine, its E values in place: {MODEL, TUNE, TONE, DECY,
 * SNAP, ACC, KICK 0..127, PERC 0..127} -> {KIT, TUNE, TONE, DECY, SNAP, ACC, KICK 0..1, DRV 0}. 1 = it was one
 * (its engine is ENGI_DRUM now); projects (project.c) and user presets (upreset.c) */
static int drum_from_phys(uint32_t engine, int16_t *e)
{
    if (engine != ENGI_PHYS || e[0] != 4)
        return 0;
    e[0] = 4; /* Retired custom drum variants now play the synthesized 808. */
    e[6] = (int16_t)(e[6] >= 64);                                       /* KICK: PUNCH, ROUND */
    e[7] = 0;
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
enum { Q_OFF, Q_SNAP, Q_WHITE, Q_ALL, Q_MPC };  /* P_QUANT (seq.c kb_map, midi_map); 1 = SNAP, the former ON */
typedef struct {
    uint8_t side; /* stereo spread side, retained while sounding */
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release, 4 given up: fading out */
    uint8_t fade;                /* stage 4: blocks of the fade still to render (voice.c voice_kill) */
    int32_t env;                 /* Q24 */
    int32_t env_out;             /* last control-rate amplitude, Q15 */
    int32_t pitch16, pitch_cur;  /* 1/16 semitone, with glide */
    int32_t gstep;               /* glide TIME mode: 1/16 st per control tick, 0 = RATE mode */
    int32_t fine;                /* unison detune: phase increment * (1 + fine / 4096) */
    int32_t scale_fine;          /* note-on snapshot: scale pitch below 1/16 semitone */
    uint32_t ph[3];
#if MELODEE_LEGACY_EXTRAS
    int32_t s[8];                /* retired VOICE/NOISE also use slots 6 and 7 */
#else
    int32_t s[6];                /* current engines: slots 0..5, including drum gates */
#endif
    uint32_t age;
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    int16_t mrnd;
} voice_t;

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
    int32_t plog;                /* the voice's own pitch offset without TUNE and bend (glide, LFO and ENV pitch, the
                                  * matrix's, unison detune), Q24 octaves: for engines that figure their pitch (FM6) */
} vmod_t;

typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
                                 /* (a sound load never touches the steps; the arp is the track's: a preset does
                                  * not set it) */
} preset_t;

struct track;
typedef struct {                 /* an engine (engines.c ENGINES[]; the eng_*.c files) */
    const char *name;            /* "ANALOG" (PRESETS, the editor) */
    const char *page_title[2];   /* EDIT 1 and EDIT 2 */
    param_desc_t edit[8];        /* P_E0..P_E7 */
    const preset_t *presets;
    uint8_t npresets;
    uint8_t knob[4];             /* HOME: the four parameters on KNOB 1..4 */
    uint8_t poly;                /* voice cap for POLY and UNISON (up to NVOICE), 0 = NPOLY */
    uint8_t sampled;             /* 1 = plays recorded material (a position, not a phase): voice.c keeps
                                  * no phases over a retrigger, spreads none for UNISON, renders at SUS 0 */
    uint8_t keep;                /* bit k: s[k] is kept when a sounding voice is retriggered (filters) */
    uint8_t oneshot;             /* 1 = hits (DRUM): always POLY, no glide; amp ends its voices (eng_drum.c) */
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    /* optional (0 = none): the voice amplitude instead of the ADSR curve, once per control tick;
     * gets the ADSR value (Q15, env_tick already ran: it still gates the voice), returns Q15 */
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    /* optional: a mode-dependent descriptor of EDIT k (the same range and default as edit[k],
     * another label / value names), 0 = edit[k] */
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    /* optional: once per block and part, before its voices (also with no voice sounding) */
    void (*block)(struct track *t);
    /* optional: the note of key k (0 = the lowest) when the engine maps the keys itself, else -1
     * (then the scale keyboard of seq.c kb_map) */
    int32_t (*keys)(const struct track *t, uint32_t k);
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    /* 1 = note_on restarts the voice's envelopes from nothing (CZ-1, FM6): a sounding voice cannot restart in place, its
     * level would drop to 0 in one sample. A key struck again in its own tail takes another voice; the tail rings on */
    uint8_t restarts;
    /* 1 = a POLY key struck again in its own tail takes another voice and the tail rings on. Without it the voice restarts
     * in place, for an engine with nothing to jump there (JUNO-106: no velocity, the envelope rises from where it is) */
    uint8_t rings_on;
    /* voices of state beyond the cap: a new note starts in one while a voice it took fades */
    uint8_t spare;
    int (*done)(struct track *t, voice_t *v);
    /* optional: MONO / LEGATO / UNISON moved a sounding voice to a new note without a new attack */
    void (*legato)(struct track *t, voice_t *v);
    /* optional: a key went down in MONO / LEGATO / UNISON, whether or not it takes the voice */
    void (*mono_key)(struct track *t, uint32_t note);
    /* optional: the part's block after its voices (FM6: Dexed's DC filter); nr: voices rendered */
    void (*post)(struct track *t, int32_t *out, uint32_t n, uint32_t nr);
    /* optional: the voice cap of the part now, instead of poly  */
    uint32_t (*cap)(const struct track *t);
    /* optional: the voice budget units one of its voices takes now  */
    uint32_t (*units)(const struct track *t);
    /* with presets 0: preset k (< npresets) made on demand, its name into name (21 bytes); engines.c eng_preset */
    void (*preset_make)(uint32_t k, preset_t *p, char *name);
    /* optional: after post, the part's own stereo stage (JUNO-106: its chorus). 1: out now holds L + R and left L,
     * mixed at full width around PAN (mix.c); 0: out stays the mono part */
    uint32_t (*stereo)(struct track *t, int32_t *out, int32_t *left, uint32_t n);
} engine_t;
/* voice_start: what the voice it starts did just before (0 free, 1 released, 2 its key down: a steal or a move);
 * engine_t.note_on may read it  */
static uint8_t voice_was;

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    int8_t mpc_octave;           /* MPC bank's input octave, -3..3; independent of song.octave */
    uint16_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    uint8_t user_native;         /* legacy project origin only; new presets use the shared bank */
    voice_t v[NVOICE];
    /* LFO */
    uint32_t lfo_ph;
    int32_t lfo_val;             /* Q15 */
    int32_t lfo_fade;            /* Q15 ramp after note-on */
    uint32_t lfo_rnd;
    int16_t lfo_prev;
    uint8_t sp_alt;
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t latched;             /* HOLD: keep notes after release */
    /* arp runtime */
    uint32_t arp_pos;            /* samples (INT), or MIDI musical units, into the arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp note, 0 = none */
    uint32_t arp_off;            /* remaining gate in the same clock units */
    /* mono */
    uint8_t mono_stack[NVOICE];  /* keys held, in press order (as many as Dexed keeps voices for) */
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t foot, breath, porta; /* CC4 foot, CC2 breath, CC65 portamento on: FM6's DX7 controllers */
    int16_t bend_raw;            /* the pitch bend as sent (signed 14-bit): FM6 bends by its own range */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
    uint8_t wait_n, wait_note[NVOICE], wait_vel[NVOICE];   /* note-ons waiting for a given-up voice's fade (voice.c voice_wait) */
} track_t;

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays */
    uint8_t sel;                 /* selected track 0..trk_n()-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
} song_t;

static track_t trk[NTRK] __attribute__((section(".pool")));   /* the instrument: parts A and B (saadet_init clears it) */
static song_t song;
#define TSEL (&trk[song.sel])    /* the selected track */

static int32_t input_octave(const track_t *t)
{ return t->p[P_QUANT] == Q_MPC ? t->mpc_octave : song.octave; }
/* The parts in use: part A alone, or both in a multitimbral mode. The other keeps its struct and sound, plays nothing
 * (voice.c trk_note_on) and costs no CPU (mix.c mix_block) */
static inline uint32_t parts_mode(void) { return (uint32_t)song.g[G_PMODE] < PT_N ? (uint32_t)song.g[G_PMODE] : PT_ONE; }
static inline uint32_t trk_n(void) { return parts_mode() == PT_ONE ? 1u : NPART; }

#include "microtonal.h"

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) times steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
static inline void reset_intended(void)         /* a reset on purpose (UBOOT, an update): not a failed boot */
{
    bootguard.pending = 0;
}

/* GLO solo belongs to the mixer, independently of quick-layer effects. */
