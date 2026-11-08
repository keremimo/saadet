# Saadet quick start

Saadet turns the M-VAVE FM-1 into a polyphonic synthesizer of one or two parts with five engines: P5 (Prophet-5), FM6 (DX7),
CZ-1, J106 (JUNO-106) and HYDRA (wave scanning). This page gets you from a stock FM-1 to playing and saving sounds.

## 1. Install (about 5 minutes)

You need a computer with **Chrome or Edge**, and a USB **data** cable (a charging-only cable won't work).

1. Open the installer: **<https://keremimo.github.io/saadet/install/>**
2. Plug the FM-1 straight into the computer (no hub) and switch it on. Close other music apps and editor tabs.
3. Press **Find FM-1**. When the browser asks to use MIDI devices, choose **Allow**.
4. Optional: tick **Also install m-bootloader**. It adds a recovery menu that works even if a firmware won't start.
   Leave it off if you're unsure; you can add it later from the same page.
5. Press **Install**, read the checklist, and start.
6. **Keep the tab open and in front** until the page says **Done**. Don't unplug the cable or switch the FM-1 off.
   Its screen shows UPDATING, or stays dark: both are normal.

If an installation stops, nothing is lost: the FM-1 waits in update mode, even after a power cycle. Open the
installer again, press **Find FM-1**, then **Resume installation**.

## 2. Play

| Control | What it does |
| --- | --- |
| **PRESETS** knob | Browses the current engine's sounds. A sound loads when the knob rests. |
| **ALGORITHM** knob | Moves to the next or previous engine. The screen takes on the engine's colour. |
| **HOME** | The sound, the chord you're playing and the scale. **K1–K4** are the sound's macros; turn **SELECT** for its other four. |
| **VOLUME** | Master level. |
| **OCT− / OCT+** | Shift the keyboard an octave. Press both together to return to 0. |

Every button opens a place; tap it again (or turn **SELECT**) to turn its page. Hold a button to list what's
inside on the keys, then press a key to jump there. Holding **ARP** is the exception: it is a sustain pedal that
latches. Press **ARP** again to let go of what it keeps.

### Two sounds at once

**GLOBAL > PARTS**, K1: MULTI gives each part a MIDI channel, SPLIT divides the keys at the SPLIT note (below it part B), LAYER
plays both. Then tap **HOME** on HOME to change part: everything edits the part named in the header. The parts share the voices,
half each, and lend what they aren't using.

## 3. Shape the sound

- **EDIT**: the engine's own panel (oscillators, operators, filter and so on). **ENV** and **LFO**: envelopes,
  modulation and the four-route matrix.
- **SEL** (SCALE): scale and root. **SEQ** (CHORD): one-finger chords, voicing and strum. **ARP**: arpeggiator; hold it
  and play to keep those notes as a drone you can play over, until you press it again.
- **REC** latches the notes you let go of until you play the next chord. **PLAY**: voice mode, glide and unison.

## 4. Save and find sounds

- **SAVE**: choose a slot, spell a name with CURSOR and LETTER, star it if you like, then press **SAVE** again.
- **FX** opens the **LIBRARY**: favourites, your sounds and every engine's factory sounds. K2 stars a sound.
- Hold **HOME** to put your favourites on the keys.
- With two parts on, turn **SELECT** in SAVE to the PERFORM page to keep both parts as a named performance. LIBRARY's PERFORM
  list brings it back.
- The current sounds and settings are kept automatically and come back at power-on.

## 5. The editor and backups

Open **Saadet Studio** at **<https://saadet.ellicstudio.com>** in Chrome or Edge, connect the unit by USB and press
**Connect**. It edits every engine on a big screen, keeps your library and sets up parts and performances. **Back up**
under Device before every update: a backup holds all your sounds, favourites and settings.

## 6. Bluetooth MIDI

- **GLOBAL → BLE**: connect a phone or computer to "Saadet" (Mac: Audio MIDI Setup → MIDI Studio → Bluetooth).
- **GLOBAL → DEV**: connect one Bluetooth MIDI keyboard or controller.
- One link at a time: a computer or a device, not both.

Details are in [the panel guide](docs/UI.md#bluetooth-experimental-platform-build).

## 7. If something goes wrong

- **Install stopped**: reopen the installer, **Find FM-1**, **Resume installation**.
- **FM-1 won't start**: with m-bootloader, switch off, hold **PLAY** and switch on for the recovery menu (or hold
  **REC** for USB download mode). From m-bootloader 2.2, choose **MIDI MODE** there (from 2.3 it opens by itself when the
  firmware won't start), then **Find FM-1** in the installer: it installs Saadet through it. Without m-bootloader,
  see the installer's "If the FM-1 won't start".
- **Crashes**: don't switch off. Open the installer, "Report a problem", and read the crash report.
- **Back to M-VAVE's firmware**: the installer's "Go back to M-VAVE's firmware" section.

The full control map is in [docs/UI.md](docs/UI.md); [README.md](README.md) covers storage, compatibility with
Melodee, and what has and hasn't been validated on hardware.
