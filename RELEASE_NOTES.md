# Saadet 1.0.0

Six engines, two parts and a new editor for the M-VAVE FM-1, based on Saadet 0.1.1. Saadet 0.2.0 was never released;
everything its notes listed is part of 1.0. The audio rate remains 44.1 kHz. Saadet also builds for the M-VAVE SMK-37,
in beta.

## New in 1.0

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
- Optional m-bootloader 1.2 recovery, installed separately after the application.

The firmware uses MPL-2.0 with the retained third-party terms documented in LICENSING.md. The editor and
some tools retain GPL-3.0-only. The downloadable source includes the matching application and pinned
fm1-base/felucca-bsp source; vendor factory sound data keeps its own attribution and terms.
The PPG Wave 2.2's wavetable, operating-system and factory-program data are compiled into the firmware but are not
in the source: a build from it needs your own copies, or leaves PPG out (LICENSING.md, BUILDING.md).

## Installation

Open [the installer](https://keremimo.github.io/saadet/install/) in Chrome or Edge on a supported computer, connect the FM-1 by USB and follow
the installer. Back up any sounds you want to keep. m-bootloader is optional and off by default. If selected,
it is written after Saadet; keep USB and power connected until completion. See BUILDING.md for recovery.

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
main loop timed in an emulator of its operating system, not on a real 2.2.

The preparation tools do not install or publish anything. BUILD_INFO.json and SHA256SUMS identify the exact
package and corresponding source; VERIFICATION.json records the completed automated preparation checks.
