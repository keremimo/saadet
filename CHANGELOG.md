# Changelog

Each release in short. The full notes for the current release are in [RELEASE_NOTES.md](RELEASE_NOTES.md).

## Unreleased

### Fixed

- **FM-1s with the UC25HQ80 flash** (JEDEC B36014): USB recovery (`tools/fm1_recover.py`) takes them as it takes the
  stock Puya part.
- **Installer**: `tools/fm1_install.py` finds an FM-1 whose firmware reports its flash after its identity (Melodee
  on fm1-base); it said none was connected. `--info` names the flash part when the firmware reports it.

## 1.0.0 (7 November 2026)

Six engines, two parts and a new editor. 0.2.0 was never released; its changes are here.

### Added

- **PPG**: the PPG Wave 2.2, with its original wavetables, all 100 factory programs (both groups, layered or split)
  and the sampled piano and saxophone of TABLE 31. Envelopes and filter follow the 2.2's operating system. User slots.
- **HYDRA**: three oscillators on 56 waves with wave scanning, mutants, ring, noise, two filters, 16 factory sounds and
  eight named macros on every sound. Voices are budgeted from what the patch costs before a note starts.
- **HYDRA user waves**: 14 of your own, drawn or imported in Saadet Studio or shaped on the panel in EDIT > WAVE LAB.
- **Hydrasynth import**: ASM SysEx dumps, .hydra banks and .patch files become HYDRA sounds (approximate).
- **Two parts**: by MIDI channel, split or layered, sharing the voice budget; tap HOME to change part.
- **Performances**: 12 named setups of both parts, saved and loaded on the panel and in Saadet Studio.
- **Eight macros on every sound**, in two rows of four; SELECT on HOME for the second row.
- **ARP drone**: hold ARP as a latching sustain pedal; the keys play over what it keeps.
- **Saadet Studio**, the new editor at <https://saadet.ellicstudio.com>: every engine, the library, imports, parts,
  performances, device settings and backups. It follows edits made on the panel live.
- **SMK-37 (beta)**: Saadet Pro, Elite and MKE builds with all 37 keys, velocity, pedal, wheels, pads, knob and fader
  banks and an 8 KNOBS bank. Only the Elite has run it.

### Changed

- Bluetooth runs on an open stack instead of JieLi's libraries, freeing about 150 KB of flash. One link at a time;
  pair again once after installing.
- The limiter takes only what goes over -2 dBFS (was -5); the settled level rises from -6 to -3.6 dBFS. CZ-1 plays
  6 dB quieter, level with Prophet and FM6.
- The output's cuts to whole samples are dithered.
- REC's latch starts off at power-on.
- Builds pin Pillow and fontTools so the same source gives the same package hash.

### Fixed

- Overload no longer cuts voices; the voice budget is bound before a note starts.
- A note that takes a voice no longer cuts it: the old voice fades over 23 ms.
- Prophet: no click on a re-pressed key in its own tail, and no thump or click when voices stop.
- CZ-1 and FM6: no click on a key struck in its own tail; MONO fades the last note over 5.8 ms.
- USB recordings no longer repeat frames under a heavy voice load (2.9 ms more recording latency).
- A restart Saadet makes on purpose no longer counts as a failed boot that could leave the second core off.
- Without the second core, HYDRA plays fewer voices instead of running late.
- The installer resumes an interrupted switch back to Melodee with the unit's own update loader.

## 0.1.1 (27 October 2026)

### Fixed

- MIDI clock pulses lost in an input queue overflow are recovered, keeping USB, TRS and BLE transport in phase.
- Every overdue arpeggiator step is played after a clock burst, keeping swing, phase and gate timing.

## 0.1.0 (23 October 2026)

The first release: a single-part synthesizer for the M-VAVE FM-1 with Prophet, FM6, CZ-1 and JUNO-106 engines,
384 shared user sound slots, native 24-bit audio, USB, TRS and Bluetooth MIDI, the web editor and a browser installer.
