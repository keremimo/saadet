# Clean-room specification: MIDI performance input

This is the behavioural specification for Saadet's MIDI performance input. The clean implementation is written
from this document, the MIDI 1.0 standard and the current Saadet sources that are present in the working tree.
It describes what the code must do, not how an earlier implementation did it. The process and its record are in
[PROCESS.md](PROCESS.md).

Terms: "track" is Saadet's synth part (`track_t`, `trk[]`; `NTRK` tracks, currently one, `TSEL` the selected
one). The code must work for any `NTRK` below 8. "Q8 semitones" means 256 units per semitone.

## 1. What is to be written

1. A new file `firmware/src/midi_input.c`, licensed MPL-2.0, `Copyright (C) 2026 Kerem Kilic (Ellic Studio)`.
   `seq.c` includes it right after `midi_param.c` (both includes are already in place). It is compiled as part of
   the same translation unit as `seq.c`, `chord.c` and `voice.c`, so it uses their `static` functions and data
   directly and defines its own as `static`.
2. In `firmware/src/voice.c`: the MIDI pitch-bend state, its smoothing, `p5_update_bend`, and adding the bend to
   each voice's pitch (section 5). The places are marked `CLEANROOM` in the file.

Nothing else needs changing, apart from what the build or the tests show is missing for these two parts.

## 2. The surroundings (read their definitions and comments)

| Name | Where | Meaning |
| --- | --- | --- |
| `midi_track(ch)` | seq.c | The track a channel's messages reach (receive-channel routing), or NULL. |
| `midi_note_track(ch, note)` | seq.c | The track a note-on reaches, or NULL. |
| `midi_map(t, note)` | seq.c | The pitch a MIDI note plays through the scale layouts; `KB_SILENT` = plays nothing. |
| `input_on(t, note, vel)` / `input_off(t, note)` | seq.c | Start or end a live note on a track (arp, latch and live state included). `input_off` does nothing while `midi_note_held(t, note)` is true. |
| `chord_build(t, root, out)` | chord.c | The pitches a note plays as a chord (1 to `CHORD_MAX`); one pitch when chords are off. |
| `mchord[MCHORD_N]`, `mchord_t` | chord.c | Pool of MIDI notes playing a chord or a remapped pitch: `id` (track + 1, 0 = free), `ch`, `src` (the incoming note), `n` (tones started so far), `note[]`, `strum`. |
| `mchord_of(ch, src, id)`, `mchord_held(id, note)`, `mchord_forget(track)` | chord.c | Find a pool entry; whether a track's MIDI chords sound a pitch; drop a track's entries. |
| `chord_strum_begin(t, notes, n, &strum, vel)` | chord.c | Prepares a strum and returns how many tones start now. The rest follow later while the entry's bit in `midi_strum_pending` is set (chord.c advances them). |
| `midi_chord_tone(m)` | seq.c | Starts the next tone of entry `m` (and counts it in `m->n`). |
| `kb_chn[k]`, `kb_trk[k]`, `kb_chord[k][]`, `key_off(k, t)` | seq.c | Physical key `k` (0..26): how many tones it sounds, on which track, which pitches; release it. |
| `fm1_in.notes` | HAL | Bitmask of the physical keys held down now. |
| `live_held[track][]` | seq.c | The track's live-note bitmap. |
| `trk_all_off(t)`, `voice_kill(v)`, `t->v[i].active`, `NVOICE` | voice.c | Release all of a track's notes normally; fade one voice out within one block. |
| `t->nheld`, `t->arp_phys`, `t->arp_note` | core.h | The track's held/arp input state. |
| `mod_midi(t, st, d1, d2)` | mod.c | The modulation matrix's MIDI sources (CC, pressure). |
| `midi_parameter(t, cc, value)` | midi_param.c | Controllers mapped to sound parameters. |
| `t->bend_raw` | core.h | The track's current channel pitch bend as sent, signed 14-bit (-8192..8191). FM6 applies its own range from it. |
| `t->eng_req`, `ENGI_PROPHET`, `p5_patch_of(t)->raw[P5_BEND]` | — | The Prophet engine and its patch's bend range: raw 0..11 means 1..12 semitones. |

## 3. Interface other code relies on

These names and signatures are used by `seq.c`, `chord.c`, `platform/instrument.c` and the tests:

- `static void midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)`: one channel voice message.
  `st` is the status with the channel removed (0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0 or 0xE0), `ch` 0..15, data
  0..127. It runs in the audio interrupt's control step, once per message, so it must not be slow or recurse
  deeply. Mark it `__attribute__((noinline))` to keep it out of the rendering loops.
- `static int midi_note_held(const track_t *t, uint32_t note)`: whether a MIDI note owned by track `t` sounds
  pitch `note` now. That is either a note played as itself (owned, including one kept only by the sustain pedal,
  and not playing a pool entry) or a tone of one of the track's MIDI chords (`mchord_held`). `seq.c` declares it
  ahead of `input_off`.
- `static int midi_local_held(const track_t *t, uint32_t note)`: whether a physical key on track `t` sounds
  `note`. True if, for some key `k`, the track's slot `s = KB_SLOT(k, trk_index(t))` has `note` among the first
  `kb_chn[s]` entries of `kb_chord[s]`.
- `static void midi_forget_track(uint32_t track)`: a panic for one track (preset change, project load, a lost
  note-off). See section 4.8.
- `static void midi_input_reset(void)`: all channels back to their power-on state and no MIDI note owned by any
  track (after an input-queue overflow, and at start-up). It does not touch voices or the chord pool (callers do).
- `static uint32_t midi_held_count(uint32_t track)`: how many MIDI notes (pressed, or kept by the pedal) the
  track owns now.

## 4. Behaviour

### 4.1 Channels

The 16 MIDI channels each have their own state, shared by every input port (USB, TRS and Bluetooth arrive the
same way). The power-on state is: pitch bend centred; bend sensitivity 2 semitones and 0 cents; sustain pedal up;
no registered parameter selected (the null RPN, 127/127). This state is runtime only and never saved.

A channel "targets" the tracks it routes to now (`midi_parts(ch)`, a bit per track: LAYER and SPLIT reach both),
plus every track on which it still owns notes. Controllers that affect sound (pitch bend, sensitivity, the channel mode messages) act on all
of a channel's targets. Channel state always updates even when the channel routes nowhere. In particular,
releasing the pedal must end the channel's pedal-held notes even after the routing has changed.

### 4.2 Note on (0x90 with velocity above 0)

1. If the channel already owns that note number on any track, including one kept only by the pedal, end it first
   as in 4.3, ignoring the pedal. Then continue: a repeated note restarts.
2. The destinations are `midi_note_parts(ch, note)`, a bit per track. If there are none, stop here. Steps 3 to 6 run
   for each.
3. Bring the destination's bend up to date with the channel's (4.5) before starting the note.
4. Map the note with `midi_map`. If the result is `KB_SILENT`, nothing sounds and nothing is owned.
5. Build its pitches with `chord_build`.
   - If that is exactly the incoming note (one pitch, equal to the note number), it is a plain note. Start it
     with `input_on`, unless that pitch already sounds on the track from another MIDI note (`midi_note_held`) or
     a physical key (`midi_local_held`). In that case, do not start it again, but still take ownership (step 6).
   - Otherwise (a chord, or one remapped pitch), it needs a free pool entry (`id == 0`). If there is none and
     the mapped pitch equals the note number, play it as a plain note. If there is none otherwise, play nothing
     and own nothing. With an entry: record the channel, the source note, the pitches and `n = 0`, prepare the
     strum with the note's velocity (also keep the velocity in `strum.vel`), set `id`, start the tones
     `chord_strum_begin` returns with `midi_chord_tone`, and set the entry's bit in `midi_strum_pending` when
     the strum has more tones to come (`strum.total` non-zero).
6. The channel now owns that note number on the destination track.

Velocity is passed through as received. Note-on with velocity 0 is a note-off.

### 4.3 Note off (0x80, or 0x90 with velocity 0)

If the channel does not own the note number, ignore it. If the channel's pedal is down, the note stays owned
and is marked as kept by the pedal (4.4). Otherwise end it on every track it owns it on:

1. Drop the ownership first, because `input_off` consults `midi_note_held`.
2. If a pool entry belongs to that channel, source note and track: clear its bit in `midi_strum_pending` (tones
   not yet strummed never start), free it (`id = 0`), and end each tone it started (the first `n` pitches).
   Otherwise end the plain note's pitch.
3. Ending a pitch means `input_off` on the track, unless a physical key on that track sounds the same pitch
   (`midi_local_held`). `input_off` itself keeps a pitch another MIDI note still holds.

Release velocity is ignored.

### 4.4 Sustain pedal (CC64)

A value of 64 or more is pedal down, below 64 is pedal up (MIDI 1.0's on/off switch convention). While the
pedal is down, note-offs keep their notes (4.3). Pedal up ends every note of that channel that is kept by the
pedal, as in 4.3. Notes whose keys are still pressed are not affected. A repeated pedal-down changes nothing.

### 4.5 Pitch bend (0xE0)

The 14-bit value is `d1 | d2 << 7` (LSB first, per MIDI 1.0); 8192 is centre. Keep it per channel as a signed
value from -8192 to +8191. Apply the channel's bend to each target track:

- Set the track's `bend_raw` to the signed value.
- Set the track's bend target (section 5) in Q8 semitones. The range is the channel's sensitivity
  (semitones and cents, 4.6) in Q8; for the Prophet engine (`eng_req == ENGI_PROPHET`), it is the patch's own
  range (`P5_BEND` raw, clamped to 0..11, plus 1, in semitones). The mapping is linear with integer
  arithmetic: centre gives exactly 0, +8191 gives exactly +range and -8192 gives exactly -range. Each side is
  scaled by its own full-scale value, so the two directions are exact.

### 4.6 Registered parameters: bend sensitivity

CC101 selects the RPN MSB, CC100 the LSB. CC6 (data entry MSB) and CC38 (data entry LSB) change the selected
parameter. Only RPN 0/0, pitch bend sensitivity, is supported: CC6 sets the semitones (values above 24 count as
24) and CC38 the cents (values above 99 count as 99). After either changes, apply the bend again (4.5) so the new
range takes effect at once. Selecting an NRPN (CC99 or CC98) cancels the RPN selection, as does the null RPN
127/127, and data entry is then ignored. Data increment and decrement (CC96, CC97) are not supported. The
sensitivity stays until changed again or until `midi_input_reset`.

### 4.7 Channel mode messages

- **CC120, All Sound Off.** For every target track: release all its notes (`trk_all_off`), clear `nheld`,
  `arp_phys` and `arp_note`, fade every active voice within one block (`voice_kill`, whatever its release
  time), then `midi_forget_track` (4.8). The pedal is ignored.
- **CC123, All Notes Off.** End each note the channel owns as a note-off (4.3), so the pedal is honoured and
  pedal-kept notes stay. Then, for every target track that has no MIDI-owned notes left and no physical key
  held down for it (`fm1_in.notes` bit `k` with `kb_trk[k]` on that track): `trk_all_off` and clear `nheld`,
  `arp_phys` and `arp_note`. Latched arp notes then stop as well.
- **CC121, Reset All Controllers.** Pitch bend to centre (applied to the targets), RPN to null, pedal up
  (ending pedal-kept notes). The bend sensitivity is kept, as MIDI's recommended practice for this message
  says.
- CC122 (local control) and CC124 to CC127 (omni and mono/poly modes) are ignored.

### 4.8 Forgetting a track (`midi_forget_track`)

After this, no note-off, pedal-up or All Notes Off from any channel may end a note started on the track
afterwards. It:

- releases every physical key on the track that sounds tones (`key_off` for each key with `kb_trk` on the track
  and `kb_chn > 0`);
- clears the strum bits of the track's pool entries;
- clears `live_held` for the track;
- removes every MIDI note the track owns, on every channel, and the track from every channel's targets;
- drops the track's pool entries (`mchord_forget`);
- sets the track's bend target, smoothed bend and `bend_raw` to 0.

It does not stop voices: its callers do.

### 4.9 Other messages

For each controller (0xB0) on a channel that routes to a track: first `mod_midi(t, 0xB0, cc, value)`, then
`midi_parameter(t, cc, value)`, then the handling above. Channel pressure (0xD0) on a routed channel goes to
`mod_midi(t, 0xD0, d1, d2)`. Polyphonic pressure (0xA0) and program change (0xC0) are not handled here. CC1 (mod
wheel) needs nothing beyond `mod_midi`: it does not add vibrato of its own.

## 5. Pitch bend in voice.c

- State: `static int32_t midi_bend_target[NTRK], midi_bend_q8[NTRK];` in Q8 semitones, the target set by 4.5 and
  the smoothed value the voices use. Tests and `midi_forget_track` access both by these names.
- Smoothing: once per audio block (`track_render_audio` runs once per track per block, also for a track without
  voices), move the smoothed value towards the target. Blocks are `CTL` samples at 44.1 kHz. It should follow
  with a time constant of a few milliseconds, so a wheel movement isn't stepped but still feels immediate. It
  must use integers only, and arrive at the target exactly, without a lasting offset of a unit or two: a jump
  across the whole ±24-semitone range lands exactly within about 30 ms. The first block after a jump must
  already move, but not all the way.
- `static void p5_update_bend(track_t *t)` (declared ahead in `eng_prophet_test.c` and called by the Prophet
  engine every block): recompute the track's target from `bend_raw` with the Prophet patch range, as in 4.5. When
  `MELODEE_CONTROL_FAST` is non-zero and `bend_raw` is 0, set the target to 0 without reading the patch.
- Applying it: for each sounding voice, add the smoothed bend to the voice's pitch in the representation TUNE
  already uses in the same function. Whole 1/16 semitones go into the pitch (with TUNE, before the clamp to the
  table), and the remainder below 1/16 semitone goes into `m.fine`. `m.fine` is in units of 1/4096 of the
  frequency ratio, about 2.367 units per cent; one Q8 unit is 100/256 cents. Negative bends must split the same
  way TUNE's negative cents do, so that the whole and fine parts always add up to the bend.
- The offset `m.plog` handed to engines that compute their own pitch (FM6) must exclude the bend, exactly as it
  excludes TUNE: FM6 applies its own bend range from `bend_raw`.

## 6. Acceptance

`tests/midi_input_test.c` checks this specification through `midi_event` and the track's observable state. It
must pass, together with the existing host tests listed in [PROCESS.md](PROCESS.md).
