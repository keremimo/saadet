# Saadet Elite / Pro: the SMK-37's own UX

The design for Saadet on the SMK-37 (`SAADET_SMK37`; the port in [SMK37_PORT.md](SMK37_PORT.md)): what the 16 RGB
pads, the knob and fader banks, the four faders, the two wheels and the pedal do on a one-part synth with no
sequencer, so the controls the keyboard adds carry real functionality from the first release. The FM-1 build is
untouched by any of it.

Status (2026-11-05): built (`firmware/src/ui_smk.c`), covered by the host tests. Keys, buttons, knobs, pads, faders and BANK
buttons are confirmed on an Elite; the pad bank is built and waits for that. Mocks: `docs/smk37-ux/mocks.html`
(composed in HTML on the screen's own geometry, the pads beside each screen). Where the build differs from the first
design, the text below says what was built.

Where Melodee's design (`Felucca/docs/SMK37_UX.md`) had tracks, patterns, steps and drums, Saadet has one sound,
four engines, a chord engine, an arp, scales and a library. The grammar is kept so a player who owns both learns it
once; the content is Saadet's.

## 1. The model

Four things are in play at any moment, each with a default that follows the page, and an override:

| Axis | Values | Default | Picked by |
| --- | --- | --- | --- |
| Place | as the FM-1 (HOME and the ten places, their pages) | - | the place buttons, SELECT, `<` `>` |
| Pad bank | CHORDS, SOUNDS, FAVS, PARTS | HOME: CHORDS until chosen, then what was chosen; a page: its own (section 2.4) | KNOB BANK + FADER BANK together: tap (next), held (picker) |
| Knob bank (K1..K4) | PAGE, MACROS, FILTER, ENV, LFO, ARP, CHORD, MIX | PAGE | KNOB BANK tap (next), held (picker) |
| Fader bank (faders 1..4) | the same eight | ENV | FADER BANK tap (next), held (picker) |

The pads, the knobs and the faders are independent. Nothing in the FM-1's navigation changes: a button opens its
place, again turns its page, SELECT turns it, holding lists what is inside. Momentary states (a button held, a BANK
button held, SAVE's name) take the pads for as long as they last and give them back.

What stays as Melodee's SMK37.md has it: K1..K4 = the four values at the foot of the screen, K5 MASTER, K6 SELECT,
K7 ALGORITHM, K8 PRESETS; the page buttons by the printed label (FX = LIBRARY, SCALE, CHORD = ENV, NOTE REPEAT =
LFO, PARA = EDIT, GLOBE = GLOBAL, PATCH = HOME, BT = SAVE, ARP, SEQ = CHORD, SEQ PLAY = PLAY, SEQ REC = latch,
OCTAVE -/+); the keybed (the middle 27 as the FM-1's, the outer 10 more notes); the power-on combos. The printed
names fit Saadet's places better than they fit Melodee's: CHORD opens the envelopes and SEQ the chords, which is
odd, so the screen names the place as it always did and the manual gives the map once.

The one hue: the engine's colour (`T_THEME`: coral P5, blue FM6, yellow CZ-1, lilac J106). The screen already takes
it on; the pads say the same thing the screen says.

## 2. Pads

### 2.1 Colour grammar

Five states, told apart by colour and level, never by level alone (`legend` in the mocks):

| State | Light | Meaning |
| --- | --- | --- |
| OFF | dark ember | nothing there, or silent: a degree outside the scale, an empty slot, a bank the engine lacks |
| ON | the engine's colour, dim | something there: a degree in the scale, a sound in the list, an entry that can be pressed |
| FILLED | the engine's colour, full | the one picked or holding: the scale's root, the sound playing, the current page, the chord type in use |
| ACTIVE | white-blue | happening now: a pad held, a note sounding, a chord struck |
| WAITING | orange, breathing | a dump waiting in SAVE, DELETE and ERASE armed (the confirmation breathes on the pads as the FX button breathes today) |

Where pads stand for sounds of several engines (FAVOURITES, USER) each pad takes its sound's engine hue. A pad with
no function in the current map is OFF at a lower level. No blinking outside WAITING.

### 2.2 The modes

The pads never double the keybed: there is no KEYS mode (decided 2026-11-04; the keys are the keys, with 37 of
them, and a second keyboard on the pads would be the one thing the FM-1 already had). The pads do what the keys
cannot: chords and sounds.

**CHORDS** (`home_chords`, `chord_pads`, `j106_env_faders`): the chord engine on the pads, the pads' default
wherever you play. Bottom row: the eight degrees of the scale (I .. VII, then
I an octave up), each playing the CHORD place's chord on that degree, through its voicing and strum: with DIA3 or
DIA7 the diatonic triad or seventh of the degree, with MAJ .. POW that fixed chord on the degree's root. Top row:
the eight chord types in the order of the CHORD place (OFF DIA3 DIA7 MAJ MIN DOM7 MAJ7 MIN7; SUS4 and POW stay on
K1): a tap sets the type, the one in use FILLED, the others ON. Lights on the bottom row: every degree ON, the root
degree FILLED, a struck chord ACTIVE while it sounds; a degree outside the scale (KEYS OFF) OFF. Velocity goes to the
chord. Pressure is strum speed (decided 2026-11-04): a chord struck and leaned on strums faster, its STRUM rate
scaled by the pressure from the rate set up to the next faster one; with STRUM OFF pressure does nothing. The pads
send no aftertouch. The keys keep playing single notes (or the chord, as CHORD is set) at the same time: a left-hand pad
chord and a right-hand line. With CHORD OFF the bottom row plays the degree's single root note, so the pads still
give the scale under the hand without being a keyboard.

**PARTS** (built after the mocks): pads 1 and 2 pick part A or B, in their engines' colours, the one you are on FILLED; pads 5..8 the
modes ONE, MULTI, SPLIT and LAYER, the current FILLED; the bottom row the first eight performances, in part A's engine colour, the
one loaded FILLED. An empty slot, or part B with one part, is dark and does nothing.

**SOUNDS** and **FAVS** (`library_sounds`, `patch_held_favs`): 16 sounds. In LIBRARY, of the list on the screen (FAVOURITES,
USER, P5, FM6, CZ-1, J106). Elsewhere SOUNDS is the engine playing's own list and FAVS the favourites, opening on the page of
the sound playing. A page of 16, pad 1 top left to pad 16 bottom right; `<` `>` turn the list's page
of 16 and the screen's chosen sound follows, without loading it. SELECT still turns LIBRARY's lists, as on the FM-1. A tap loads the sound at once (no rest wait: a tap is a
choice). Lights: a slot with a sound ON (FAVOURITES and USER: in its engine's hue), the sound playing FILLED, a pad
held ACTIVE, an empty slot OFF. Starred sounds are not told apart on the pads: the screen's disc does that.

### 2.3 Momentary maps

These take the pads for as long as they last, whatever the mode, and give them back:

| While | Pads | Screen |
| --- | --- | --- |
| a place button held (ENV, LFO, EDIT, GLOBAL, LIBRARY, SCALE, ARP, CHORD, PLAY) | the place's list: its objects or pages as the FM-1 lights them on the keys, entry 1 top left; the current FILLED, the rest ON, beyond the count OFF. A tap jumps there and the button may be let go. The keys play on under a held button (on the FM-1 they choose; here the pads do) | the held list as today, the row numbers 1..16 the pads' (`env_held`); the keys strip at the foot becomes a pads strip |
| PATCH held | the favourites, 16 at a time, each in its engine's hue, the playing one FILLED (`patch_held_favs`) | FAVOURITES as today |
| SCALE held | a piano: top row C4 .. C5, bottom row the black keys between (the two gaps OFF); the root FILLED, the scale's notes ON, the rest OFF; a tap makes that note the root (`scale_held`) | the SCALE place's list as today |
| ARP held | the pedal (as the FM-1): the pads keep their mode; chords struck are sustained | as today |
| BT (SAVE) with a dump waiting, DELETE armed, ERASE armed | every pad WAITING until the press confirms or the arming cancels | as today |
| SAVE's name | pad 1..16: the letter groups ABC DEF GHI JKL MNO PQR STU VWX YZ 123 456 789 0.# &+/ - and space: a tap sets the letter under the cursor, another within a second cycles it; K2 moves the cursor | SAVE as today |
| KNOB BANK or FADER BANK held | top row the eight banks (the current ACTIVE, one the engine lacks OFF), the bottom row dark; a tap picks (`pick_banks`) | the bank picker (section 3) |
| KNOB BANK + FADER BANK held | the first four pads CHORDS SOUNDS FAVS PARTS (the current ACTIVE), the rest dark; a tap picks (`pick_pads`) | the pad bank, as a list (section 2.4) |

### 2.4 The pad bank: what the pads are

KNOB BANK and FADER BANK pressed together are the pad bank, whichever came first and whatever it had begun. Held: the list on the
screen and on the first four pads, opening at once; a pad, K1 or `<` `>` picks. Tapped: the next. Until both are up neither is a
BANK button, so neither bank moves. Letting go keeps what is shown (`pick_pads`).

| Place | The pads |
| --- | --- |
| HOME | The pad bank chosen on HOME: CHORDS until then. It stays, through power-off (favourites row 15, byte 25) |
| LIBRARY | SOUNDS, unless the pad bank is chosen there |
| Every other page | CHORDS, unless the pad bank is chosen there |

Decided 2026-11-05: the player chooses what the pads do on HOME (Kerem). A bank chosen on a page lasts until the page changes, as
the knobs' does, so HOME's choice never takes a page's own pads away. There is no AUTO to pick any more: this replaces the BANK
pickers' bottom row. The header carries the name as a tag while the pads differ from the place's own (`pads_override`: state, not
a message; left of the engine's tag, the label face, the engine's colour; the octave pill gives way).

### 2.5 What leaves the keys

The SMK's keys have no lights. Everything the FM-1 showed on them has a new home:

| FM-1 key lights | SMK-37 |
| --- | --- |
| the scale's keys and the notes sounding (the held list's strip) | HOME's keyboard picture, now C3..C6 (the real keybed); the CHORDS pads' bottom row shows the scale's degrees |
| a held button's list on the keys (a key jumps) | the pads (a tap jumps); the keys play on |
| HOME held: the favourites on the keys | PATCH held: the favourites on the pads |
| the 10 outer keys | notes only, in every map |

## 3. Knob banks

What K1..K4 edit. A bank is one of the pages that already exist (`ui_pages.c`'s stops), pinned to the knobs
whatever place is on the screen, so `field_step`, the cards, the editor's view of a change and the native SysEx
edits follow with no new path:

| Bank | PROPHET | FM6 | CZ-1 | JUNO-106 |
| --- | --- | --- | --- | --- |
| PAGE | the place's own four (the FM-1) | | | |
| MACROS | HOME's four: CUTOFF RESONANCE ENV AMOUNT RELEASE | MOD LEVEL MOD RATIO MOD EG DETUNE | VIBRATO LFO RATE GLIDE LEVEL | CUTOFF RESONANCE ENV AMOUNT RELEASE |
| FILTER | EDIT > FILTER > CUTOFF: CUTOFF RES ENV AMT KEYS | - | LINE 1 > WAVES: WAVE WAVE 2 WINDOW LEVEL | EDIT > VCF > CUTOFF: FREQ RES ENV ENV POL |
| ENV | ENV > AMP ENV: ATTACK DECAY SUSTAIN RELEASE | EDIT > the operator EDIT shows > RATES 1-4 (the bank follows the operator) | ENV > DCA 1 > RATES 1-4 | ENV > ENV: ATTACK DECAY SUSTAIN RELEASE |
| LFO | LFO > P5 LFO: RATE SAW TRI SQUARE | LFO > FM LFO > WAVE | LFO > VIBRATO | LFO > J106 LFO: RATE DELAY DCO VCF |
| ARP | ARP > PATTERN: MODE RATE OCTAVES GATE | | | |
| CHORD | CHORD: CHORD VOICING STRUM STROKE | | | |
| MIX | GLOBAL > OUTPUT: LEVEL PAN SPREAD, and MASTER as K4 (the virtual master, section 6) | | | |

LIBRARY, SAVE and GLOBAL keep their own four whatever bank is picked, with no tag: what they edit is not the sound. On every
other page, opening or turning it shows its own four, so its controls can always be reached: the bank takes over only when it is
chosen on that page (the first tap of KNOB BANK brings the picked bank back, the next goes on), until the page changes. HOME
keeps the bank, for playing. The LED says a bank is picked.

A bank the engine has nothing for ("-") is skipped when cycling and dark on the picker's pad; its name stays in
the picker's list, dim. FM6 has no FILTER bank; its ENV bank is the EG of the operator the EDIT place shows, so
turning EDIT's pages moves the bank with it (decided 2026-11-04).

Switching: **KNOB BANK tap** = the next bank (cycle, PAGE first; a header message names it); **held** (the HOLD
setting's time) = the picker over the picture, the current bank marked (`pick_banks`): K1 or `<` `>` walk it, a
top-row pad picks, letting go keeps what is shown. The LED is lit while the bank is not PAGE.

The screen (`edit_bank_filter`): the strip (the four values under the picture) shows the bank's four in place of
the place's, labels, values and bars as any page's, the knob turning hot in the engine's colour; the picture and
the header stay the place's. The bank's name is a header tag (FILTER, ENV, MACROS ..; none for PAGE). On HOME the
bank's cards drop in over the keyboard on the first turn and go after 1.5 s, as a macro's readout does today.

## 4. Fader banks

The same eight banks; the default is ENV: on a Prophet or a JUNO-106 the faders are A D S R the moment the keyboard
powers on, on a CZ-1 the first four rates, on an FM6 the pitch EG. **FADER BANK tap** = the next bank, **held** =
the picker (`pick_banks` with the faders' column), the LED lit while the bank is not ENV. PAGE on the faders is the
classic: the faders are the four values of the page on the screen and follow SELECT.

Pickup: a fader is absolute, a value is not. After a bank or sound change a fader is "away" until it crosses the
value, or comes next to it; then it follows. Whenever its parameter changes under it (a load, a knob, the editor, a
SysEx edit) it is away again. The screen shows it (`home_faders_env`, `j106_env_faders`): a fader moving brings the
bank's four cards into the strip for 1.5 s on every place (on HOME over the keyboard), the moving one hot, each card
with a pickup gauge under its value: the value's bar, a tick where the fader stands; away: the bar in MID until the
tick reaches it. A header tag FADERS says the cards are the faders' and not the knobs'. Then the place's cards come
back. A fader on a bank the engine lacks does nothing (its card reads "--").

Momentary: NOTE REPEAT (the FM-1's LFO) held while a fader moves is a momentary edit: let go and the value before
comes back. Nothing changes in the engines or the parameter set; a bank only decides which existing parameter a
fader reaches, through the same field a knob would.

## 5. The SMK-only buttons

| Button | Tap | Held |
| --- | --- | --- |
| KNOB BANK | next knob bank | the bank picker; + a top-row pad: pick; + `<` `>`: previous / next bank |
| FADER BANK | next fader bank | the same for the faders |
| KNOB BANK + FADER BANK | next pad bank | the pad bank; + one of the first four pads: pick; + `<` `>`: previous / next |
| `<` `>` | one SELECT detent: the previous / next page of the place; on HOME, where SELECT does nothing, the previous / next sound of the engine (PRESETS' step, the sound loads at rest); with the pads on SOUNDS or FAVS, the previous / next 16 | with a BANK button: its bank; with both: the pad bank |

PATCH held lists the favourites (HOME held). LEDs: KNOB BANK lit off PAGE, FADER BANK lit off ENV, both blink while
held; ARP flashes with the arp's notes; SEQ REC lit while the latch holds; SEQ (CHORD) lit while a chord type is on;
BT breathes while a dump waits; OCTAVE -/+ lit off 0; SEQ PLAY green while the PLAY place is open, its red for the
ERASE countdown.

## 6. Wheels, pressure, velocity, the pedal, the outer keys, MASTER

- Pitch wheel: the sound's bend (P5 BEND, FM6 BEND UP / DN, J106 and CZ-1 two semitones), as MIDI bend already is.
  Self-calibrating: the rest is the centre, the range grows to what it has reached (the travel is short).
- Mod wheel: CC1 / the matrix's MODW source, P5 WHEEL MOD, FM6's WHEEL; engines with no wheel route get a vibrato
  unless a MOD route uses MODW.
- Pad pressure: strum speed in CHORDS (section 2.2); no aftertouch from the pads. The SMK's keys have no
  pressure, so the matrix's AT source stays MIDI's.
- Velocity: keys and pads from the stock curves in the calibration record; GLOBAL > KEYS > VELOCITY (NORMAL SOFT
  HARD FIXED) remaps them. Every engine takes velocity through the common note path; J106 keeps the 106's fixed
  level (decided: no velocity to the VCA), velocity reaching it only through the matrix's VEL source.
- The sustain pedal (the jack): down, a key or pad let go stays held (the arp, chords and the latch see it held),
  struck again it sounds again; up releases; CC64 goes out. Its rest level is taken at power-on (either polarity,
  nothing plugged: never down); a pedal down for a minute is one plugged in afterwards: the new rest. ARP held stays
  the pedal too, so a player without a pedal loses nothing. HOME's status line says SUSTAIN for either.
- The 10 outer keys: notes, always.
- MASTER: K5 is an encoder, so the master is a value kept with the settings, 0..100, shown on GLOBAL > OUTPUT and
  as a header readout for a moment when K5 turns (as the octave is).

## 7. Screen changes

All composed from what exists; no new component, no hint rows, no text panels:

1. **Header tags** (`vm_header`): the knob bank's name, FADERS while the faders' cards are up, the pad bank while
   it differs from the place's own. Left of the engine's tag on HOME, right of the page's name elsewhere, the label face, the engine's
   colour; the octave pill gives way; a toast wins.
2. **The strip** shows the bank's cards, or for 1.5 s after a fader moves the fader bank's cards with the pickup
   gauge (a 2 px bar and a tick under the value: `vm_bar` gets a pickup argument). HOME: the cards drop in over the
   keyboard, as a macro's readout does.
3. **The bank picker**: the held list's rows over the picture (`vm_draw_reveal`'s face), the current marked, while
   a BANK button is held.
4. **The held list**: row numbers are pad numbers (1..16); the keys strip at its foot is a pads strip (two rows of
   eight, the current FILLED).
5. **HOME's keyboard**: C3..C6, the keybed, 22 white keys. The chord line, the state line and the macros unchanged.
6. **GLOBAL > KEYS** (`global_keys`), just before ERASE: VELOCITY (NORMAL SOFT HARD FIXED), PADS (OFF LOW MID HIGH FULL;
   the default MID), TRS OUT (ON OFF). The pedal's polarity needs no setting: its rest is learnt at power-on. The FM-1's
   LIGHTS row on DISPLAY is the button LEDs' level on the SMK. GLOBAL > OUTPUT's fourth value is MASTER. GLOBAL > TEMPO's
   CLOCK still offers TRS, which has no input here (to remove).
7. Everything else (LIBRARY, SAVE, IMPORT, ERASE, the pictures) is unchanged: the pads mirror it.

## 8. Open questions (for Kerem)

Decided on 2026-11-04 and written into the sections above:

1. CHORDS' top row: a tap sets the chord type, for the pads and the keys alike (not momentary).
2. SOUNDS on LIBRARY: a tap loads the sound at once; a sweep loads each one.
3. The fader default is ENV.
4. FM6's ENV bank is the EG RATES of the operator the EDIT place shows; FM6 has no FILTER bank.

5. J106 velocity: none; the 106's fixed level stays, velocity reaches it through the matrix's VEL source only.
6. Pressure on CHORDS: strum speed, not aftertouch.
7. `<` `>` on HOME: the previous / next sound.

Nothing is open.

Dropped: a KEYS pad mode (the pads as the 16 white keys). Kerem, 2026-11-04: not a good design for this firmware;
the pads must not be a second keyboard.

## 9. Implementation plan

The port plan's steps 5 and 6. Estimates from Melodee's measured phases (banks +2.7 KB, pads and maps +2.8 KB on
its build); Saadet's app area on the SMK has 40 KiB more than the FM-1's, so flash is not the constraint.

**Phase 0 (this): design and mocks.** `docs/SMK37_UX.md`, `docs/smk37-ux/mocks.html`. Then the firmware-drawn
mocks: `tests/smk_ux_mocks.c` on `tests/ui_harness.c` (`ui_png` at 240 x 240), the additions drawn with `gfx.c`'s
primitives over the real pages, composed with the pads and LEDs by `tools/smk_ux_mocks.py`. No firmware change.

Built 2026-11-05: phases 1 to 3 together: the Elite's image 574,908 B to 582,200 B, data RAM 96,020 B of 98,304. The lists below
are the original plan.

**Phase 1: banks (knobs and faders), ~2.5 KB flash, ~24 B RAM.**
- `firmware/src/ui_smk.c`: the bank table (a bank is a `ui_stop_t` per engine, built from `ST_*`), the knob bank
  as the stop the knobs edit while not PAGE (`ui_knob_field` consults it), the fader state and pickup (four
  values seen, a picked mask), the picker on `VM_REVEAL`'s rows.
- `ui.c` `ui_input`: KNOB BANK / FADER BANK tap and hold, `<` `>`, the faders' poll through `field_step` with an
  absolute value (as MIDI knobs set one), the momentary edit under NOTE REPEAT.
- `ui_view.c`: the header tag, the pickup gauge in `vm_bar`, the strip over HOME's keyboard.
- Tests: `tests/ui_test.c` cases behind `SAADET_SMK37` on the host stub's faders; the lint; the render scenes.
- Done: the mocks `edit_bank_filter`, `home_faders_env`, `j106_env_faders`, `pick_banks` match the device.

**Phase 2: pad modes and the momentary maps, ~3 KB flash, ~64 B RAM.**
- `ui_smk.c`: the pad bank; the lights (CHORDS from the scale, the chord type and the struck chord,
  SOUNDS from the list) in the engine's hue (RGB565 to the pads' 8-bit, the PAD LIGHTS level); the pad maps (pad to
  degree, pad to list entry, pad to letter); the taps.
- `seq.c`: pad notes and chords into the note path with their velocity; pressure into the AT source; the pedal
  beside `pedal_down()`.
- `ui.c`: the held lists on the pads, PATCH's favourites, SCALE's piano, SAVE's letters.
- Done: `home_chords`, `chord_pads`, `library_sounds`, `patch_held_favs`, `env_held`, `scale_held` match the device.

**Phase 3: GLOBAL > KEYS, wheels, the virtual master, ~1 KB.** `global_keys` matches.

**Phase 4, later:** a 4 x 4 favourites launcher as a mode of its own, the arp's rhythm on the
pads, MIDI LEARN of the SMK's extra controls.

Order of work: phase 1 first (it changes nothing the pads do and can ship alone), phase 2 as one
commit per mode, each flashed and tried on the Elite before the next.
