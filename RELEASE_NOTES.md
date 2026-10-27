# Saadet 0.1.1

MIDI clock drift fix for the M-VAVE FM-1, based on Saadet 0.1.0.
The audio rate remains 44.1 kHz.
This is an experimental release; the outstanding hardware checks are listed below.

## Fixed in 0.1.1

- Recover MIDI clock pulses discarded during an input queue overflow, keeping USB, TRS and BLE transport
  phase aligned with the selected master while still releasing stuck notes.
- Consume every overdue arpeggiator step after a clock burst, retaining swing, residual phase and gate timing.
- Backport the timing changes from Melodee commit `0e3d353`; retain Saadet's single-synth MIDI input handling.

## Included

- One polyphonic synthesizer with Prophet, FM6, CZ-1 and JUNO-106 engines.
- Native tone editing, SysEx import/export, factory sounds and 384 shared user sound slots.
- Arpeggiator, HOLD, chords, strum, scales, microtonal tuning and MIDI expression.
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

Final acceptance of this exact 0.1.1 package on an FM-1 remains to be recorded:

1. Verify sustained USB/TRS/BLE clock sync under MIDI input load and check ARP swing and gates.
2. Install and confirm the release identity FM-1_9000101 and the on-screen version.
3. Play and receive BLE notes, sustain and pitch bend; check that disconnect leaves no stuck notes.
4. Reboot and test the remembered outgoing device, retries, manual Disconnect, Radio Off and Forget.
5. Exercise USB/TRS/BLE together while playing all four engines, and check audio continuity under load.
6. Save/reopen sounds and settings, and check the normal update and recovery path.

The preparation tools do not install or publish anything. BUILD_INFO.json and SHA256SUMS identify the exact
package and corresponding source; VERIFICATION.json records the completed automated preparation checks.
