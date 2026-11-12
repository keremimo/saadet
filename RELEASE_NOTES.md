# Saadet 1.0.5

A fix release on Saadet 1.0.4 for the M-VAVE FM-1 and, in beta, the M-VAVE SMK-37: held chords stop pumping, the
MOD ENV works on every synth engine, and the FM-1 Screen Reader finds Saadet's words. The SMK-37 packages keep their
identities (`SMK-37 Elite_910` and so on), so an SMK-37 on 1.0.0 to 1.0.4 shows as up to date in the installer: press
**Reinstall** to get the changes below.

## Fixed in 1.0.5

- Held chords at high levels pumped. Once a chord reached the limiter, its gain rose and fell with the chord's own
  beating, which came out as a wobble around the notes (about -28 dB on a triad, -13 dB on a detuned pad). The
  limiter now holds its reduction while the peaks keep coming back, for up to a second, so a held chord stays steady
  (those products are now -73 and -65 dB). A lone hit still recovers in about a quarter of a second.
- The MOD ENV (ENV > AMOUNTS: FILTER, PITCH, SHAPE) did nothing on the engines with their own envelopes: Prophet-5,
  JUNO-106, FM6, CZ-1 and PPG Wave. It now moves their filter, pitch and shape, its SUSTAIN holds them there, and it
  never touches the voice's level: the engine's own envelopes still shape and end each note. Velocity above 110 no
  longer opens the filter extra on those engines, which have their own velocity response.
- The FM-1 Screen Reader could not find Saadet's words: the firmware pointed it at fm1-base's template site. It now
  points at `https://keremimo.github.io/saadet/words/`, which the installer site publishes. Thanks to Charles Vestal
  ([#5](https://github.com/keremimo/saadet/issues/5)).

## Fixed in 1.0.4

- JUNO-106: every patch sounded muffled. The cutoff followed a curve of Saadet's own, 8 Hz to 17.5 kHz over the FREQ
  slider, which put FREQ 64 at 390 Hz and 100 at 3.4 kHz, one to two octaves below the instrument. It now follows the
  Juno-106's own control law: its CPU sums the slider, the envelope, the LFO and the keys into one 14-bit word for the
  filter, 1143 counts an octave, and the service manual calibrates FREQ 49 on C4 to whistle at 248 Hz (992 Hz on C6).
  FREQ 64 is now 795 Hz and 100 is 13 kHz. VCF ENV at full sweeps 14.2 octaves (was 10) and VCF LFO 3.6 (was 4), as
  the firmware scales them; KYBD is unchanged. Roland's factory patches now sound as they were voiced.
- JUNO-106: full resonance whistled about 10 dB under a single wave. It now whistles as loud as a pulse through the
  open filter, as recordings of the instrument show. With the cutoff where Roland put it, the old limit also made the
  sub of a resonant PWM patch (B56 ETHEREAL) wobble with the PWM; it is steady again. Patches at high resonance come
  out louder.

## New in 1.0.3

- Bluetooth radio calibration recovery, as Melodee has it. Saadet's radio uses the calibration the stock firmware
  stored, and keeps a copy of it; with neither, it measures its own at every start. A unit whose stock calibration
  sector is gone can now get its own back from a full 1 MiB flash backup taken before: `tools/ble_calibration.py`
  reads the backup, and `--restore` sends only the checked 100-byte calibration over USB (editor command 83). It is
  taken at the next power-on. Saadet never replaces a different calibration it already keeps, and refuses while
  anything is sounding. Saadet's INFO announces the command (editor feature 64).

## Fixed in 1.0.3

- Chords at high levels came out rough. The output stage ran every sample through a tanh curve after the limiter, so
  a chord at the limiter's level came out with intermodulation at about -26 dB, and still about -33 dB at -6 dBFS.
  The curve now starts at the limiter's threshold (-2 dBFS): below it the output is exact, and only the brief overs
  the limiter has not caught yet bend, to -0.07 dBFS at most. Loud passages come out up to 1.6 dB louder, as the
  limiter now settles at -2 dBFS (was -3.6). USB recordings show that; the speaker and headphones keep their -6 dB
  ceiling below it.

## New in 1.0.2

1.0.1 was never published; its fixes are in 1.0.2.


- The installer offers **m-bootloader 2.3** (was 1.2) as its optional recovery bootloader. 2.2 added MIDI mode, which
  reinstalls a firmware over USB MIDI with no firmware running, and fixed `BAD CONFIG` after the 1.2 option; from 2.3
  the menu opens in MIDI mode by itself when the firmware won't start.
- The installer installs Saadet through m-bootloader's MIDI mode. Hold PLAY (SEQ PLAY on an SMK-37) while switching on
  and choose MIDI MODE (it opens by itself when the firmware won't start), then press Find device. MIDI mode answers as
  an update loader does; the page tells it apart with M-Flasher's protocol and shows m-bootloader's version and the
  flash chip. MIDI mode checks the whole package before it erases anything and writes only the firmware area, so the
  boot area and saved sounds stay. On an SMK-37 the page asks whether it is a Pro or an Elite, as MIDI mode calls every
  SMK-37 a Pro.

## Fixed in 1.0.2

- The computer's volume and mute for its own audio played through the FM-1 or SMK-37 (the device's playback volume in
  the computer's sound settings) did nothing: Saadet refused every such request once the USB interfaces put MIDI last
  for macOS 13-15. On Linux the connect logged `cannot get min/max values for control 2 (id 5)`. They are answered
  now. Saadet's own sound, MASTER, MIDI and USB recording were never affected.
- SMK-37: turning MASTER (K5) while a note was held could break the sound. The level is kept in flash two seconds after
  the last turn, and that write stopped the audio while the note was sounding. The write now waits until nothing is
  sounding, as Saadet's settings writes already did; the level itself still changes at once. Checked on an SMK-37 Elite.
- The installer on the release site installs the SMK-37 packages, chosen by the model the connected device names (never
  its port name). This reached the 1.0.0 site after its release.
- `tools/fm1_install.py` finds an FM-1 whose firmware reports its flash after its identity (Melodee on fm1-base), where
  it said none was connected; `--info` names the flash part. USB recovery (`tools/fm1_recover.py`) takes FM-1s with the
  UC25HQ80 flash (JEDEC B36014), found in units sold since autumn 2026, as it takes the stock Puya part.

## New in 1.0

Six engines, two parts and a new editor for the M-VAVE FM-1, based on Saadet 0.1.1. Saadet 0.2.0 was never released;
everything its notes listed is part of 1.0. The audio rate remains 44.1 kHz. Saadet also builds for the M-VAVE SMK-37,
in beta.

### Engines

- PPG, a new engine: the PPG Wave 2.2, playing its original wavetables and all 100 factory programs, each with its two
  groups layered or split, and the sampled grand piano and saxophone of TABLE 31. Its envelopes and filter follow the
  2.2's own operating system. PPG sounds have user slots of their own and are edited on the panel and in Saadet Studio.
- HYDRA, a new engine: three oscillators on 56 of our own waves, OSC 1 and 2 playing one or scanning a list of eight, four
  mutants (FM, STACK, SYNC, PW, SQUEEZE, WARP, HARMONIC), ring and noise, two filters and 16 factory sounds. Its voices cost what
  the patch asks: eight of a plain patch or one with a mutant, six with mutants on both oscillators, fewer with heavier
  stacks, and the budget is bound before a note
  starts. A saved state and performances from before it keep their sounds and gain HYDRA's INIT. Its eight macros are the
  Hydrasynth's kind: each sound names them and routes them to its own destinations with depths (LFO > MACRO 1–8), and a voice
  is budgeted with every macro at its costliest, so no turn makes the sound late.
- HYDRA user waves: a bank of 14 waves of your own beside the 56, for OSC 1 and 2 and their scan lists. Draw them, set
  their harmonics or import a .wav or a wavetable in Saadet Studio, or shape one by ear in EDIT > WAVE LAB on the panel
  (MIX, SHAPE, SPECTRUM and drawbars). Editing a factory wave copies it into a free user wave; edits are kept at rest
  and go into backups.
- HYDRA's voices are budgeted in quarter steps, as the two cores run them, so a patch plays as many voices as it can
  afford. If the second core is off, every voice is budgeted at its own time, so HYDRA plays fewer voices rather than
  late.
- Saadet Studio and the web editor convert ASM Hydrasynth patches into HYDRA sounds: SysEx dumps, ASM Manager .hydra banks and .patch files,
  into the library or straight into the bank. It is approximate: the waves are matched by family, and the LFOs, the other
  envelopes, most of the matrix and the effects are left out. Of the 2,304 patches tried (ASM's banks and two
  third-party packs) all converted; 7 played only from the ribbon, wheels or LFOs are silent at rest.
- Every sound has eight macros, in two rows of four. Turn SELECT on HOME for the second row on K1–K4. On an SMK-37, the new
  8 KNOBS bank puts all eight on K1–K8 at once; while it is picked, K5–K8 are not MASTER, SELECT, ALGORITHM and PRESETS. On
  HYDRA the eight are the sound's own macros, by the names it gives them.

### Parts and playing

- Two parts. GLOBAL > PARTS plays them by MIDI channel, split or layered, and tap HOME to change part. With one part, the
  default, nothing changes and part B costs nothing. They share the voice budget, half guaranteed to each, the rest lent.
- Performances: 12 named setups of both parts, saved from SAVE's PERFORM page and loaded from LIBRARY. Backups include them;
  an older backup restores and leaves them alone. A saved state from before this release loads as one part.
- Hold ARP as a sustain pedal that latches, for a drone. The notes you play or let go of while ARP is down keep
  sounding after you let go of it, and the keys play over them, until you press ARP again. Striking a kept note
  again restarts it, and with the arp on, the arp keeps playing the kept notes. Their MIDI note-offs wait for
  ARP too.
- Tap ARP to open it or turn its page; while notes are kept, that press ends them instead. Holding ARP no longer
  lists its pages on the keys; the keys play while it is held. HOME's status line shows SUSTAIN while ARP is down
  or notes are kept, and ARP's LED stays lit while they are kept.
- The pedal applies to the panel keys. External MIDI notes still follow their own CC64 sustain.
- REC's latch now starts off at power-on; it no longer returns with the last sound.

### Sound

- Overload no longer cuts voices, whatever a stored Melodee SAFETY setting says.
- The limiter now takes only what goes over -2 dBFS, up from -5 dBFS, so at about 70 % of MASTER's travel a triad no longer
  pushes a held note down, except on JUNO-106. At full MASTER it still takes 1 to 2 dB off a triad on average (about 5 on
  JUNO-106), and more with six notes or two layered parts.
- CZ-1 plays 6 dB quieter, at the level of Prophet and FM6, so saved CZ-1 sounds and performances come out quieter.
  JUNO-106 keeps its level and is now the loudest engine, by about 4 dB.
- The level the limiter settles at rises from -6 to -3.6 dBFS, and brief peaks from about -3 to -2 dBFS. USB recordings show
  that; the speaker and headphones keep their -6 dB ceiling below it.
- The cuts to whole samples on the way out (each engine's output, the part's level, MASTER and the output ceiling) are
  dithered, so a fading note's rounding error no longer follows it. That error sits at the last of the 24 bits, about
  -138 dBFS, so no audible change is expected.
- A note that takes a voice no longer cuts it. The old voice fades over 23 ms under the new note, which starts at once, on
  Prophet, JUNO-106 and CZ-1. When several notes take voices at the same moment, or on FM6 with all 16 playing, a note may
  wait until a fade has hurried out, 5.8 ms at most. Two parts now keep one voice's room for the fade: 8 Prophet voices
  between them, not 9.
- Prophet: pressing a key again in its own tail, softer or harder, no longer clicks or drops the tail to the new level in one
  sample. The tail rings on under a fresh voice. In MONO, LEGATO and UNISON the new velocity's level arrives over 5.8 ms.
- Prophet voices no longer leave a thump or a click behind them. The output's coupling capacitor now sits on each voice,
  before the VCA, so a voice that stops quickly takes its DC with it.
- CZ-1 and FM6: striking a key in its own tail no longer clicks; the tail rings on under the new note. MONO's next note on
  these two engines fades the last one first, 5.8 ms, in place of a jump to silence.
- USB recordings hold through long renders. Under a heavy voice load they used to repeat a few frames, hundreds of times in
  half a minute; the recording now keeps one more render in hand, 2.9 ms of added recording latency. The speaker and
  headphones never had this and do not change.

### Saadet Studio

- Saadet Studio is the new editor, in Chrome or Edge at <https://saadet.ellicstudio.com>, over USB. It edits all six
  engines with their own pages, keeps a sound library in the browser, imports SysEx, Hydrasynth patches, .wav files and
  wavetables, exports SysEx, and sets up parts, performances and the unit's settings, with backups and restores.
- It follows the panel: an edit made on the unit shows in the editor as it happens. It shows each part's voice budget.
- CZ-1 sounds are edited from the computer without cutting the notes that are sounding. FM6's function settings are
  editable.
- The old web editor stays on the release site for now; Saadet Studio replaces it.

### Bluetooth, system and installer

- Bluetooth now runs on an open stack instead of JieLi's binary libraries, which frees about 150 KB of flash for new engines.
  There is one Bluetooth link at a time: a phone or computer, or one external device, not both. After installing, forget
  Saadet on your phone or computer once and connect again: the radio has a new address and old pairings are gone.
- The second core starts, stops and recovers as it does on Melodee. A restart that Saadet makes on purpose no longer
  counts as a failed boot, which could leave the second core off until the next power-cycle and halve what the engines
  can play. A crash report keeps the second core off only if it was running.
- The installer resumes an interrupted switch back to Melodee with the update loader of the unit's own model.
- Builds pin Pillow and fontTools, which draw the firmware's UI font, so the same source gives the same package hash.

### SMK-37 (beta)

- Saadet builds for the M-VAVE SMK-37: Saadet Pro, Saadet Elite and Saadet MKE, for the SMK-37 Pro, the SMK-37 Elite and
  the MKE-P37. All 37 keys with velocity, the sustain pedal, both wheels, the pads, knob and fader banks with a picker
  and pickup gauges, and the 8 KNOBS bank (above). Each package is refused on any other model. See docs/SMK37.md.
- Only the Elite has run it so far; the Pro and MKE packages are the same firmware under their names.

## Fixed in 0.1.1

- Recover MIDI clock pulses discarded during an input queue overflow, keeping USB, TRS and BLE transport
  phase aligned with the selected master while still releasing stuck notes.
- Consume every overdue arpeggiator step after a clock burst, retaining swing, residual phase and gate timing.
- Backport the timing changes from Melodee commit `0e3d353`; retain Saadet's single-synth MIDI input handling.

## Included

- A polyphonic synthesizer of one or two parts with Prophet, FM6, CZ-1, JUNO-106, HYDRA and PPG engines.
- Native tone editing, SysEx import/export, factory sounds and 384 shared user sound slots.
- Eight macros on every sound; HYDRA's own and a bank of 14 user waves.
- Arpeggiator, HOLD, the ARP drone, chords, strum, scales, microtonal tuning and MIDI expression.
- Native 24-bit audio, stereo USB playback and synth-master capture at 44.1 kHz.
- USB, TRS and Bluetooth MIDI. GLOBAL > BLE manages the phone/computer link;
  GLOBAL > DEV discovers and connects an external Bluetooth MIDI device. One Bluetooth link at a time.
- The last successfully connected outgoing MIDI device is remembered and retried after boot or connection
  loss, with increasing delays up to 30 seconds. Disconnect and Radio Off stop retries; Forget clears it.
- Automatic last-sound recall, complete Saadet backups, Saadet Studio and a browser-based USB installer.
- Optional m-bootloader 2.3 recovery, installed separately after the application, and installs through its MIDI mode.

The firmware uses MPL-2.0 with the retained third-party terms documented in LICENSING.md. The editor and
some tools retain GPL-3.0-only. The downloadable source includes the matching application and pinned
fm1-base/felucca-bsp source; vendor factory sound data keeps its own attribution and terms.
The PPG Wave 2.2's wavetable, operating-system and factory-program data are compiled into the firmware but are not
in the source: a build from it needs your own copies, or leaves PPG out (LICENSING.md, BUILDING.md).

## Installation

Open [the installer](https://keremimo.github.io/saadet/install/) in Chrome or Edge on a supported computer, connect the FM-1 by USB and follow
the installer. Back up any sounds you want to keep. m-bootloader is optional and off by default. If selected,
it is written after Saadet; keep USB and power connected until completion. See BUILDING.md for recovery. A unit in
m-bootloader's MIDI mode (2.2 and later) is installed the same way: connect it and press Find device.

After installing over 0.1.x, forget Saadet on your phone or computer once and connect again: the Bluetooth radio has a
new address and old pairings are gone. A saved state from 0.1.x loads as one part.

Installing and saving over Melodee can replace its former project A data; Melodee project backups are not Saadet
backups. Preserve a backup before switching firmware families.

## Known limits and validation

Devices that rotate their private Bluetooth address may need to be selected again. Incoming phone/computer
connections are initiated by the host; automatic reconnect applies to the outgoing MIDI-device link.

Automated checks cover native engines/audio, MIDI, storage, UI, BLE protocol and reconnect policy, installer
transfer/resume, bootloader handling, source/package consistency and licence notices, on the FM-1 and SMK-37 builds.
Device checks since 0.1.1 were made on an SMK-37 Elite running the same source. Measured there and not yet on an FM-1:
HYDRA's voice costs and USB recordings under a heavy voice load. Not yet measured on any unit: two parts at the full
budget with USB audio on, and PPG's voice cost, which is set from the host. PPG's envelope speed rests on the 2.2's
main loop timed in an emulator of its operating system, not on a real 2.2. The installer's MIDI-mode install and
m-bootloader 2.3 installed from Saadet are tested against simulated units and fm1-base's updater on the host, not yet on
an FM-1 or an SMK-37. m-bootloader's author reports it little tested on FM-1s with the UC25HQ80 flash. The USB
volume fix is tested against the shipping USB code and descriptors on the host, not yet on a unit. 1.0.3's output
curve is tested on the host, at both sample precisions, for exactness under the threshold and the intermodulation of a
held chord; the build runs on an SMK-37 Elite, but its sound has not been compared by ear there. The calibration
restore is tested against the firmware's own checks on the host; it has not yet restored a unit. 1.0.4's JUNO-106
filter is tested on the host against the service manual's calibration and the whistle's level against a pulse; it has
not been heard on a unit or compared with an instrument.

The preparation tools do not install or publish anything. BUILD_INFO.json and SHA256SUMS identify the exact
package and corresponding source; VERIFICATION.json records the completed automated preparation checks.
