# Saadet on the SMK-37 (beta)

Saadet also builds for the M-VAVE SMK-37: **Saadet Pro**, **Saadet Elite** and **Saadet MKE** (for the SMK-37 Pro, the SMK-37
Elite and the MKE-P37). It has only run on an Elite so far. The Pro and MKE packages are the same firmware under those names.

The panel guide, [UI.md](UI.md), still applies. This page is what the keyboard adds.

## Controls

| SMK-37 | Saadet |
| --- | --- |
| Keys C3..C6 | 37 velocity keys, through the scale, chords, strum, octave and arp. Keys always play notes |
| K1..K4 | The four values at the foot of the screen, or the knob bank's four |
| K5 / K6 / K7 / K8 | MASTER / SELECT / ALGORITHM / PRESETS. With the 8 KNOBS bank, the sound's other four macros |
| FX, SCALE, CHORD, NOTE REPEAT, PARA, GLOBE | LIBRARY, SCALE, ENV, LFO, EDIT, GLOBAL |
| PATCH / BT | HOME / SAVE |
| ARP / SEQ / SEQ PLAY / SEQ REC | ARP (held: sustain, kept as a drone until pressed again) / CHORD / PLAY / latch |
| OCTAVE- / OCTAVE+ | As printed. Both at power-on: hardware calibration |
| `<` `>` | The previous or next page. On HOME, the previous or next sound. With the pads on SOUNDS or FAVS, the previous or next 16 |
| KNOB BANK / FADER BANK | Tap: the next bank. Held: the picker, on the screen and the pads |
| KNOB BANK + FADER BANK together | The pad bank: what the pads are. Tap: the next. Held: the picker |
| Faders 1..4 | The fader bank's four: at power-on the envelope, ATTACK DECAY SUSTAIN RELEASE on most engines |
| Pitch and mod wheels | Bend (two semitones; the Prophet and FM6 use their own range) and the mod wheel. On the CZ-1 the mod wheel is a vibrato unless the matrix uses it |
| Sustain pedal | A plain foot pedal: the notes you let go of while it is down sound on until you lift it. ARP's drone is apart from it. Plug it in before power-on, or hold it down for a minute once |
| TRS jack | MIDI OUT: what USB MIDI sends, without SysEx. There is no MIDI IN on this jack |

## Banks

A bank is four values that K1..K4 or the faders reach from any place: PAGE, MACROS, FILTER, ENV, LFO, ARP, CHORD and MIX. The
knobs have a ninth, 8 KNOBS: every knob is the sound's, K1..K4 its first four macros and K5..K8 its other four, so MASTER, SELECT,
ALGORITHM and PRESETS wait until you pick another bank (MASTER is also K4 of MIX, and GLOBAL > OUTPUT). On HYDRA the eight are the
sound's own macros, named and routed by it (LFO > MACRO 1–8). The cards show the row you turned last. The
header names the knobs' bank while it is in use. HOME keeps the bank. A page you open shows its own four knobs until you tap KNOB
BANK or pick a bank there. LIBRARY, SAVE and GLOBAL always keep their own.

A fader is away after its value changes some other way. It takes over when it crosses the value, so the sound never jumps. While a
fader moves, the screen shows its bank's cards for a moment: a bar for the value and a tick for where the fader stands.

Hold NOTE REPEAT while moving a fader for an edit that lasts as long as the button.

## Pads

- **CHORDS**, wherever you play. The top row sets the chord type: OFF DIA3 DIA7 MAJ MIN DOM7 MAJ7 MIN7. The bottom row plays the
  scale's degrees, I to VII and I an octave up, with the chord, voicing and strum set on CHORD. With OFF, each degree plays
  its single note. Leaning on a pad strums faster, up to twice the rate set.
- **SOUNDS**. Sixteen sounds of the engine playing, or of the open list on LIBRARY. A tap loads one at once. `<` `>` turn the page.
- **FAVS**. The same, with your favourites.
- **PARTS**. Top row: part A, part B, then on the right the four modes ONE, MULTI, SPLIT and LAYER. Bottom row: the first eight
  performances. In SPLIT, the chord pads play the part you are on; in LAYER, both.

Press KNOB BANK and FADER BANK together to choose what the pads are. Tap both for the next, or hold both and tap one of the first
four pads. On HOME your choice stays, even through power-off. On any other page it lasts until the page changes, and the page
opens with its own pads: SOUNDS on LIBRARY, CHORDS everywhere else. The header names the bank while it differs from the page's own.

| While | The pads are |
| --- | --- |
| A place's button is held | What it lists. A tap jumps there, and the keys play on |
| PATCH is held | Your favourites |
| SCALE is held | A piano. A tap sets the root |
| On SAVE | Letters. A tap sets the letter under the cursor, another within a second cycles it |
| A BANK button is held | The top row the banks |
| Both BANK buttons are held | The first four pads: CHORDS, SOUNDS, FAVS, PARTS |
| A question waits (an import, a delete, an erase) | All breathing orange. The answer is the button's |

The pads take the engine's colour. Dark ember: nothing there. Dim: something there. Full: the one in use. White-blue: happening now.

## Settings

GLOBAL > KEYS has VELOCITY (NORMAL, SOFT, HARD, FIXED), PADS (their brightness) and TRS OUT. GLOBAL > OUTPUT has MASTER.

## Install

Install it from [the web installer](https://keremimo.github.io/saadet/install/), as on the FM-1: it asks the connected keyboard
which model it is and installs that model's package. Each package is also refused on any other model by the keyboard itself.
Have the manufacturer's package and updater for your model ready: they are the way back to stock. m-bootloader is the FM-1's
only.

The first start clears what other firmware left in the shared store. It never touches the Bluetooth pairing, the factory
calibration, the stock resources or the key area.
