/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* MIDI performance input: notes with per-channel ownership, the sustain pedal, pitch bend and its sensitivity (RPN 0),
 * and the channel mode messages. Included by seq.c after midi_param.c; midi_event runs in the audio ISR's control
 * step (events_block), once per channel voice message.
 * Each of the 16 channels keeps its own bend, sensitivity, pedal and RPN selection, whichever port its messages came
 * in on. A channel owns the notes it started, per track: its note-offs, pedal and All Notes Off end exactly those,
 * even after the routing changed. Its bend, sensitivity and mode messages act on its targets: the tracks it routes to
 * now and every track it still owns notes on. */
static struct {
    int16_t bend;                /* -8192..8191, 0 = centre */
    uint8_t semi, cents;         /* the bend's sensitivity (RPN 0): 0..24 semitones, 0..99 cents */
    uint8_t rpn[2];              /* the selected RPN, MSB and LSB: 127 / 127 = none (also after an NRPN) */
    uint8_t pedal;               /* CC64 down */
} midi_chan[16];
/* bit per note: the notes each channel owns on each track (LAYER: on both), and of those the ones only the pedal keeps.
 * (NTRK + 1) x 256 bytes */
static uint32_t midi_own[NTRK][16][4], midi_kept[16][4];

/* the tracks channel ch owns note on, bit per track */
static uint32_t midi_owners(uint32_t ch, uint32_t note)
{
    uint32_t tr, mask = 0;
    for (tr = 0; tr < NTRK; tr++)
        mask |= ((midi_own[tr][ch][note >> 5] >> (note & 31u)) & 1u) << tr;
    return mask;
}

/* channel ch's targets, bit per track */
static uint32_t midi_targets(uint32_t ch)
{
    uint32_t mask = midi_parts(ch), tr;
    for (tr = 0; tr < NTRK; tr++)
        if (midi_own[tr][ch][0] | midi_own[tr][ch][1] | midi_own[tr][ch][2] | midi_own[tr][ch][3])
            mask |= 1u << tr;
    return mask;
}

/* channel ch's bend on track t: bend_raw as sent (FM6 ranges it itself), the target by the channel's sensitivity or
 * the Prophet patch's own range */
static void midi_bend_to(uint32_t ch, track_t *t)
{
    t->bend_raw = midi_chan[ch].bend;
    if (t->eng_req == ENGI_PROPHET)
        p5_update_bend(t);
    else
        midi_bend_target[trk_index(t)] =
            midi_bend_scale(midi_chan[ch].bend, midi_chan[ch].semi * 256 + midi_chan[ch].cents * 256 / 100);
}

static void midi_bend_all(uint32_t ch)
{
    uint32_t mask = midi_targets(ch), tr;
    for (tr = 0; tr < NTRK; tr++)
        if ((mask >> tr) & 1u)
            midi_bend_to(ch, &trk[tr]);
}

/* a MIDI-owned note of t sounds pitch note: one played as itself (pressed or kept by the pedal; a note playing a
 * pool entry sounds its tones instead), or a tone of one of t's MIDI chords */
static int midi_note_held(const track_t *t, uint32_t note)
{
    uint32_t tr = trk_index(t), ch;
    for (ch = 0; ch < 16u; ch++)
        if ((midi_own[tr][ch][note >> 5] >> (note & 31u)) & 1u && !mchord_of(ch, note, tr + 1u))
            return 1;
    return mchord_held(tr + 1u, note);
}

/* a key of t sounds note */
static int midi_local_held(const track_t *t, uint32_t note)
{
    uint32_t tr = trk_index(t), k, i;
    for (k = 0; k < KB_N; k++) {
        uint32_t s = KB_SLOT(k, tr);
        for (i = 0; i < kb_chn[s] && i < CHORD_MAX; i++)
            if (kb_chord[s][i] == note)
                return 1;
    }
    return 0;
}

/* pitch x of t ends, unless a key of t sounds it (input_off keeps one another MIDI note holds) */
static void midi_pitch_off(track_t *t, uint32_t x)
{
    if (!midi_local_held(t, x))
        input_off(t, x);
}

/* channel ch's note on track tr ends, whatever the pedal: first its ownership (input_off asks midi_note_held), then
 * the tones its pool entry started (those still to strum never start), or its own pitch */
static void midi_end(uint32_t ch, uint32_t note, uint32_t tr)
{
    track_t *t = &trk[tr];
    mchord_t *m = mchord_of(ch, note, tr + 1u);
    uint32_t i;
    midi_own[tr][ch][note >> 5] &= ~(1u << (note & 31u));
    midi_kept[ch][note >> 5] &= ~(1u << (note & 31u));
    if (!m) {
        midi_pitch_off(t, note);
        return;
    }
    midi_strum_pending &= ~(1u << (uint32_t)(m - mchord));
    m->id = 0;
    for (i = 0; i < m->n; i++)
        midi_pitch_off(t, m->note[i]);
}

/* channel ch's note ends on every track it owns it on */
static void midi_end_all(uint32_t ch, uint32_t note, uint32_t owners)
{
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++)
        if ((owners >> tr) & 1u)
            midi_end(ch, note, tr);
}

/* note-off (release velocity ignored): with the pedal down the note stays, kept by the pedal */
static void midi_note_off(uint32_t ch, uint32_t note)
{
    uint32_t owners = midi_owners(ch, note);
    if (!owners)
        return;
    if (midi_chan[ch].pedal)
        midi_kept[ch][note >> 5] |= 1u << (note & 31u);
    else
        midi_end_all(ch, note, owners);
}

static void midi_pedal_up(uint32_t ch)
{
    uint32_t note;
    midi_chan[ch].pedal = 0;
    for (note = 0; note < 128u; note++)
        if ((midi_kept[ch][note >> 5] >> (note & 31u)) & 1u)
            midi_end_all(ch, note, midi_owners(ch, note));
}

/* a note on one track: mapped and built as the keys do. A note that plays itself starts once per track (other MIDI
 * notes and keys may sound it already); a chord or another pitch takes a pool entry (full: only a note that plays itself
 * sounds, alone). Either way the channel owns the note on that track */
static void midi_note_start(uint32_t ch, uint32_t note, uint32_t vel, track_t *t)
{
    uint32_t tr, root, n, i = MCHORD_N;
    uint8_t x[CHORD_MAX];
    int plain;
    midi_bend_to(ch, t);                                /* the bend the note starts with */
    if ((root = midi_map(t, note)) == KB_SILENT)
        return;
    n = chord_build(t, root, x);
    tr = trk_index(t);
    plain = n == 1u && x[0] == note;
    if (!plain) {
        for (i = 0; i < MCHORD_N && mchord[i].id; i++)
            ;
        if (i == MCHORD_N && root != note)
            return;
        plain = i == MCHORD_N;
    }
    if (plain) {
        if (!midi_note_held(t, note) && !midi_local_held(t, note))
            input_on(t, note, vel);
    } else {
        mchord_t *m = &mchord[i];
        m->ch = (uint8_t)ch;
        m->src = (uint8_t)note;
        m->n = 0;
        memcpy(m->note, x, n);
        n = chord_strum_begin(t, m->note, n, &m->strum, vel);
        m->strum.vel = (uint8_t)vel;
        m->id = (uint8_t)(tr + 1u);
        while (m->n < n)
            midi_chord_tone(m);
        if (m->strum.total)                             /* the rest follow (seq.c chord_strum_tick) */
            midi_strum_pending |= 1u << i;
    }
    midi_own[tr][ch][note >> 5] |= 1u << (note & 31u);
}

/* note-on: a repeat restarts, on the tracks the note plays now */
static void midi_note_on(uint32_t ch, uint32_t note, uint32_t vel)
{
    uint32_t parts, tr;
    midi_end_all(ch, note, midi_owners(ch, note));
    parts = midi_note_parts(ch, note);
    for (tr = 0; tr < NTRK; tr++)
        if ((parts >> tr) & 1u)
            midi_note_start(ch, note, vel, &trk[tr]);
}

/* every note of t released, the arp's held and latched ones too */
static void midi_release_all(track_t *t)
{
    trk_all_off(t);
    t->nheld = 0;
    t->arp_phys = 0;
    t->arp_note = 0;
}

/* a panic for one track: nothing owned before (MIDI notes, the pedal's, keys, strums) can end a note started after.
 * Its voices are the caller's */
static void midi_forget_track(uint32_t track)
{
    uint32_t k, ch, w;
    for (k = 0; k < KB_N; k++)
        if (kb_chn[KB_SLOT(k, track)])
            key_off(k, &trk[track]);
    for (k = 0; k < MCHORD_N; k++)
        if (mchord[k].id == track + 1u)
            midi_strum_pending &= ~(1u << k);
    memset(live_held[track], 0, sizeof live_held[track]);
    for (ch = 0; ch < 16u; ch++)
        for (w = 0; w < 4u; w++) {
            midi_kept[ch][w] &= ~midi_own[track][ch][w];
            midi_own[track][ch][w] = 0;
        }
    mchord_forget(track);
    midi_bend_target[track] = midi_bend_q8[track] = 0;
    trk[track].bend_raw = 0;
}

/* every channel as at power-on, no note owned (voices and the chord pool are the caller's) */
static void midi_input_reset(void)
{
    uint32_t ch;
    memset(midi_own, 0, sizeof midi_own);
    memset(midi_kept, 0, sizeof midi_kept);
    for (ch = 0; ch < 16u; ch++) {
        midi_chan[ch].bend = 0;
        midi_chan[ch].semi = 2;
        midi_chan[ch].cents = 0;
        midi_chan[ch].rpn[0] = midi_chan[ch].rpn[1] = 127;
        midi_chan[ch].pedal = 0;
    }
}

/* the MIDI notes track owns now, pressed or kept by a pedal */
static uint32_t midi_held_count(uint32_t track)
{
    uint32_t n = 0, ch, w, b;
    for (ch = 0; ch < 16u; ch++)
        for (w = 0; w < 4u; w++)
            for (b = midi_own[track][ch][w]; b; b &= b - 1u)
                n++;
    return n;
}

/* the controllers kept per channel: CC64 pedal, RPN 0 (CC101 / CC100 select, CC6 / CC38 data entry; an NRPN, CC99 /
 * CC98, deselects), CC120 All Sound Off, CC121 Reset All Controllers, CC123 All Notes Off. CC96 / CC97 (increment),
 * CC122 (local) and CC124..127 (omni, mono / poly) do nothing */
static void midi_control(uint32_t ch, uint32_t cc, uint32_t v)
{
    uint32_t mask, tr, i;
    switch (cc) {
    case 64:
        if (v >= 64u)
            midi_chan[ch].pedal = 1;
        else
            midi_pedal_up(ch);
        break;
    case 101:
    case 100:
        midi_chan[ch].rpn[101u - cc] = (uint8_t)v;
        break;
    case 99:
    case 98:
        midi_chan[ch].rpn[0] = midi_chan[ch].rpn[1] = 127;
        break;
    case 6:
    case 38:
        if (midi_chan[ch].rpn[0] || midi_chan[ch].rpn[1])
            break;
        if (cc == 6)
            midi_chan[ch].semi = (uint8_t)(v > 24u ? 24u : v);
        else
            midi_chan[ch].cents = (uint8_t)(v > 99u ? 99u : v);
        midi_bend_all(ch);                              /* the new range at once */
        break;
    case 120:                                           /* the pedal ignored, every voice faded in a block */
        mask = midi_targets(ch);
        for (tr = 0; tr < NTRK; tr++)
            if ((mask >> tr) & 1u) {
                midi_release_all(&trk[tr]);
                for (i = 0; i < NVOICE; i++)
                    if (trk[tr].v[i].active)
                        voice_kill(&trk[tr].v[i]);
                midi_forget_track(tr);
            }
        break;
    case 121:                                           /* the sensitivity stays (MIDI's recommended practice) */
        midi_chan[ch].bend = 0;
        midi_bend_all(ch);
        midi_chan[ch].rpn[0] = midi_chan[ch].rpn[1] = 127;
        midi_pedal_up(ch);
        break;
    case 123:                                           /* note-offs (the pedal keeps its notes); then a target left
                                                         * with no MIDI note or key down stops, its arp latch too */
        mask = midi_targets(ch);                        /* (the targets before the notes end) */
        for (i = 0; i < 128u; i++)
            midi_note_off(ch, i);
        for (tr = 0; tr < NTRK; tr++) {
            if (!((mask >> tr) & 1u) || midi_held_count(tr))
                continue;
            for (i = 0; i < KB_N && !(((KB_KEYS() >> i) & 1u) && ((kb_parts[i] >> tr) & 1u)); i++)
                ;
            if (i == KB_N)
                midi_release_all(&trk[tr]);
        }
        break;
    default:
        break;
    }
}

/* one channel voice message: st the status without its channel, ch 0..15, data 0..127. The controllers go to the
 * modulation matrix of every track the channel reaches and to the mapped parameters of the one it edits, first; poly
 * pressure and program change are not handled here */
static __attribute__((noinline)) void midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)
{
    if (st == 0x80u || (st == 0x90u && !d2)) {
        midi_note_off(ch, d1);
    } else if (st == 0x90u) {
        midi_note_on(ch, d1, d2);
    } else if (st == 0xB0u || st == 0xD0u) {
        uint32_t parts = midi_parts(ch), tr;
        track_t *t = midi_track(ch);
        for (tr = 0; tr < NTRK; tr++)
            if ((parts >> tr) & 1u)
                mod_midi(&trk[tr], st, d1, d2);
        if (st == 0xB0u) {
            if (t)
                midi_parameter(t, d1, d2);
            midi_control(ch, d1, d2);
        }
    } else if (st == 0xE0u) {
        midi_chan[ch].bend = (int16_t)((int32_t)(d1 | d2 << 7) - 8192);
        midi_bend_all(ch);
    }
}
