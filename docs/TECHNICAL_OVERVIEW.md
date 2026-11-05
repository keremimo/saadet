# Saadet technical overview

A two-part synthesizer fork of Melodee for the M-VAVE FM-1, with Bluetooth MIDI.

New to Saadet? Start with [the quick start](../QUICKSTART.md).

Saadet starts at Melodee `nightly` commit
`b5148ba` (the committed upstream tip fetched on 22 October 2026). The original
checkout and its uncommitted work are separate. `upstream` is read-only locally.
Saadet's public repository is [keremimo/saadet](https://github.com/keremimo/saadet).

[Install Saadet](https://keremimo.github.io/saadet/install/) ·
[Releases and downloads](https://github.com/keremimo/saadet/releases) ·
[Web editor](https://keremimo.github.io/saadet/editor.html)

- Up to two polyphonic synth parts, each Prophet, FM6, CZ-1 or JUNO-106: one part, or both by MIDI channel (MULTI), split
  or layered. They share one voice budget, each guaranteed an equal share of it. With one part, part B costs nothing.
- 12 named performances: both parts' sounds and how they are played.
- Arpeggiator, HOLD, chords, strum, scales, microtonal tuning, modulation and MIDI performance.
- Native sound editing, factory libraries, SysEx import/export and 384 shared user sound slots.
- Stereo USB playback and stereo synth-master capture at 44.1 kHz, with 16/24-bit USB formats.
  Native rendering and the common audio path use 24-bit precision; USB format selection is independent.
- Automatic last-sound recall, device settings, complete Saadet backups, USB update and recovery.

There are no further tracks, solo/multitrack mixer, drum engine,
sequencer, patterns, songs, recording, motion automation, send buses, punch effects, projects, project templates,
MPC bank-to-track routing or USB stems.

The on-device interface provides engine editing, envelopes, modulation, scales,
chords, arp, library, favourites, saving and SysEx import. Single native SysEx
tones audition immediately and support live editing; SAVE keeps them in the
library. The FX button opens LIBRARY, where K3 SEND exports a sound over USB
in its native format; insert effects are removed. User sounds can be deleted with K4 DELETE
and an FX confirmation. GLOBAL's last page can erase all saved data after arming
K1 and holding GLO for three seconds. See [the panel guide](UI.md) for the
controls and cancellation gestures. The installer enters update mode automatically. With m-bootloader
installed, hold PLAY at power-on for its recovery menu, or REC for USB download mode.

Build and test instructions are in [BUILDING.md](../BUILDING.md). Serve the repository
locally and open [the editor](../web/editor.html) for Sound, FM6, Library and Settings.
The [manual](../web/manual/index.html) describes the instrument. [The web installer](../web/install/index.html)
installs a release from Chrome or Edge, with m-bootloader as an option; `tools/make_site.py`
builds its release site (see BUILDING.md).

The 0.1.0 release preparation workflow is in BUILDING.md; changes and the final hardware acceptance steps
are in [RELEASE_NOTES.md](../RELEASE_NOTES.md). Prepared bundles include the matching firmware source and licence
notices, so users do not need access to this checkout's Git remote to obtain the source.

## Storage and compatibility

Saadet stores its last complete synth state, both parts, in an A/B pair at `0xA0000` and
`0xA1000`, formerly part of Melodee project A. Installing and saving Saadet can
therefore replace that project's data. A state from before the parts loads as one part, its sound on part A.
Performances take six more A/B pairs, two performances each, at `0xA8000`..`0xB3FFF` (on the SMK-37, `0xAD000`..`0xB8FFF`),
also formerly Melodee project data: saving a performance can replace it. Preset and settings identities remain in
place for the inherited 384-slot migration. Old library phrase bytes and unsupported legacy entries are inert. Reserved protocol/parameter numbers
remain reserved; they do not enable the removed features.

Saadet backups use `saadet-backup` version 2 and include settings, all 22 shared
sound banks, synth state object 32 and performance objects 33..38. A version 1 backup restores and leaves the
performances alone. Object 28 remains a preset bank. Project,
song, pattern and sample objects are rejected. Melodee project/backup files are
not Saadet backups. Settings retain an opaque legacy tail for compatibility,
without exposing project-template functions.

## Validation and provenance

The host suite exercises native engines, controls, MIDI ownership and repeats,
native 24-bit transport, power-loss recovery, archive round trips, USB transport,
panel navigation, sound deletion and protected full-storage erasure. The target build checks RAM placement and firmware packaging.
These checks do not establish physical audio quality, control behavior or
sustained buffer deadlines on an FM-1. No hardware installation is part of this fork's creation.

New Saadet/Melodee work is by Kerem Kilic (Ellic Studio). Retained Felucca work
is by Leo Kuroshita (@kurogedelic), Hügelton Instruments, with its notices kept.
JUNO-106 (engine 21) is a new MPL-2.0 engine written for Saadet from the
Juno-106's published architecture and MIDI implementation, without code or data
from other Juno emulations: one DCO per voice with saw, pulse/PWM, sub-octave
square and noise, a resonant four-pole low-pass, one ADSR, the delayed triangle
LFO, then the instrument-wide high-pass and stereo chorus I/II. Tones keep the
instrument's 18-byte patch layout; its patch messages import over USB and its
single-control messages edit the playing tone. Its presets are Roland's 128
factory patches (A11–B88), decoded from recordings of the factory cassette and
kept as Roland's data ([assets/j106-factory](../assets/j106-factory/README.md)).
Times and ranges are engineering estimates;
neither the sound nor its CPU cost has been measured against an instrument or
on an FM-1. See [LICENSING.md](../LICENSING.md#juno-106-engine-mpl-20).
SID is retired with its stored engine ID reserved. FM6 uses the MSFA renderer;
legacy MARK I/OPL settings load through it and can sound different. CZ-1 is
independent of the removed PHASE/CrispyZebra engine. BLE/platform migration
status and provenance are in [the migration record](BLE_MIGRATION.md).

The initial reduction was adapted from earlier Melodee synth-only work, then
updated against this nightly's native engines, FX, shared library and drawing code.
See [LICENSING.md](../LICENSING.md), [LICENSE](../LICENSE), and [LICENSES](../LICENSES/)
for applicable upstream and third-party terms.

The `platform/` target ports Saadet to **fm1-base + felucca-bsp** with BLE MIDI
and is the firmware build (`./build.sh`); the original platform is retired.
BLE, USB-only and host simulator targets are available. The port is still experimental. It builds
locally and has simulator coverage. After recovery from a failed first boot, the corrected
firmware now boots on the FM-1, answers USB MIDI/editor requests and reports
BLE advertising. An earlier build completed iPhone pairing and a MIDI subscription;
physical audio/control and BLE acceptance of the exact release package remain pending.
The platform also prepares m-bootloader 1.2 for a separate, checked installation
to add recovery at power-on; see the installation order in BUILDING.md.
See [BUILDING.md](../BUILDING.md#fm1-base--ble-port-the-default-build) and
[the migration record](BLE_MIGRATION.md) for the licensing and validation
boundaries.
