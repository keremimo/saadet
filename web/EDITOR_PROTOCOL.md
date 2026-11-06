# Saadet editor protocol

The running implementation is `firmware/src/editor.c` and its included native,
preference, library and backup handlers. The host fixtures in `tests/synth_test.c`
and `web/test_synth.mjs` exercise the actual capability and archive records.
Hardware protocol behavior has not been verified for Saadet.

Frames use `F0 7D 46 4C command arguments… F7`. All payload bytes are seven-bit
safe. Signed parameter values use two least-significant-first seven-bit bytes,
offset by 8192. Replies echo the command. WATCH enables parameter/reload pushes (bit 1 adds TRACK_CHANGED, bit 2
PUSH: see "Pushes"). A push the unit could not send (the host stopped reading for 200 ms) stays due and goes out at a later
pass, so a client watching never misses a change it was owed.

INFO identifies `SAADET`, two parts, zero steps, synth marker `53 01` (hex), native
engine capabilities, and shared-library marker `55 01` with a two-byte total of
384 slots. Engine identities remain Prophet 19, FM6 12, CZ-1 15 and SID 3;
JUNO-106 is 21. After the shared-library marker, `4A 01 15 16 60 1A` announces
JUNO-106: engine 21, favourites namespace 22, command 96 and 26 fields. `48 01 19 1A 61 10 01`
announces HYDRA: engine 25, favourites namespace 26, command 97 and a patch of 144 bytes (two digits, low first). Engine 20
is reserved (the Prophet's user namespace), as are 22 (the JUNO-106's) and 23 and 24. Reserved identities do not expose
additional engines or parts. Globals 10, 11 and 24 are the parts mode (0 ONE, 1 MULTI, 2 SPLIT, 3 LAYER), the split note
and part B's MIDI channel. With the mode at ONE, only part zero can be selected.

After HYDRA's user-wave marker, `45 01 features 63 64 65 52 0C` (hex) announces the editor features: a mask (1 PUSH, 2
performances, 4 the voice budget, 8 a CZ-1 tone into a playing part without a cut, 16 FM6's function settings, 32 global
BUDGET is the instrument's voice budget), then the commands for performances (99), PUSH (100), VOICES (101) and FM6's
function settings (82), and the number of performances (12). Firmware without the marker has none of these.

| Commands | Purpose |
|---|---|
| 1–5, 8, 10 | INFO, parameter get/set/dump/descriptors, preset selection, factory names |
| 16–21 | Legacy seven-bit user-slot compatibility; use command 80 for the full library |
| 22–25 | WATCH, CHANGED, RELOAD and PING |
| 27–29, 31–32 | Parts: select one, read and set the other part's mix and parameters |
| 34–38 | UI state/preferences/palettes and favorites |
| 65–67 | Saadet archive inventory, chunk read and staged restore; objects 33–38 hold the performances, 39 and 40 HYDRA's user waves |
| 68–71 | Native FM6 transfers |
| 72 | Audio diagnostics |
| 78 | Legacy native-slot compatibility |
| 80 | Shared 384-slot library |
| 81 | Raw battery ADC and full-charge reference |
| 75–77 | Native CZ transfers and legacy bank metadata where applicable |
| 95 | Native Prophet transfers |
| 96 | Native JUNO-106 transfers |
| 97 | Native HYDRA transfers |
| 98 | HYDRA user waves |
| 82 | A part's FM6 function settings and operator switches |
| 99 | Performances: list, load, store, rename, delete |
| 100 | PUSH: the parts' native patches and sounds, HYDRA's user waves |
| 101 | VOICES: the shared voice budget as it plays |

Command 72 answers on both builds with the same layout (`tools/usb_audio_stats.py`
reads it; flag 1 resets the maxima, 2 adds voices, 4 the second core, 8 memory,
16 per-part voice cuts). On the fm1-base build the base owns USB audio: fields
0–15 and 17 read 0, `audio_late` counts the base's missed renders and
`audio_max_us` is the longest render it measured.

Step, pattern, song, motion and project commands are rejected without a reply.
Sample uploading is unavailable. Clients must use the advertised capabilities.
The editor exposes only Sound, FM6, Library and Settings.

## Battery reference

Command 81 without arguments reads a fresh battery ADC sample. Arguments
`1, full_low7, full_high7` save a measured full-charge value (1–1023) in device
settings. Replies are `1, result, raw_low7, raw_high7, full_low7, full_high7`;
the leading 1 is the schema, and a zero stored full value means uncalibrated.
Result 0 confirms the request, 1 rejects arguments, 2 means no valid reading or
a write blocked by transport/old settings, 3 means applied in RAM but not saved,
and 4 means the settings write is queued. These values have no 8192 offset.
The full reference alone does not establish a battery percentage or an empty
reference. `tools/battery_calibrate.py --capture-full` samples the current
battery and saves the median, so use it only when the battery is fully charged.

## Shared sounds

Command 80 arguments begin with operation, slot low seven bits and slot high
bits. Replies echo operation and slot followed by a result code. Absolute slots
are 0–383; displayed slots are 1–384.

Operations are list (0, at most 16 entries), get (1), put (2), store current sound
(3), load (4), erase (5) and rename (6). Store/load accept only part zero.
Native payload kinds are FM6 (1, 128 bytes), CZ-1 (2, 144 bytes), Prophet
(3, 138 bytes), JUNO-106 (4, 31 bytes) and HYDRA (5, 144 bytes). Native data is grouped in seven-byte chunks preceded by the
chunk's high-bit mask. Legacy phrase fields in the numeric wire layout remain
reserved; no phrase is applied or recorded.

## JUNO-106 tones

A tone is 31 bytes: the instrument's sixteen sliders, SW1 and SW2 in patch-message
order, its patch number (0–127, A11–B88) and a 12-character space-padded name
(`firmware/src/j106_patch.h`). Command 96 takes operation, part, payload and
replies operation (GET and PUT reply 1), result, part:

- 0 GET: the tone as a patch message without F0/F7 (`41 30 0n pp`, 18 bytes;
  n is GLOBAL > MIDI OUT CH), then its name, zero-terminated.
- 1 PUT: a patch or manual-mode message without F0/F7; the part plays the tone
  (named `PATCH A11`… or `MANUAL`; use 3 to name it).
- 2 field edit: field, value. Fields 0–17 are the bytes; 18–25 are range,
  pulse, saw, chorus (off/I/II), PWM source, VCF envelope polarity, VCA mode
  and HPF (0–3). A switch edit changes only its own bits.
- 3 name: 1–12 printable characters.
- 4 send: the device sends the patch message.

Roland frames outside the editor header are accepted too: a patch (`41 30`) or
manual-mode (`41 31`) message waits in SAVE for its slots, as other native
dumps do, and a single-control message (`41 32 0n cc vv`, cc 0–17) edits the
playing JUNO-106 tone. Only USB delivers SysEx; TRS MIDI input drops it.
Favourites use namespace 21 for the presets (INIT, Roland's factory patches
A11–B88 as presets 1–128) and 22 for user slots.

## HYDRA patches

A patch is 144 bytes (`firmware/src/hydra_patch.h`), its last twelve a space-padded name. Bytes 95..102 name its eight macros
(an index into `HY_MACRO_NAMES`, 0 AUTO); 103..130 are 14 assignments of two bytes, the macro (high three bits) and its
destination (low five, an index into `HY_TARGETS`, 0 free) then the depth + 127. The macros' values are the part's parameters
E1..E8 (`P_E0..P_E7`, -64..63), read and set as any parameter; a PUT or a native message sets them to 0. Command 97 takes
operation, part, payload and replies operation (GET and PUT reply 1), result, part:

- 0 GET: the patch as low-first nibble pairs (288 bytes).
- 1 PUT: the same; the part plays the patch.
- 2 byte edit: byte, value as two seven-bit digits, low first (the raw byte: value - the field's minimum; KEY reaches 200).
- 3 name: 1-12 printable characters.
- 4 send: the device sends the patch's message.

Its message is Saadet's own, `F0 7D 48 59 01`, the nibble pairs, `F7`: outside the editor header it auditions the patch and
waits in SAVE for its slots, as other native dumps do. Favourites use namespace 25 for the presets (INIT, 16 factory sounds)
and 26 for user slots.

## HYDRA user waves

A wave's id, as a HYDRA patch's slots hold it: 0 EMPTY, 1–56 the factory waves, then the user waves from the id INFO gives;
64 ids are reserved for them (57–120), so the bank can grow without a new patch format. OSC 1 and OSC 2's WAVE and OSC 3's
WAVE hold the id - 1. An id past the unit's bank plays silence. After HYDRA's marker, `57 01 62 39 0E 40` announces command
98, the first user id (57), the bank's count (14) and the reserved count (64).

A user wave is 256 int16 points (low byte first on the wire), kept as `tools/gen_hy_waves.py` keeps the factory waves:
harmonics 1–64, no DC, its peak at 30000. Clients band-limit and normalise before they send one. Its name is 1–7 printable
characters, as seven bytes with 0 after the name. Command 98 takes the operation, then a wave's id; replies are the
operation, the result and the id:

- 0 LIST: the count and the first id, then for each user wave: 1 when written (0: free, a sine named `USER n`), its name
  (seven bytes) and the CRC-32 of its points as five seven-bit digits, low first.
- 1 GET id: any factory or user wave: its name (seven bytes), then its 512 bytes packed seven to eight (a byte of high
  bits, then seven low ones; 586 bytes).
- 2 PUT id, name, points: a user wave, packed as GET has it. It is heard at the next audio block in every part using it,
  and kept when the unit rests.
- 3 STORE: the changed waves into flash now (3: stop playback first, 4: the write failed).
- 4 REVERT id: the wave as flash holds it.
- 5 CLEAR id: free again.

Files carry user waves as Saadet's own message, `F0 7D 48 59 02`, the id, the seven name bytes, the packed points, `F7`
(600 bytes). A file with HYDRA sounds lists the waves first, numbered in the file, then the patches naming them by those
ids; a client that imports it puts each wave into the unit's bank (one with the same points is reused, others take free
waves) and rewrites the patches' ids. The unit itself does not read these messages.

## Pushes

WATCH with bit 2 (`WATCH 05` or `07`) adds command 100 pushes; the reply carries bit 2 when the firmware has them. Each
names what changed, and the client reads it again:

- `100 0 part engine`: part's native patch changed (Prophet program, JUNO-106 tone, HYDRA patch, CZ-1 tone, FM6 voice with
  its function settings and operator switches), from the panel, MIDI controllers, a native SysEx message or anything else.
  At most one every 30 ms per part: a knob turning is followed, not echoed byte by byte.
- `100 1 part engine preset_low preset_high`: a part other than the selected one took another sound (a performance load,
  a slot load to part B). The selected part's still pushes RELOAD.
- `100 2`: HYDRA's user waves changed (WAVE LAB, a restore): read the list (command 98) and the waves whose CRC changed.

The unit follows each part's native patch by a hash, so every edit path is seen. The client's own requests for a part's
native patch (95, 96, 97 and 98, 68 and 69 for a part, 75 and 76 for a part, 82) mark it as known: no push echoes them.

Command 100 as a request (no arguments) answers the part count, then per part its engine and the native patch's hash
(five seven-bit digits, low first), then HYDRA's user wave generation (five digits): a client that polls instead of
watching compares these and reads only what changed.

## Performances

Command 99 takes an operation and a slot (0–11), and answers the operation, the slot and a result (0 done, 1 refused,
2 stop playback first, 3 the flash write failed). 0 LIST (slot 0): then the count (12), the slot loaded or stored last
plus 1 (0 none), and per slot used, part A's engine and the name (up to 12 characters, 0-terminated). 1 LOAD: both parts
take the performance's sounds, the parts mode, the split and part B's channel, as the LIBRARY page loads it (RELOAD and
PUSH follow). 2 STORE, then a name of 1–12 printable characters: the setup as it stands. 3 RENAME, then the name.
4 DELETE.

## Voices

Command 101 without arguments answers the shared voice budget as it plays, in quarter steps (a voice of cost 1 is VQ):
VQ, the budget and the ceiling (two seven-bit digits each), the parts sounding, the part count, then per part its cost per
note (two digits), the voices playing, the voices fading, the notes waiting for a voice, the cost its notes claim and its
guaranteed share (two digits each). The budget bounds polyphony before a note starts; nothing is cut mid-note. Global 41
(BUDGET, 8–32) is the instrument's own budget setting, as the VOICES page sets it, and is saved with the settings.

## FM6 function settings

Command 82: `0 part` reads a part's function settings (16 bytes: bend up, bend down, bend step, portamento mode, time,
glissando, then wheel, foot, breath and aftertouch range and target, velocity scaling, renderer) and its operator switches
(bit 0 OP6 … bit 5 OP1). `1 part setting value` sets one; setting 16 is the switches. Replies: operation, part, result.

## CZ-1 tones without a cut

Command 76 with target 3 puts a tone into a part that plays a native CZ-1 tone now, as the panel's own edits change it:
sounding notes go on, E1–E8 stay, and COMP keeps the tone from before the first change. Any other part answers result 1;
target 0 still loads a tone whole (notes cut, E1–E8 reset).

## Backups

Archives are `saadet-backup` version 3. The ordered object identities are
`1, 6, 7, 9–28, 32, 33–38, 39, 40`: settings, 22 shared sound banks, current synth state, the six performance objects and
HYDRA's two user wave objects (`firmware/src/hydra_bank.c`: seven waves each, 3676 bytes, `HYW1`). Object 28 is a sound
bank; 32 is exclusively Saadet state. Version 2 archives end at 38 and version 1 at 32; they restore without touching what
they lack, and a unit without the user waves restores everything else of a version 3 archive. The inventory writes the
user waves RAM has changed into flash first. No project, sample, pattern or song identities are accepted.

The inventory supplies object length and CRC32. Restore begins an object with
its expected length/CRC, transfers offset-checked chunks, then commits or cancels.
The firmware checks schema, native records, panel mapping and state checksum
before A/B flash publication. The editor validates the complete archive and
preflights all sizes before committing any object, restores sound banks first,
settings next, and current state last. Restore is atomic per object, rather than
across the whole archive. A USB reset, reused transfer scratch or timeout cancels
staged data. See `web/fm1backup.js` for the client-side framing and validation.

Saadet state uses `SDS1` and an internal size/FNV checksum. It gained the
JUNO-106 tone at its end; a shorter state from before it still loads and
restores, with the INIT tone. Each part also holds a HYDRA patch; a state or performance object from before HYDRA restores
with its INIT patch. Its flash pair is
`0xA0000/0xA1000`, formerly part of Melodee project A; saving Saadet can replace
that project's data. Melodee backups and project files are not Saadet archives.
