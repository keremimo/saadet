# The Saadet panel

Saadet is an instrument of one or two parts with five engines: P5 (Prophet-5), FM6 (DX7), CZ-1, J106 (JUNO-106) and HYDRA. The
screen takes on the colour of the engine of the part you are on. Every button opens a place, and each place stays where you left it.

## Controls

| Control | What it does |
| --- | --- |
| VOLUME | Master level. |
| SELECT | Turns the page of the place on the screen. On HOME: clockwise, K1–K4 edit the sound's other four macros; back, the first four. |
| PRESETS | Browses the engine's sounds: its factory sounds, then your sounds on it. The sound loads when the knob rests. |
| ALGORITHM | Browses the next or previous engine's sounds. |
| K1–K4 | Edit the four values at the foot of the screen. On HOME they are the sound's macros, four of its eight. |
| OCT- / OCT+ | Shift the keyboard by an octave. Press both to return to 0. Hold both for 5 s to enter update mode. |

Tap a place's button again to turn its page. Hold a button to list what is inside it: the list also lights up on the
keys, and pressing a key jumps to that entry. While a button is held, the keys do not play notes. ARP is the
exception: holding it is the sustain pedal, and the keys play.

## Places

| Button | Place | Inside |
| --- | --- | --- |
| HOME | HOME | The sound's name, engine and slot. Under the name, the chord you're playing and its notes (a slash chord when the lowest note isn't the root); after you let go it stays, faded, until you play again. Before you play anything it shows the key. A keyboard from C2 to B5 marks the scale, with the sounding notes lit. The status line shows the arp, chord keys, latch and sustain. Turn K1–K4 to change the macros. Hold HOME to list your favourites on the keys; pressing a key plays that favourite. |
| FX | LIBRARY | Lists: FAVOURITES, USER, P5, FM6, CZ-1, J106, HYDRA. K1 picks a sound (it loads at rest), K2 stars it as a favourite. User sounds also have K4 DELETE. |
| SEL | SCALE | Scale, root, key mode, shift; the MPC degree. |
| ENV | ENV | The engine's own envelopes (P5 filter and amp; FM6 pitch EG; the CZ-1's six 8-stage envelopes; the J106's one ADSR with its VCA mode, VCF amount and polarity; HYDRA's ENV 1 and what it moves), then the common MOD ENV (on HYDRA, AMP ENV: the voice's amplitude). |
| LFO | LFO | The engine's own LFO (P5 LFO, FM LFO, CZ-1 vibrato, J106 LFO rate, delay and its DCO and VCF depths; HYDRA has the common one alone), the common LFO, and the four-route matrix. HYDRA's macros follow it: MACRO 1–8, each a NAME, its VALUE and up to three destinations with their depths. |
| EDIT | EDIT | The engine's own panel. P5: oscillators, mixer, filter, poly mod, wheel mod, play. FM6: voice and algorithm, OP1–OP6, macros, function settings. CZ-1: tone, detune, line 1 and line 2. J106: DCO waves and modulation, VCF cutoff and modulation (with the HPF), VCA mode, level and chorus. HYDRA: OSC 1 with mutants 1 and 2, OSC 2 with mutants 3 and 4, OSC 3, the mixer, FILTER 1, FILTER 2. A value that has no effect on the sound as it stands is shown dim, but you can still edit it. |
| GLO | GLOBAL | Tempo, output, the parts, tuning, MIDI channels and thru, voice budget, display, erase everything. |
| SAVE | SAVE | Choose a slot, set the name letter by letter (CURSOR, LETTER), and star it if you like. Press SAVE again to store. |
| ARP | ARP | Pattern and feel. Its LED flashes with the arp's notes. Tap ARP to open it or turn its page. Hold ARP as a sustain pedal that latches, a drone: the notes you play or let go of while it is down keep sounding after you let go of ARP, and the keys play over them, until you press ARP again. That press ends them instead of opening the page, and what you play while still holding it becomes the new drone. Striking a kept note again restarts it, and it stays. With the arp on, the arp keeps playing the kept notes. While notes are kept, HOME shows SUSTAIN and the LED stays lit (with the arp on, it flashes with the arp). Loading another sound ends them. |
| SEQ | CHORD | Chord, voicing, strum. |
| PLAY | PLAY | Voice mode, glide, priority, unison. |
| REC | (latch) | Holds the notes you let go of until you play the next chord, or until you press REC again. With the arp on, it holds the arp's notes instead. |

## Parts

Saadet plays one part until you ask for two. **GLOBAL > PARTS**: K1 MODE, K2 PART, K3 SPLIT, K4 B CH.

| MODE | The keys | MIDI |
| --- | --- | --- |
| ONE | Part A. | IN CH plays part A. Part B is silent and costs nothing. |
| MULTI | The part you are on. | B CH plays part B; IN CH plays part A (OMNI: every other channel). |
| SPLIT | Below the SPLIT note part B, from it part A. The split stays on the same key whatever the octave. | The same, on IN CH. |
| LAYER | Both parts, each with its own chord, arp and transpose. | The same, on IN CH. |

With two parts, **tap HOME on HOME to change part**, and every page, knob, the library and SAVE follow: the header says PART A
or PART B, and on GLOBAL's OUTPUT the values do: A LEVEL, A PAN, A SPREAD are the part's own. Scale and key mode stay one
for both; ROOT and SHIFT are each part's. Part B's keys go out on B CH. REC, in SPLIT and LAYER, latches both.

The parts share one voice budget (**GLOBAL > VOICES**): each is guaranteed half. A part may borrow what the other is not using
and gives voices back, its oldest first, when the other needs them. Overload never cuts a voice.

A **performance** keeps both parts' whole sounds, the mode, the split, B CH and the part you were on, in one of 12 slots. It does
not keep the tempo or IN CH and OUT CH. With two parts on, SAVE has a second page, PERFORM: turn SELECT to reach it, then choose a
slot, spell a name and press SAVE. LIBRARY's last list, PERFORM, loads one when K1 rests; K4 and FX delete it.

## Bluetooth (experimental platform build)

Saadet has one Bluetooth link at a time: a phone or computer connected to it, or
one external Bluetooth MIDI device it connected to. The link carries MIDI both
ways, and keys played on Saadet go to it. USB remains available. While a computer
is connected, GLOBAL > DEV shows COMPUTER CONNECTED and does not scan; while a
device is connected, GLOBAL > BLE shows DEVICE LINK IN USE and a computer cannot
find Saadet. Disconnect one to use the other.

**GLOBAL > DEV** scans while that page is open, and a computer cannot find Saadet
during the scan. Put the external keyboard,
controller or synth in its Bluetooth MIDI pairing/advertising mode. K1 DEVICE
selects a discovered device; SAVE or a clockwise turn of K4 LINK connects it.
A device that does not answer within 10 seconds is given up, so a computer can find
Saadet again. The page shows CONNECTING, FINDING MIDI, then CONNECTED after the peer
acknowledges its MIDI notification subscription. If the peer requires pairing,
accept its pairing prompt within 30 seconds. If PAIRING CODE appears, enter
that six-digit code on the other device; leading zeros are part of the code.
The page stays on FINDING MIDI or PAIRING CODE until setup succeeds. SAVE/K4 disconnects
an active or pending connection. Leaving the page stops scanning, but keeps an
established connection. Up to eight discoveries can be selected, with three
visible rows that scroll with the choice. This is BLE MIDI only, not Bluetooth
audio or arbitrary Bluetooth accessories.

**GLOBAL > BLE** manages the phone/computer link. K1 RADIO switches the whole
radio off/on for this session. K2 DROP disconnects only the phone/computer;
K4 RETRY drops a stalled phone link and resumes advertising when available.
On Mac, connect to Saadet in Audio MIDI Setup > Window > Show MIDI Studio >
Bluetooth. On iPhone/iPad, use a music app's Bluetooth MIDI settings.
WAITING FOR MIDI means the link is up but the host has not enabled MIDI:
on Mac, quit and reopen Audio MIDI Setup, turn RETRY, then Connect again.

To forget saved bonds, turn **K3 FORGET clockwise** on GLOBAL > BLE, then
press **SAVE** while FORGET ALL PAIRINGS? is displayed. This ends the BLE link
and clears **all** saved BLE pairings on Saadet, preserving the radio address and
sounds. Turn K3 back, leave the page, or wait 15 seconds
to cancel before confirmation. FORGET FAILED means the operation did not
complete; PAIRINGS CLEARED means it was saved. Forget Saadet on
the other device as well if it still remembers the old bond, then reconnect.

Saadet accepts optional Just Works pairing for incoming connections. Its
outgoing MIDI client supports a displayed code for authenticated pairing and requests
pairing when a MIDI read or subscription returns an authentication/encryption
error, without forcing it on devices that do not require it. After boot it looks for the last
device that reached CONNECTED, 10 seconds of scanning every 30 seconds, and connects when it hears it, also behind a private
address. A computer can connect between those rounds, and there are none while GLOBAL > BLE is open. A lost link is looked for
again; a refused pairing or a code nobody entered is not, and the page says why: PAIRING FAILED, FORGET ON PEER (forget Saadet
on the other device), NO MIDI SERVICE or CONNECTION TIMEOUT. Only the peer's subscription acknowledgement shows CONNECTED.

Saadet's Bluetooth is the base's own open stack since this build, so bonds made on an earlier build are gone and the radio
address has changed: forget Saadet on the phone or computer once and connect again.

## Importing SysEx

Send a dump to Saadet over USB-MIDI: a DX7 voice or a 32-voice bank, a CZ-1 tone, a Prophet-5 program, a JUNO-106 tone, a HYDRA patch, or a Saadet
user preset. Saadet opens SAVE and shows the sounds and the slots they will go into. Turn K1 to pick the first slot,
then press SAVE to import. Single native tones also select their engine and
play immediately from RAM, so a librarian can audition and edit without a
save. Repeated native dumps update that sound and retain the chosen SAVE
destination. DX7 voice-parameter and JUNO-106 single-control messages edit
their active engine directly, including the pending tone in SAVE.

The web editor also reads ASM Hydrasynth patches: a SysEx dump (.syx), an ASM Manager bank (.hydra) or one patch (.patch).
It converts each into a HYDRA sound, as near as HYDRA goes: the oscillators (the waves by family), mutants, ring, noise,
mixer, both filters, ENV 1, the level and the eight macros with their names, destinations and depths; ENV 2 becomes the amp
envelope. LFOs, ENV 3–5, most of the matrix, the arpeggiator and the effects are left out, so a sound played from the ribbon,
the wheels or an LFO alone may be silent. Import them on the HYDRA panel, into the library, or straight into the bank's slots.

Banks wait for import and do not replace the playing sound. A Saadet preset
starts playing after import. Opening another place cancels the pending import;
an auditioned native tone keeps playing. User library slots are written only
when you press SAVE. Normal idle recall still remembers the playing state.

## Sending SysEx

In any LIBRARY list, select a sound with K1 and turn **K3 SEND clockwise**.
`SOUND SENT` confirms that its SysEx was sent over USB-MIDI. The selected
sound uses its native Prophet, DX7, CZ-1, JUNO-106 or HYDRA format; a saved sound
without native data uses Saadet's preset format. When the selected sound is
the one playing, the dump includes its current native edits. Sending does
not save or load a sound. Empty lists have no SEND control.

For J106, select **FM-1** for both MIDI input and output in the app, and match
its channel to Saadet's GLOBAL > MIDI OUT CH. Open LIBRARY's J106 list,
select a tone, then turn K3 SEND. J106 receives one tone at a time; its SAVE
button keeps the received tone in its own library. Opening J106 does not
automatically retrieve Saadet's factory or user library.

Native edits are MIDI-compatible tone data; Saadet's additional modulation,
scale and performance settings are controlled through its own editor protocol.

## Removed

The insert effects are gone. Sounds that were saved with effect settings still load, but those settings are ignored.

## Deleting a sound

In any LIBRARY list, select a user sound and turn K4 DELETE up. A white header asks to delete its slot, and the FX
button breathes. Tap FX to delete. Turn K4 back, move SOUND, change page, press another button, or wait three seconds
to cancel. Factory sounds have no DELETE control.

Deleting clears the saved slot and its favourite. The sound playing keeps playing with its current edits, without
a saved slot; the next sound in the list does not load automatically. A failed write shows DELETE ERROR and keeps
the record. There is no undo.

## Erasing everything

Go to the last GLOBAL page, ERASE. Turn K1 ERASE ALL up, then hold GLO for three seconds. The countdown replaces
the page list. Letting go cancels without turning the page; turn K1 back or leave the page to cancel the armed
state. A GLO press that began before arming never counts toward confirmation.

The device shows progress while erasing all user sounds, favourites, stored engine banks, settings, panel
calibration and the sound restored at power-on. Keep power connected until it finishes. It then restarts with
defaults. Built-in factory sounds and the firmware stay. If this FM-1 required hardware calibration, calibrate
it again by holding OCT- and OCT+ at power-on. Erasing has no undo.

## Battery reference

`tools/battery_calibrate.py --capture-full` reads 31 battery ADC samples over USB-MIDI and saves their median as
this device's full-charge reference. Run it with the battery fully charged. The reference is stored with device
settings and included in backups; erase everything clears it. It does not produce a percentage until the empty
reference has also been measured. The command requires firmware with battery calibration support.


## Experimental base target

On the separate fm1-base target, GLOBAL > TEMPO's CLOCK choices are INT, USB,
TRS and BLE. BLE is a MIDI peripheral advertised as **Saadet** and starts
through the base's boot/crash guard. Physical keyboard notes are also sent to
connected USB/BLE hosts.

**GLOBAL > MIDI K3 THRU** (device setting, default OFF) also sends incoming
MIDI out again: USB to BLE, TRS to USB and BLE, and the BLE link to USB.
BLE input does not go back out over BLE, which would echo it to its source.
When a BLE link drops or THRU is turned off, notes and sustain it passed on
are released at their destination. When an out queue is nearly full,
pressure, pitch bend and continuous controllers are dropped first. SysEx is
not passed through. The native editor and DX7/CZ bank imports use USB.
See `BLE_MIGRATION.md` for current limitations and unmeasured hardware checks.
