/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Acceptance of docs/cleanroom/midi-input-spec.md through midi_event and the track's observable state: routing,
 * repeated notes, sustain, pitch bend and its sensitivity (RPN 0), the channel mode messages, forgetting a track,
 * chords and the bend's smoothing and pitch. Section numbers refer to the specification. */
#include <assert.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"

static void msg(uint32_t status, uint32_t d1, uint32_t d2) { midi_event(status & 0xF0u, status & 15u, d1, d2); }
static void cc(uint32_t ch, uint32_t n, uint32_t v) { msg(0xB0u | ch, n, v); }
static void bend(uint32_t ch, int32_t v) { uint32_t u = (uint32_t)(v + 8192); msg(0xE0u | ch, u & 127u, u >> 7); }
static void tick(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
static uint32_t held(void) { uint32_t n = 0; for (uint32_t i = 0; i < 4u; i++) n += __builtin_popcount(live_held[0][i]); return n; }
static uint32_t voices(void) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += TSEL->v[i].active != 0; return n; }
static void fresh(uint32_t engine)
{
    power_on();
    host_preset(TSEL, engine, 0);
    for (uint32_t i = 0; i < 400u; i++) tick();     /* (an engine change completes in the audio blocks) */
}
/* sign changes of the left channel over n blocks: a rough frequency */
static uint32_t crossings(uint32_t blocks)
{
    uint32_t n = 0; int32_t prev = 0;
    for (uint32_t b = 0; b < blocks; b++) {
        int32_t out[CTL * 2]; mix_block(out, CTL);
        for (uint32_t i = 0; i < CTL; i++) { int32_t s = out[2 * i]; if ((s > 0) != (prev > 0)) n++; prev = s; }
    }
    return n;
}

static void notes_and_routing(void)                 /* 4.1, 4.2, 4.3 */
{
    fresh(ENGI_J106);
    for (uint32_t ch = 0; ch < 16u; ch++) { msg(0x90u | ch, 60, 100); assert(held() == 1 && midi_held_count(0) == 1); msg(0x80u | ch, 60, 0); assert(!held() && !midi_held_count(0)); }
    msg(0x90, 62, 100); msg(0x90, 62, 0); assert(!held());          /* velocity 0 is a note-off */
    msg(0x90, 64, 100); msg(0x90, 64, 90); assert(held() == 1 && midi_held_count(0) == 1);   /* a repeat restarts */
    msg(0x80, 64, 0); assert(!held() && !midi_held_count(0));
    msg(0x80, 65, 0); assert(!held());                              /* an unowned note-off: nothing */
    song.g[G_ROUTE] = 6; msg(0x90, 64, 100); assert(!held()); msg(0x95, 64, 100); assert(held() == 1);
    song.g[G_ROUTE] = 2; msg(0x85, 64, 0); assert(!held()); song.g[G_ROUTE] = 0;   /* owned: ends after re-routing */
    msg(0x90, 60, 100); msg(0x91, 60, 100); assert(held() == 1 && midi_held_count(0) == 2 && midi_note_held(TSEL, 60));
    msg(0x80, 60, 0); assert(held() == 1 && midi_note_held(TSEL, 60));   /* the other channel still holds it */
    msg(0x81, 60, 0); assert(!held() && !midi_note_held(TSEL, 60));
    puts("MIDI input: routing, repeats and shared pitches");
}

static void sustain(void)                            /* 4.4 */
{
    fresh(ENGI_J106);
    msg(0x90, 60, 100); cc(0, 64, 127); msg(0x80, 60, 0); assert(held() == 1 && midi_held_count(0) == 1);
    cc(0, 64, 100); assert(held() == 1);                             /* a repeated pedal-down: nothing */
    msg(0x90, 62, 100); cc(0, 64, 0); assert(held() == 1 && midi_note_held(TSEL, 62) && !midi_note_held(TSEL, 60));
    msg(0x80, 62, 0); assert(!held());
    cc(0, 64, 64); msg(0x90, 60, 100); msg(0x80, 60, 0); msg(0x90, 60, 100); assert(held() == 1 && midi_held_count(0) == 1);
    cc(0, 64, 63); assert(held() == 1);                              /* pressed again: the key holds it */
    msg(0x80, 60, 0); assert(!held());
    cc(1, 64, 127); msg(0x91, 67, 100); msg(0x81, 67, 0); cc(0, 64, 0); assert(held() == 1);   /* per channel */
    song.g[G_ROUTE] = 9; cc(1, 64, 0); assert(!held()); song.g[G_ROUTE] = 0;   /* pedal-up after re-routing */
    puts("MIDI input: sustain per channel, repeats, keys still down, re-routing");
}

static void pitch_bend(void)                         /* 4.5, 4.6 */
{
    fresh(ENGI_J106);
    bend(0, 8191); assert(midi_bend_target[0] == 2 * 256 && TSEL->bend_raw == 8191);   /* routed, no note needed */
    bend(0, -8192); assert(midi_bend_target[0] == -2 * 256 && TSEL->bend_raw == -8192);
    bend(0, 0); assert(midi_bend_target[0] == 0 && TSEL->bend_raw == 0);
    bend(0, 4096); assert(midi_bend_target[0] >= 255 && midi_bend_target[0] <= 257);
    cc(0, 101, 0); cc(0, 100, 0); cc(0, 6, 12); cc(0, 38, 0); assert(midi_bend_target[0] >= 6 * 256 - 1 && midi_bend_target[0] <= 6 * 256 + 1);   /* re-applied at once */
    bend(0, 8191); assert(midi_bend_target[0] == 12 * 256);
    cc(0, 38, 50); assert(midi_bend_target[0] == 12 * 256 + 128);
    cc(0, 38, 120); assert(midi_bend_target[0] == 12 * 256 + 99 * 256 / 100);
    cc(0, 6, 30); cc(0, 38, 0); assert(midi_bend_target[0] == 24 * 256);   /* 24 at most */
    cc(0, 99, 1); cc(0, 6, 3); assert(midi_bend_target[0] == 24 * 256);    /* an NRPN cancels data entry */
    cc(0, 101, 0); cc(0, 100, 0); cc(0, 6, 3); assert(midi_bend_target[0] == 3 * 256);
    cc(0, 101, 127); cc(0, 100, 127); cc(0, 6, 9); assert(midi_bend_target[0] == 3 * 256);   /* the null RPN */
    cc(0, 101, 0); cc(0, 100, 0); cc(0, 96, 1); cc(0, 97, 1); assert(midi_bend_target[0] == 3 * 256);   /* no increment */
    bend(1, 8191); assert(midi_bend_target[0] == 2 * 256);           /* each channel its own sensitivity */
    song.g[G_ROUTE] = 3; bend(0, -8192); assert(midi_bend_target[0] == 2 * 256); song.g[G_ROUTE] = 0;   /* not routed */
    fresh(ENGI_PROPHET);
    for (uint32_t r = 0; r < 12u; r++) {             /* the Prophet patch's own range */
        p5_patch_of(TSEL)->raw[P5_BEND] = (uint8_t)r;
        bend(0, 8191); assert(midi_bend_target[0] == (int32_t)(r + 1) * 256);
        bend(0, -8192); assert(midi_bend_target[0] == -(int32_t)(r + 1) * 256);
    }
    bend(0, 0); assert(midi_bend_target[0] == 0);
    puts("MIDI input: 14-bit bend, RPN 0 sensitivity, NRPN and null RPN, the Prophet range");
}

static void mode_messages(void)                      /* 4.7 */
{
    fresh(ENGI_J106);
    cc(0, 101, 0); cc(0, 100, 0); cc(0, 6, 7);
    msg(0x90, 60, 100); cc(0, 64, 127); msg(0x80, 60, 0); bend(0, 8191);
    cc(0, 121, 0);                                    /* Reset All Controllers */
    assert(!held() && midi_bend_target[0] == 0 && TSEL->bend_raw == 0);
    bend(0, 8191); assert(midi_bend_target[0] == 7 * 256);           /* sensitivity kept */
    cc(0, 6, 1); assert(midi_bend_target[0] == 7 * 256);             /* RPN back to null */
    bend(0, 0);
    msg(0x90, 60, 100); msg(0x90, 64, 100); cc(0, 64, 127); msg(0x80, 64, 0);
    cc(0, 123, 0);                                    /* All Notes Off: the pedal keeps them */
    assert(held() == 2 && midi_held_count(0) == 2);
    cc(0, 64, 0); assert(!held());
    msg(0x90, 60, 100); cc(0, 123, 0); assert(!held() && !midi_held_count(0) && TSEL->nheld == 0);
    msg(0x90, 60, 100); msg(0x90, 67, 100); cc(0, 64, 127); msg(0x80, 67, 0);
    for (uint32_t i = 0; i < 20u; i++) tick();
    assert(voices());
    cc(0, 120, 0);                                    /* All Sound Off: ignores the pedal, voices gone */
    assert(!held() && !midi_held_count(0) && TSEL->nheld == 0);
    for (uint32_t i = 0; i < VOICE_FADE_BLOCKS + 2u; i++) tick();
    assert(!voices());
    msg(0x91, 60, 100); msg(0x80, 60, 0); cc(0, 64, 0); assert(held() == 1);   /* old owners are gone */
    msg(0x81, 60, 0); assert(!held());
    cc(0, 122, 0); cc(0, 124, 0); cc(0, 127, 0);     /* ignored */
    puts("MIDI input: reset all controllers, all notes off, all sound off");
}

static void forget_and_reset(void)                   /* 4.8, 3 */
{
    fresh(ENGI_J106);
    msg(0x90, 60, 100); cc(0, 64, 127); msg(0x80, 60, 0); bend(0, 8191);
    for (uint32_t i = 0; i < 20u; i++) tick();
    midi_forget_track(0); trk_all_off(TSEL);
    assert(!midi_held_count(0) && !midi_note_held(TSEL, 60) && midi_bend_target[0] == 0 && midi_bend_q8[0] == 0 && TSEL->bend_raw == 0);
    msg(0x91, 60, 100); cc(0, 64, 0); msg(0x80, 60, 0); assert(held() == 1);   /* a later start survives */
    msg(0x81, 60, 0); assert(!held());
    cc(2, 101, 0); cc(2, 100, 0); cc(2, 6, 11); cc(2, 64, 127);
    midi_input_reset();
    bend(2, 8191); assert(midi_bend_target[0] == 2 * 256);           /* back to 2 semitones */
    msg(0x92, 60, 100); msg(0x82, 60, 0); assert(!held());           /* and the pedal up */
    puts("MIDI input: forgetting a track, resetting every channel");
}

static void chords(void)                             /* 4.2 step 5 */
{
    fresh(ENGI_J106);
    TSEL->p[P_CHRD] = 3; TSEL->p[P_STRUM] = 0;
    msg(0x90, 60, 100); assert(held() == 3 && midi_held_count(0) == 1);
    msg(0x80, 60, 0); assert(!held() && !midi_held_count(0));
    TSEL->p[P_STRUM] = 3; msg(0x90, 60, 100); assert(held() == 1 && midi_strum_pending);
    msg(0x80, 60, 0); for (uint32_t i = 0; i < 200u; i++) tick();
    assert(!held() && !midi_strum_pending);           /* tones not yet strummed never start */
    TSEL->p[P_STRUM] = 0;
    for (uint32_t i = 0; i < MCHORD_N; i++) msg(0x90u | (i & 15u), 36 + i, 100);   /* the pool full */
    uint32_t before = midi_held_count(0);
    msg(0x9F, 100, 100); assert(midi_held_count(0) == before + 1 && midi_note_held(TSEL, 100));   /* the note alone */
    msg(0x8F, 100, 0);
    for (uint32_t i = 0; i < MCHORD_N; i++) msg(0x80u | (i & 15u), 36 + i, 0);
    assert(!held() && !midi_held_count(0));
    puts("MIDI input: chords, strums cancelled by the note-off, a full chord pool");
}

static void bend_in_the_voices(void)                 /* 5 */
{
    fresh(ENGI_J106);
    midi_bend_target[0] = 12 * 256; tick();
    assert(midi_bend_q8[0] > 0 && midi_bend_q8[0] < 12 * 256);       /* smoothed */
    for (uint32_t i = 0; i < 40u; i++) tick();
    assert(midi_bend_q8[0] == 12 * 256);              /* lands exactly within ~30 ms */
    midi_bend_target[0] = -7 * 256 - 3; for (uint32_t i = 0; i < 40u; i++) tick();
    assert(midi_bend_q8[0] == -7 * 256 - 3);
    midi_bend_target[0] = midi_bend_q8[0] = 0;
    fresh(ENGI_PROPHET);
    p5_patch_of(TSEL)->raw[P5_BEND] = 11;            /* +/-12 semitones */
    msg(0x90, 48, 100); for (uint32_t i = 0; i < 100u; i++) tick();
    uint32_t base = crossings(400);
    bend(0, 8191); for (uint32_t i = 0; i < 40u; i++) tick();
    uint32_t up = crossings(400);
    bend(0, -8192); for (uint32_t i = 0; i < 40u; i++) tick();
    uint32_t down = crossings(400);
    printf("crossings: %u at centre, %u an octave up, %u an octave down\n", base, up, down);
    assert(base && up * 10u > base * 17u && up * 10u < base * 23u);  /* about twice */
    assert(down * 10u > base * 4u && down * 10u < base * 6u);        /* about half */
    puts("MIDI input: smoothing that lands exactly, the bend in the voices' pitch");
}

int main(void)
{
    notes_and_routing();
    sustain();
    pitch_bend();
    mode_messages();
    forget_and_reset();
    chords();
    bend_in_the_voices();
    puts("MIDI INPUT: the clean-room specification passed");
    return 0;
}
