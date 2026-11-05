# Saadet 0.2.0

A sustain pedal on the panel for the M-VAVE FM-1, based on Saadet 0.1.1.
The audio rate remains 44.1 kHz.
This is an experimental release; the outstanding hardware checks are listed below.

## New in 0.2.0

- Two parts. GLOBAL > PARTS plays them by MIDI channel, split or layered, and tap HOME to change part. With one part, the
  default, nothing changes and part B costs nothing. They share the voice budget, half guaranteed to each, the rest lent.
- Performances: 12 named setups of both parts, saved from SAVE's PERFORM page and loaded from LIBRARY. Backups include them;
  an older backup restores and leaves them alone. A saved state from before this release loads as one part.
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
- Hold ARP as a sustain pedal that latches, for a drone. The notes you play or let go of while ARP is down keep
  sounding after you let go of it, and the keys play over them, until you press ARP again. Striking a kept note
  again restarts it, and with the arp on, the arp keeps playing the kept notes. Their MIDI note-offs wait for
  ARP too.
- Tap ARP to open it or turn its page; while notes are kept, that press ends them instead. Holding ARP no longer
  lists its pages on the keys; the keys play while it is held. HOME's status line shows SUSTAIN while ARP is down
  or notes are kept, and ARP's LED stays lit while they are kept.
- The pedal applies to the panel keys. External MIDI notes still follow their own CC64 sustain.
- REC's latch now starts off at power-on; it no longer returns with the last sound.

## Fixed in 0.1.1

- Recover MIDI clock pulses discarded during an input queue overflow, keeping USB, TRS and BLE transport
  phase aligned with the selected master while still releasing stuck notes.
- Consume every overdue arpeggiator step after a clock burst, retaining swing, residual phase and gate timing.
- Backport the timing changes from Melodee commit `0e3d353`; retain Saadet's single-synth MIDI input handling.

## Included

- A polyphonic synthesizer of one or two parts with Prophet, FM6, CZ-1 and JUNO-106 engines.
- Native tone editing, SysEx import/export, factory sounds and 384 shared user sound slots.
- Arpeggiator, HOLD, panel sustain, chords, strum, scales, microtonal tuning and MIDI expression.
- Native 24-bit audio, stereo USB playback and synth-master capture at 44.1 kHz.
- USB, TRS and Bluetooth MIDI. GLOBAL > BLE manages the phone/computer link;
  GLOBAL > DEV discovers and connects an external Bluetooth MIDI device.
- The last successfully connected outgoing MIDI device is remembered and retried after boot or connection
  loss, with increasing delays up to 30 seconds. Disconnect and Radio Off stop retries; Forget clears it.
- Automatic last-sound recall, complete Saadet backups and a browser-based USB installer.
- Optional m-bootloader 1.2 recovery, installed separately after the application.

The firmware uses MPL-2.0 with the retained third-party terms documented in LICENSING.md. The editor and
some tools retain GPL-3.0-only. The downloadable source includes the matching application and pinned
fm1-base/felucca-bsp source; vendor factory sound data keeps its own attribution and terms.

## Installation

Open [the installer](https://keremimo.github.io/saadet/install/) in Chrome or Edge on a supported computer, connect the FM-1 by USB and follow
the installer. Back up any sounds you want to keep. m-bootloader is optional and off by default. If selected,
it is written after Saadet; keep USB and power connected until completion. See BUILDING.md for recovery.

Saadet is a single-part synth. Installing and saving over Melodee can replace its former project A data;
Melodee project backups are not Saadet backups. Preserve a backup before switching firmware families.

## Known limits and validation

Devices that rotate their private Bluetooth address may need to be selected again. Incoming phone/computer
connections are initiated by the host; automatic reconnect applies to the outgoing MIDI-device link.

Automated checks cover native engines/audio, MIDI, storage, UI, BLE protocol and reconnect policy, installer
transfer/resume, bootloader handling, source/package consistency and licence notices. Hardware records include
successful displayed-code iPhone pairing and a MIDI subscription on an earlier build.

Final acceptance of this exact 0.2.0 package on an FM-1 remains to be recorded:

1. Hold ARP and play: the notes stay after letting go of ARP, other keys play over them and pressing ARP again
   ends them, with the arp on and off; with nothing kept, a quick ARP tap still opens and pages ARP; MIDI out
   sends the waiting note-offs. Latch REC, wait for the autosave and power-cycle: REC starts off.
2. Verify sustained USB/TRS/BLE clock sync under MIDI input load and check ARP swing and gates.
3. Install and confirm the release identity FM-1_9000200 and the on-screen version.
4. Play and receive BLE notes, sustain and pitch bend; check that disconnect leaves no stuck notes.
5. Reboot and test the remembered outgoing device, retries, manual Disconnect, Radio Off and Forget.
6. Exercise USB/TRS/BLE together while playing all four engines, and check audio continuity under load.
7. Hold a note and add a chord on each engine, with one part and with two layered, at full and at lower MASTER; compare
   the engines' levels. The render time with the added dither has not been measured on a unit.
7. Save/reopen sounds and settings, and check the normal update and recovery path.
8. Two parts on the device, with CPU1 online: every pair of engines at the full budget, in LAYER and SPLIT, with USB audio on.
   Record the longest render against the 2902 us deadline and missed renders. Overload cuts no voice now, so this is the only
   proof the budget is safe. Also: tap HOME, the PARTS pads on an SMK-37, a performance saved, power-cycled and loaded, and
   a saved state from 0.1.x coming back as one part.

The preparation tools do not install or publish anything. BUILD_INFO.json and SHA256SUMS identify the exact
package and corresponding source; VERIFICATION.json records the completed automated preparation checks.
