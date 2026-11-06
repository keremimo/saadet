/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Keyboard, scale, chords, arpeggiator, MIDI in and the external clock (the synth build: no sequencer).
 * Everything here runs in the audio ISR, once per CTL-sample block, and
 * ends in trk_note_on / trk_note_off: engines never see where a note came from.
 * The keys and MIDI reach the parts as G_PMODE says (core.h, key_parts, midi_parts). A channel's CC1 (mod wheel), CC11
 * (expression) and aftertouch go to the modulation matrix (mod.c) of each part it reaches. A key or MIDI note plays a
 * chord when the part's CHRD is on (chord.c): its notes go through the same input as so many keys. MIDI Start / Continue / Stop of the clock
 * input GLO > GLOBAL CLK picks run the transport the ARP, the SLICER and the FX layer follow (song.playing); with
 * the internal clock they run free at BPM. */
#define KB_SILENT 255u
/* A key plays a part through its slot: k for part A, k + KB_N for part B, so in LAYER one key holds two notes and two
 * chords. A key's parts are fixed while it is down: its release ends what it started there */
#define KB_SLOTS (KB_N * NPART)
#define KB_SLOT(k, p) ((k) + (p) * KB_N)
static volatile uint8_t kb_asleep;
static kb_word_t kb_prev, kb_down[NPART];        /* the keys down; those down on each part */
static uint8_t kb_note[KB_SLOTS], kb_parts[KB_N];   /* per slot: the note it started; per key: its parts, bit per part */
static uint8_t last_note = 60;
/* Stage's played notes, after scale/chord mapping. The ISR owns the held bits per
 * track; each track's last complete voicing survives release, including a tap between UI frames. */
static uint32_t live_held[NTRK][4], live_last[NTRK][4];
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

/* MIDI channel 0..15 of part i (keys -> MIDI out): OUT CH for part A, its own for part B */
static uint32_t trk_midi_ch(uint32_t i) { return (uint32_t)(i ? song.g[G_PCH2] : song.g[G_DRUMCH]) - 1u; }

/* the parts key k plays now: both in LAYER; by where the key is in SPLIT, whatever the octave (a chord pad: the selected
 * part); else the selected part */
static uint32_t key_parts(uint32_t k)
{
    uint32_t mode = parts_mode();
    if (mode == PT_LAYER)
        return (1u << NPART) - 1u;
#ifdef SAADET_SMK37
    if (k >= KB_PAD0)
        return 1u << song.sel;
#endif
    if (mode == PT_SPLIT)
        return 53 + KB_OFS(k) < song.g[G_PSPLIT] ? 2u : 1u;
    return 1u << song.sel;
}
/* the parts the keyboard's own controls reach (wheels, REC, the pedal's message): all it can play */
static uint32_t keyboard_parts(void)
{
    uint32_t mode = parts_mode();
    return mode == PT_SPLIT || mode == PT_LAYER ? (1u << NPART) - 1u : 1u << song.sel;
}
/* the part key k lights for: the selected one if it plays that */
static track_t *key_track(uint32_t k)
{
    uint32_t parts = key_parts(k);
    return (parts >> song.sel) & 1u ? TSEL : &trk[parts == 2u];
}

static void live_note_on(const track_t *t, uint32_t note)
{
    uint32_t i, k = trk_index(t);
    live_held[k][note >> 5] |= 1u << (note & 31u);
    for (i = 0; i < 4u; i++)
        live_last[k][i] = live_held[k][i];
}

/* the 27 keys from F: black or white, and the key's place among the keys of its colour (white 0..15, black
 * 0..10) */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}

static uint32_t scale_mask(const track_t *t)
{
    const micro_scale_t *s = micro_scale(t);
    if (s) { /* approximate pitch classes for the keyboard LEDs only */
        uint32_t mask = 0;
        for (uint32_t i = 0; i < s->count; i++)
            mask |= 1u << ((s->pitch[i] + 5000) / 10000 % 12);
        return mask;
    }
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, SCALE_LEGACY - 1)];
}

static uint32_t scale_count(const track_t *t)
{
    if (micro_scale(t)) return micro_scale(t)->count;
    uint32_t mask = scale_mask(t), count = 0;
    while (mask) {
        count += mask & 1u;
        mask >>= 1;
    }
    return count;
}

static int32_t mpc_degree(const track_t *t) { return clamp(t->p[P_MPCDEG], 1, (int32_t)scale_count(t)); }

/* scale degree `degree` from C4 (0 = the ROOT above C4) in the track's scale, + offset semitones and TRN. strict: a
 * note off the MIDI range is silent (else clamped). Scales of 5, 6, 8 or 12 notes repeat no degree */
static uint32_t scale_degree_map(const track_t *t, int32_t degree, int32_t offset, int strict)
{
    if (micro_active(t)) return micro_degree_map(t, degree, offset, strict);
    uint32_t mask = scale_mask(t), i;
    int32_t count = (int32_t)scale_count(t), oct, n;
    oct = degree / count;
    degree %= count;
    if (degree < 0) {
        degree += count;
        oct--;
    }
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u) {
            if (!degree)
                break;
            degree--;
        }
    n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i + offset + t->p[P_TRANS];
    if (strict && (n < 0 || n > 127))
        return KB_SILENT;
    return (uint32_t)clamp(n, 0, 127);
}

/* note n (a key's or a MIDI note) through the WHITE / ALL layouts. WHITE (and MPC on the keys): the white keys walk
 * the scale from C4 = ROOT, the black keys are silent. ALL: every key the next degree of the scale */
static uint32_t scale_map(const track_t *t, int32_t n, int32_t offset)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t degree;
    if (t->p[P_QUANT] == Q_ALL)
        return scale_degree_map(t, n - 60, offset, 1);
    if (t->p[P_QUANT] == Q_WHITE || t->p[P_QUANT] == Q_MPC) {
        if ((degree = DEGREE[n % 12]) < 0)
            return KB_SILENT;
        return scale_degree_map(t, degree + (n / 12 - 5) * 7, offset, 0);
    }
    return (uint32_t)clamp(n + offset + t->p[P_TRANS], 0, 127);
}

static uint32_t kb_map(const track_t *t, uint32_t k)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the engine it switches to) */
    int32_t n;
#ifdef SAADET_SMK37
    if (k >= KB_PAD0)                                  /* a degree pad: the scale from the ROOT at C3 and up, the octave */
        return scale_degree_map(t, (int32_t)k - (int32_t)KB_PAD0, 12 * (input_octave(t) - 1), 0);
#endif
    if (e->keys && (n = e->keys(t, k)) >= 0)           /* the engine's own key map (GM kit, slices) */
        return (uint32_t)n;
    n = 53 + KB_OFS(k);
    if (t->p[P_QUANT] == Q_SNAP) {                     /* SNAP: every key, rounded down to the scale */
        if (micro_active(t)) {
            uint32_t note = micro_snap(t, n + t->p[P_TRANS]);
            return note == KB_SILENT ? note : micro_degree_map(t, (int32_t)note - 60, 12 * input_octave(t), 1);
        }
        uint32_t mask = scale_mask(t), guard = 12;
        n += 12 * input_octave(t) + t->p[P_TRANS];
        while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    return scale_map(t, n, 12 * input_octave(t));
}

/* a MIDI note into track t: WHITE and ALL map it as the keys (without the octave buttons), MPC plays the 16 pads of
 * an MPC's Bank H (MIDI 20..35, H01..H16 of MPC Sample's default map; any other note is silent, kits too) as
 * successive degrees of the scale, pad H02 on degree DEG. Kits and slices keep their own note map. KB_SILENT: none */
static uint32_t midi_map(const track_t *t, uint32_t note)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    if(t->p[P_QUANT]==Q_MPC){
        if(note<20u || note>35u)return KB_SILENT;
        return scale_degree_map(t,(int32_t)note-21+mpc_degree(t)-1,12*input_octave(t),1);
    }
    if (e->keys && e->keys(t, 0) >= 0)
        return note;
    if (t->p[P_QUANT] == Q_WHITE || t->p[P_QUANT] == Q_ALL)
        return scale_map(t, (int32_t)note, 0);
    if (micro_active(t) && t->p[P_QUANT] == Q_SNAP)
        return micro_snap(t, (int32_t)note + t->p[P_TRANS]);
    return note;
}

#include "chord.c"

/* ------------------------------------------------------------- arp --- */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (t->p[P_AHOLD] && t->arp_phys == 0u)
        t->nheld = 0;                               /* new chord replaces the latched one */
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u)
        t->held[t->nheld++] = (uint8_t)note;
    if (t->nheld == 1u) {
        t->arp_pos = 0xFFFFFFF;                     /* fire on this block */
        t->arp_idx = 0xFFFFFFFFu;
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (t->arp_phys)
        t->arp_phys--;
    if (t->p[P_AHOLD])
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note)
            t->held[k++] = t->held[i];
    t->nheld = (uint8_t)k;
}

static void arp_off(track_t *t)                  /* the sounding arp note ends */
{
    trk_note_off(t, t->arp_note);
    t->arp_note = 0;
}

/* n and gate_n: sample time (INT), musical units (MIDI); a stopped external
 * transport uses real time converted to musical units for a tapped note's gate. */
static volatile uint32_t beat_pos, beat_n;      /* clock units into the beat, beat of the bar (0..3) */
static void arp_step(track_t *t, uint32_t n, uint32_t gate_n)
{
    uint32_t period, cnt, list[64], len = 0, i, j, o;
    int32_t sw;
    if (t->arp_note) {
        if (t->arp_off <= gate_n)
            arp_off(t);
        else
            t->arp_off -= gate_n;
    }
    if (!t->p[P_AMODE] || !t->nheld) {
        if (!t->nheld && t->arp_note)
            arp_off(t);
        return;
    }
    period = seq_div_samples((uint32_t)t->p[P_ARATE]);
    sw = t->p[P_ASWING] * (int32_t)period / 250;
    t->arp_pos += n;
    for (;;) {
        if (t->arp_pos < period + (uint32_t)((t->arp_idx & 1u) ? sw : -sw) && t->arp_pos != 0xFFFFFFF + n)
            return;
        if (t->arp_pos >= 0xFFFFFFFu)
            t->arp_pos = 0;
        else if (midi_sync_active)
            t->arp_pos -= period + (uint32_t)((t->arp_idx & 1u) ? sw : -sw);
        else
            t->arp_pos = 0;
        len = 0;
        /* build the note list: held notes (sorted or as played) over OCT octaves */
        for (i = 0; i < t->nheld; i++)
            list[i] = t->held[i];
        cnt = t->nheld;
        if (!t->p[P_AORDER])
            for (i = 1; i < cnt; i++)
                for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                    uint32_t x = list[j];
                    list[j] = list[j - 1];
                    list[j - 1] = x;
                }
        for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
            for (i = 0; i < cnt && len < 64u; i++)
                if (micro_active(t)) {
                    uint32_t note = micro_degree_map(t, (int32_t)list[i] - 60 + (int32_t)scale_note_period(t) * (int32_t)o, 0, 1);
                    if (note < 128u) list[len++] = note;
                } else {
                    list[len++] = clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
                }
        if (!len) return;
        if (t->p[P_AMODE] == 6) { list[0] = t->held[t->nheld - 1u]; len = 1; } /* REPEAT: last played note, no octave traversal */
        t->arp_idx++;
        switch (t->p[P_AMODE]) {
        case 2:
            j = len - 1u - t->arp_idx % len;
            break;
        case 3: {
            uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = t->arp_idx % cyc;
            j = k < len ? k : cyc - k;
            break;
        }
        case 4:
            j = rng() % len;
            break;
        default:
            j = t->arp_idx % len;
            break;
        }
        if (t->arp_note)
            arp_off(t);
        if ((uint32_t)(rng() & 127u) <= (uint32_t)t->p[P_APROB]) {
            t->arp_note = (uint8_t)list[j];
            t->arp_off = period * (uint32_t)t->p[P_AGATE] / 128u;
            trk_note_on(t, t->arp_note, 100);
        }
        if (!midi_sync_active) return;
        /* Recovered clocks can cross several ARP steps. Retain the residual
         * phase and the gate remaining at the end of the elapsed time. */
        if (t->arp_note) {
            if (t->arp_off <= t->arp_pos) arp_off(t);
            else t->arp_off -= t->arp_pos;
        }
    }
}

static void arp_tick(track_t *t, uint32_t n) { arp_step(t, n, n); }

/* -------------------------------------------------------- note input --- */
static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    last_note = (uint8_t)note;
    live_note_on(t, note);
    if (t->p[P_AMODE])
        arp_add(t, note);
    else
        trk_note_on(t, note, vel);
}

static int midi_note_held(const track_t *t, uint32_t note);
static int midi_local_held(const track_t *t, uint32_t note);
static void input_off(track_t *t, uint32_t note)
{
    if (midi_note_held(t, note))
        return;
    live_held[trk_index(t)][note >> 5] &= ~(1u << (note & 31u));
    arp_remove(t, note);                            /* both: the note may have started in the */
    trk_note_off(t, note);                          /* other mode (ARP switched while held) */
}

/* REC (P_AHOLD) with the arp off: the notes the keys let go of keep sounding (and their MIDI note-offs wait) until
 * the next keys go down from none, or REC lets go (seq_block). The arp latches its own (arp_add) */
static uint32_t kb_latched[NTRK][4];
/* ARP (kb_pedal, ui.c) is the sustain pedal and it latches, a drone: the notes the keys strike or let go of while it
 * is down keep sounding (and their MIDI note-offs wait) after it lets go, the arp's too (they stay in its notes), and
 * the keys play over them, until ARP is pressed again (kb_sustain_end, ui.c). A key striking one again restarts it, and
 * it stays */
static uint32_t kb_sustained[NTRK][4];
#ifdef SAADET_SMK37
/* the SMK-37's jack is a plain foot pedal, apart from ARP's drone: the notes the keys let go of while it is down sound
 * on (the arp's too) until it lifts, or REC latches them; a key down when it lifts plays on until it lets go. A key
 * striking one again ends it first, then starts it anew */
static uint32_t kb_footed[NTRK][4];
#endif
static int latch_plain(const track_t *t) { return t->p[P_AHOLD] && !t->p[P_AMODE]; }
static int pedal_down(void) { return (fm1_in.buttons & kb_pedal) != 0u; }
static void note_bit(uint32_t *set, uint32_t x) { set[x >> 5] |= 1u << (x & 31u); }
static int note_in(const uint32_t *set, uint32_t x) { return (set[x >> 5] >> (x & 31u)) & 1u; }
static void key_note_off(track_t *t, uint32_t x)
{
    input_off(t, x);
    midi_out_event(0x08u | (0x80u | trk_midi_ch(trk_index(t))) << 8 | x << 16);
}
/* the notes of set no key sounds end (latch: REC keeps them instead); set empties */
static void kept_release(track_t *t, uint32_t *set, int latch)
{
    uint32_t x;
    if (!(set[0] | set[1] | set[2] | set[3]))
        return;
    for (x = 0; x < 128u; x++)
        if (note_in(set, x) && !midi_local_held(t, x)) {
            if (latch)
                note_bit(kb_latched[trk_index(t)], x);
            else
                key_note_off(t, x);
        }
    memset(set, 0, 4u * sizeof *set);
}
static void latch_release(track_t *t) { kept_release(t, kb_latched[trk_index(t)], 0); }
static void pedal_release(track_t *t) { kept_release(t, kb_sustained[trk_index(t)], 0); }
#ifdef SAADET_SMK37
static void foot_release(track_t *t) { kept_release(t, kb_footed[trk_index(t)], latch_plain(t)); }
#endif
/* the pedal keeps x; the latch no longer does, or the next keys would end it */
static void pedal_keep(track_t *t, uint32_t x)
{
    uint32_t tr = trk_index(t);
    note_bit(kb_sustained[tr], x);
    kb_latched[tr][x >> 5] &= ~(1u << (x & 31u));
}
/* ui.c: the pedal keeps notes: the status line and ARP's LED show it, and ARP's next press ends them */
static int sustain_on(void)
{
    uint32_t i, k;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 4u; k++)
            if (kb_sustained[i][k])
                return 1;
    return 0;
}

/* the effect of key k in the FX layer (PF_N none): the first PF_M1 white keys (F3 .. A4) in order, black keys
 * 1..4 (F#3 G#3 A#3 C#4) the track mutes */
static void key_chord_tone(uint32_t k, track_t *t)
{
    uint32_t s = KB_SLOT(k, trk_index(t)), x = kb_chord[s][kb_chn[s]], held = midi_local_held(t, x);
    int kept = note_in(kb_sustained[trk_index(t)], x);
#ifdef SAADET_SMK37
    if (note_in(kb_footed[trk_index(t)], x)) {      /* the foot pedal's: a key has it now */
        kb_footed[trk_index(t)][x >> 5] &= ~(1u << (x & 31u));
        kept = 1;
    }
#endif
    kb_chn[s]++;
    if (pedal_down())
        pedal_keep(t, x);
    if (held) return;
    if (kept)                                       /* the pedal keeps it: struck again, it ends first */
        key_note_off(t, x);
    input_on(t, x, KB_VEL(k));
    midi_out_event(0x09u | (0x90u | trk_midi_ch(trk_index(t))) << 8 | x << 16 | KB_VEL(k) << 24);
}

static void key_on(uint32_t k, track_t *t)
{
    uint32_t n, p = trk_index(t), s = KB_SLOT(k, p);
    if (!(kb_down[p] & ~KB_BIT(k)))                 /* the part's keys from none: a new chord replaces the latched one */
        latch_release(t);
    n = chord_build(t, kb_note[s], kb_chord[s]);
    n = chord_strum_begin(t, kb_chord[s], n, &kb_strum[s], KB_VEL(k));
    kb_chn[s] = 0;
    /* Publish pending work after the first tone: input_on may start transport. */
    for (uint32_t i = 0; i < n; i++) key_chord_tone(k, t);
    if (kb_strum[s].total) kb_strum_pending[p] |= KB_BIT(k);
    last_note = kb_note[s];
}

/* key k is up: the notes it started end, but those another key still holds */
static void key_off(uint32_t k, track_t *t)
{
    uint32_t p = trk_index(t), s = KB_SLOT(k, p), n = kb_chn[s], i;
    kb_strum_pending[p] &= ~KB_BIT(k);
    kb_strum[s].total = 0;
    kb_chn[s] = 0;
    for (i = 0; i < n; i++) {
        uint32_t x = kb_chord[s][i];
        if (midi_local_held(t, x)) continue;
        if (pedal_down())
            pedal_keep(t, x);
        if (note_in(kb_sustained[trk_index(t)], x))
            continue;
#ifdef SAADET_SMK37
        if (sd_jack_pedal) {
            note_bit(kb_footed[trk_index(t)], x);
            continue;
        }
#endif
        if (latch_plain(t))
            note_bit(kb_latched[trk_index(t)], x);
        else
            key_note_off(t, x);
    }
}

#ifdef FM1_INPUT_LAT
/* the press latency stats (hal/fm1_input.h fm1_in_stat, console `inp`): from the first scan that saw
 * key k closed to its note-on here, and to the note's first sample leaving the DMA (kb_out_tick:
 * audio.c, the TIMER4 tick at which the block being rendered starts to play) */
static uint32_t kb_out_tick;
static __attribute__((noinline)) void kb_lat(uint32_t k)
{
    uint32_t t0 = fm1_in.note_t0[k], d = fm1_ticks() - t0, o = kb_out_tick - t0;
    fm1_in_stat.kb_n++;
    fm1_in_stat.kb_sum += d;
    fm1_in_stat.dac_sum += o;
    if (d > fm1_in_stat.kb_max)
        fm1_in_stat.kb_max = d;
    if (o > fm1_in_stat.dac_max)
        fm1_in_stat.dac_max = o;
}
#endif

static void keyboard_block(void)
{
    kb_word_t cur = KB_KEYS(), ch;
    uint32_t k;
#ifdef SAADET_SMK37
    {   /* the jack's pedal tells the outside; keys struck again before a full lift: their bit never dropped, so the note
         * ends and starts anew at the new velocity (a key a held button owns, or that just went down or up: as is) */
        static uint8_t pedal_was;
        kb_word_t rt = sd_kb_retrig;              /* (the main loop's writer locks against this interrupt) */
        sd_kb_retrig = 0;
        if (sd_jack_pedal != pedal_was) {
            pedal_was = sd_jack_pedal;
            for (uint32_t p = 0; p < NPART; p++)
                if ((keyboard_parts() >> p) & 1u)
                    midi_out_event(0x0Bu | (0xB0u | trk_midi_ch(p)) << 8 | 64u << 16 | (pedal_was ? 127u : 0u) << 24);
        }
        rt &= cur & kb_prev & ~kb_layer;
        for (k = 0; rt && k < KB_N; k++)
            for (uint32_t p = 0; (rt >> k) & 1u && p < NPART; p++)
                if ((kb_parts[k] >> p) & 1u && kb_note[KB_SLOT(k, p)] != KB_SILENT) {
                    key_off(k, &trk[p]);
                    key_on(k, &trk[p]);
                }
    }
#endif
    ch = cur ^ kb_prev;                           /* keys also sound while entering steps */
    if (!ch) return;
    for (k = 0; k < KB_N; k++) {
        uint32_t p;
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                    /* its parts now; the key-up goes to the same ones */
            uint32_t parts = key_parts(k);
            kb_parts[k] = (uint8_t)parts;
            for (p = 0; p < NPART; p++)
                if ((parts >> p) & 1u)
                    kb_note[KB_SLOT(k, p)] = KB_SILENT;
            if (kb_asleep) continue;
            if ((fm1_in.buttons & kb_mask) || kb_lock) {       /* a layer's button held: the key is the layer's (ui_layer.c), */
                kb_layer |= KB_BIT(k);            /* a quick-layer key */
                continue;
            }
            for (p = 0; p < NPART; p++) {
                if (!((parts >> p) & 1u))
                    continue;
                kb_down[p] |= KB_BIT(k);
                kb_note[KB_SLOT(k, p)] = song.grid ? KB_SILENT : (uint8_t)kb_map(&trk[p], k);
                if (kb_note[KB_SLOT(k, p)] != KB_SILENT)
                    key_on(k, &trk[p]);
            }
#ifdef FM1_INPUT_LAT
            kb_lat(k);
#endif
        } else {
            if ((kb_layer >> k) & 1u) {
                kb_layer &= ~KB_BIT(k);
                continue;
            }
            for (p = 0; p < NPART; p++) {
                if (!((kb_parts[k] >> p) & 1u))
                    continue;
                kb_down[p] &= ~KB_BIT(k);
                if (kb_note[KB_SLOT(k, p)] != KB_SILENT)
                    key_off(k, &trk[p]);
            }
        }
    }
    kb_prev = cur;
}

/* --------------------------------------------------------- transport --- */
/* MIDI Start: the ARP's beat, the SLICER's step 0 and the FX layer's 1/16 from the top; LFOs that retrigger with a
 * synced cycle restart */
static void transport_start(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (t->p[P_LTRIG] && t->p[P_LSYNC]) t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
    }
    song.tick = 0;
    beat_pos = 0; beat_n = 0;                      /* the ARP LED's beat from the top too */
    song.playing = 1;

}

static void transport_stop(void)
{
    uint32_t i;
    song.playing = 0;
    if (midi_sync_active)                           /* external clock: the arp stops with the transport, */
        for (i = 0; i < NPART; i++)                /* its sounding note too (HOLD keeps the latched chord) */
            if (trk[i].arp_note)
                arp_off(&trk[i]);
}

/* MIDI in: every channel plays the track */
static void midi_chord_tone(mchord_t *m)
{
    track_t *t = &trk[m->id - 1u];
    uint32_t note = m->note[m->n];
    uint32_t held = midi_note_held(t, note) || midi_local_held(t, note);
    m->n++;
    /* MPC's duplicate drum pads are separate strikes, even while another pad holds that sound. */
    if (!held) input_on(t, note, m->strum.vel);
}

static uint32_t chord_strum_next(uint32_t limit)
{
    for (uint32_t p = 0; p < NPART; p++)
        for (uint32_t k = 0; kb_strum_pending[p] && k < KB_N; k++) if (kb_strum_pending[p] & KB_BIT(k)) {
            uint32_t s = KB_SLOT(k, p), due = chord_strum_due(&kb_strum[s], kb_chn[s], 0);
            if (due < limit) limit = due;
        }
    for (uint32_t k = 0; midi_strum_pending && k < MCHORD_N; k++) if (midi_strum_pending & (1u << k)) {
        uint32_t due = chord_strum_due(&mchord[k].strum, mchord[k].n, 0);
        if (due < limit) limit = due;
    }
    return limit;
}

static void chord_strum_tick(uint32_t n)
{
    for (uint32_t p = 0; p < NPART; p++)
        for (uint32_t k = 0; kb_strum_pending[p] && k < KB_N; k++) if (kb_strum_pending[p] & KB_BIT(k)) {
            uint32_t slot = KB_SLOT(k, p);
            chord_strum_t *s = &kb_strum[slot];
#ifdef SAADET_SMK37
            /* leaning on a degree pad strums faster, up to twice the rate set */
            chord_strum_due(s, kb_chn[slot], k >= KB_PAD0 ? n + n * sd_kb_press[k - KB_PAD0] / 127u : n);
#else
            chord_strum_due(s, kb_chn[slot], n);
#endif
            while (kb_chn[slot] < s->total && !chord_strum_due(s, kb_chn[slot], 0)) key_chord_tone(k, &trk[p]);
            if (kb_chn[slot] == s->total) kb_strum_pending[p] &= ~KB_BIT(k);
        }
    for (uint32_t k = 0; midi_strum_pending && k < MCHORD_N; k++) if (midi_strum_pending & (1u << k)) {
        mchord_t *m = &mchord[k];
        chord_strum_due(&m->strum, m->n, n);
        while (m->n < m->strum.total && !chord_strum_due(&m->strum, m->n, 0)) midi_chord_tone(m);
        if (m->n == m->strum.total) midi_strum_pending &= ~(1u << k);
    }
}

/* the parts channel ch reaches, bit per part. MULTI: part B on its own channel, part A on IN CH (OMNI: every other
 * channel). One part: IN CH. SPLIT and LAYER: IN CH reaches both */
static uint32_t midi_parts(uint32_t ch)
{
    uint32_t mode = parts_mode(), in = !song.g[G_ROUTE] || ch + 1u == (uint32_t)song.g[G_ROUTE];
    if (mode == PT_MULTI)
        return ch + 1u == (uint32_t)song.g[G_PCH2] ? 2u : in;
    return !in ? 0u : mode == PT_ONE ? 1u : (1u << NPART) - 1u;
}
/* the parts note plays on channel ch: SPLIT by the split point, else the channel's */
static uint32_t midi_note_parts(uint32_t ch, uint32_t note)
{
    uint32_t parts = midi_parts(ch);
    return parts && parts_mode() == PT_SPLIT ? (note < (uint32_t)song.g[G_PSPLIT] ? 2u : 1u) : parts;
}
/* the sound channel ch's mapped controllers edit: where both parts listen, the selected one */
static track_t *midi_track(uint32_t ch)
{
    uint32_t parts = midi_parts(ch);
    return !parts ? 0 : (parts >> song.sel) & 1u ? TSEL : &trk[parts == 2u];
}

#include "midi_param.c"
#include "midi_input.c"
#include "midi_clock.c"

static void clock_advance(uint32_t n)
{
    do {
        uint32_t step=chord_strum_next(n), beat=seq_beat_samples();
        for (uint32_t p = 0; p < trk_n(); p++)
            arp_step(&trk[p], song.playing || !song.g[G_CLOCK] ? step : 0u, step);
        beat_pos+=step;
        while(beat_pos>=beat){beat_pos-=beat;beat_n=(beat_n+1u)&3u;}
        chord_strum_tick(step);
        n-=step;
    } while(n);
}

static uint8_t parts_mode_was;
/* everything that happens between two rendered blocks */
static void events_block(uint32_t n)
{
    uint32_t i, pr;
    voice_units_block(1);                              /* (a part's voice cost once for the block's events: voice.c) */
#ifdef SAADET_SMK37
    if (song.g[G_CLOCK] == 2)                          /* (a song from elsewhere, the editor) nothing can come in on TRS */
        song.g[G_CLOCK] = 0;
#endif
    uint32_t selected_clock = (uint32_t)song.g[G_CLOCK];
    if (midi_clock.selected != selected_clock) {
        transport_stop();
        midi_clock_mode(0);
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_clock.selected = (uint8_t)selected_clock;
        midi_beat_samples = 0;
    }
    uint32_t clock_mode = midi_clock.mode;
    pr = panic_req;
    panic_req = 0;
    uint32_t sustain_end = kb_sustain_end;
    kb_sustain_end = 0;
    if (parts_mode() != parts_mode_was) {              /* another parts mode: nothing sounding or held carries over, */
        parts_mode_was = (uint8_t)parts_mode();        /* and one part means part A */
        pr |= (1u << NTRK) - 1u;
        if (trk_n() == 1u)
            song.sel = 0;
    }
    if (midi_in_overflow) {                            /* a lost note-off must never leave a held note */
        mi_r = mi_w;
        midi_input_reset();
        memset(mchord, 0, sizeof mchord);
        pr |= (1u << NTRK) - 1u;
        RING_PUBLISH();
        midi_in_overflow = 0;
    }
    voice_fades_end();                                 /* (voice.c) before any part looks for room */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if ((pr >> i) & 1u) {
            midi_forget_track(i);
            trk_all_off(t);
            memset(kb_latched[i], 0, sizeof kb_latched[i]);
            memset(kb_sustained[i], 0, sizeof kb_sustained[i]);
#ifdef SAADET_SMK37
            memset(kb_footed[i], 0, sizeof kb_footed[i]);
#endif
            t->nheld = 0;
            t->arp_phys = 0;
            t->arp_note = 0;
            if (i >= trk_n()) {                       /* out of use: it renders no more, so nothing may stay active, */
                for (uint32_t v = 0; v < NVOICE; v++) {   /* and its engine gives its memory back */
                    t->v[v].active = t->v[v].gate = t->v[v].stage = 0;
                    t->v[v].env = t->v[v].env_out = 0;
                }
                t->engine = eng_idx(t->eng_req);
                t->xf_on = 0;
                eng_state_clear(i);
            }
        }
        if (i >= trk_n())
            continue;
        engine_block(t);                              /* engine switch: fade, then switch (voice.c) */
        voice_wait_block(t);                          /* notes whose stolen voices have faded (voice.c) */
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !t->p[P_AMODE]) || (t->aholdp && !t->p[P_AHOLD] && !t->arp_phys)) {
            t->nheld = 0;
            if (!t->p[P_AMODE])
                t->arp_phys = 0;
            if (t->arp_note) {
                trk_note_off(t, t->arp_note);
                t->arp_note = 0;
            }
        }
        if (sustain_end)                              /* ARP pressed again: what the pedal kept ends */
            pedal_release(t);
#ifdef SAADET_SMK37
        if (!sd_jack_pedal)                           /* the foot pedal lifted: what it kept ends, or REC latches it */
            foot_release(t);
#endif
        if (!latch_plain(t))                          /* REC let go (or the arp took over): what it latched ends */
            latch_release(t);
        t->armp = t->p[P_AMODE];
        t->aholdp = t->p[P_AHOLD];
    }
    keyboard_block();
    while (mi_r != mi_w) {                            /* USB-MIDI (and TRS) in */
        uint32_t at = mi_r % MQ, pkt = midi_in_q[at], status = (pkt >> 8) & 0xFFu;
        uint32_t st = status & 0xF0u, ch = status & 0x0Fu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t source = midi_in_source[at] ? midi_in_source[at] : 1u, ms = midi_in_ms[at];
        uint32_t time = midi_in_time[at];
        uint16_t pulse_count = midi_in_pulse[at];
        if (midi_render_timed && (int32_t)(time - midi_render_time) > 0)
            break;
        mi_r++;
        if (status >= 0xF8u) {
            if (selected_clock == source) {
                if (status == 0xFAu || status == 0xFBu || status == 0xFCu) {
                    if (!clock_mode) midi_clock_mode(source);
                    clock_mode = source;
                }
                if (!clock_mode) continue; /* clocks alone cannot claim the transport */
                if (status == 0xF8u)
                    midi_clock_pulse(ms, time, pulse_count);
                else {
                    if (song.playing)
                        clock_advance(midi_clock_advance(time));
                    midi_clock_transport(status, ms);
                    if (status == 0xFAu || status == 0xFBu) {
                        midi_clock.pulse_count = pulse_count;
                        midi_clock.count_valid = 1;
                    }
                }
                if (song.playing)
                    clock_advance(midi_clock_advance(time));
            }
        } else {
            if (clock_mode && song.playing)
                clock_advance(midi_clock_advance(time));
            midi_event(st, ch, d1, d2);
        }
    }
    if (clock_mode) {
        if (song.playing) {
            uint32_t last = midi_clock.have_pulse ? midi_clock.last_ms : midi_clock.start_ms;
            if (fm1_ms - last > 500u) {
                transport_stop();                   /* cable loss must not leave a running held note */
                midi_clock.tempo_valid = 0;
            } else
                clock_advance(midi_clock_advance(midi_render_timed ? midi_render_time : MIDI_TIME_NOW()));
        }
        if (!song.playing)
            for (i = 0; i < NPART; i++)
                arp_step(&trk[i], 0, (uint32_t)((uint64_t)n * MIDI_BEAT_UNITS / beat_samples()));
    } else
        clock_advance(n);
    if (song.playing)
        song.tick++;
    voice_units_block(0);
}
