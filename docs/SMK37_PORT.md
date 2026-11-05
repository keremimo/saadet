# Saadet on the SMK-37 Elite / Pro: the port plan

Status (2026-11-04): a plan and a UX design with every question decided (section 7, `SMK37_UX.md` section 8). The
board layer and Saadet's first-boot build exist (`tools/build_base.py --board smk37 --model elite`, section 5 steps 1
to 3) but nothing has run on a unit. This records how Melodee
was ported to the SMK-37, what of that Saadet can take and what it cannot, and the work in order. The UX itself is
[SMK37_UX.md](SMK37_UX.md); its mocks are `docs/smk37-ux/mocks.html`.

## 1. How Melodee did it

Melodee's SMK-37 port (`Felucca` checkout, nightly; `SMK37.md`, `docs/SMK37_UX.md`) is one firmware with the SMK
behind a compile-time board flag, the FM-1 build byte-identical. Its parts, in the order they were built:

| Part | Melodee | Where |
| --- | --- | --- |
| Board flag and models | `BOARD=smk37 ./build.sh`, `MODEL=pro\|elite\|mke`. One firmware, three package identities (`SMK-37 Pro_900`, `SMK-37 Elite_900`, `MKE-P37_900`) so the stock updater still takes each unit's own package. The installer refuses a package for another model. | `build.sh`, `melodee.c` `MELODEE_ID`, `usb.c` (USB product `Melodee SMK-37`) |
| Input HAL | `hal/smk_input.h` keeps the FM-1 HAL's API (`fm1_in`, `fm1_input_init/tick/scan`, `fm1_enc_take`, `fm1_input_edges`, `fm1_led_key`) so the UI compiles unchanged, and adds velocity, 16 pads with pressure, 4 faders, 2 wheels, the pedal and the battery. 10 kHz TIMER5 tick, 16 ticks a frame. | `hal/smk_input.h` (785 lines) |
| Display and pad LEDs | LCD on SPI1 group A (PB6 CLK, PB7 DO, PC8 CS, PB5 D/C), no backlight pin. The 16 RGB pads (WS2812 class) hang on PC10, SPI1 group B: `fm1_padled_send` switches SPI1 over for one frame between LCD transfers. | `hal/fm1_lcd_hw.h` |
| Flash map | 1 MiB, the stock v016 region table. App area 0x9D000 bytes. Never written: 0x00000-0x03FFF (boot), 0xC0000-0xC0FFF (BTIF, pairing), 0xC1000-0xC1FFF (USRTRIM: velocity curves, pad and wheel calibration), 0xF4000-0xFDFFF (USR, stock resources), 0xFF000-0xFFFFF (key_mac). Refused at compile time and again in the flash driver. The stock marker sector 0xEA000 is released before the first write so stock rebuilds its mapping after a return. 60 store sectors instead of 79. | `hal/fm1_flash_map.h` |
| TRS jack | MIDI OUT only (UART1 TX, PC7): the USB MIDI out stream again, 256-byte ring, one byte a tick. No MIDI IN: PH8 is a matrix row. | `smk_midi_out.h`, `hal/smk_uart_tx.h` |
| Panel map | Printed label to FM-1 function: FX, SCALE=SCL, CHORD=ENV, NOTE REPEAT=LFO, PARA=EDIT, GLOBE=GLO, PATCH=HOME, BT=SAVE, ARP, SEQ, SEQ PLAY=PLAY, SEQ REC=REC, OCTAVE-/+. KNOB BANK, FADER BANK, `<`, `>` are the SMK's own. K1..K4 the page's knobs, K5 MASTER, K6 SELECT, K7 ALGORITHM, K8 PRESETS. HARDWARE CALIBRATION re-teaches it. | `panel.c` `PANEL_DEFAULT` |
| Stub and loader | The boot stub scans OCTAVE- through the 595 chain by hand; the loader refuses a package not laid out for the SMK (FAILED -15/-16/-17) before erasing. | `stub/stub.c`, `loader/` |
| UX layer | Knob and fader banks with a picker and soft pickup, pad modes with a colour grammar, momentary pad maps while a button is held, header tags. Designed first in `docs/SMK37_UX.md` with mocks drawn by the firmware's own renderer plus the pads composed in Pillow, then built in phases with a flash budget measured per phase. | `ui_smk.c`, `main.c` `smk_faders` / `smk_wheels`, `ui_input.c` |

The hardware facts came from M. Geisler's reverse engineering of the stock v016 firmware
(<https://github.com/genkigenki/fm1-to-smk37>, a CC BY-NC 4.0 guide; the board layers themselves are GPL-3.0
branches of Felucca and SLOOP). Only the Elite has run Melodee; the Pro and MKE-P37 packages are the same firmware
under their model's name.

## 2. What carries over, and the licence line

Saadet is not the legacy Melodee firmware. It builds on fm1-base and felucca-bsp (MPL-2.0) through `platform/`, and
`tools/licence_check.py` fails the build on any GPL file. That decides the shape of the port:

- **Cannot be copied:** `hal/smk_input.h`, `hal/fm1_flash_map.h`, `smk_midi_out.h`, `smk_uart_tx.h`, the stub's
  SMK scan and `ui_smk.c`. They are GPL-3.0-only, and the HAL and flash map carry M. Geisler's copyright, which no
  MPL permission covers. The genkigenki guide's text is CC BY-NC 4.0, so it is not quoted either.
- **Can be used:** the hardware facts. Pin groups, register addresses, the 595 word layout, matrix codes, mux
  tables, the ADC channels, the trim data layout and the flash region table are facts about the keyboard, credited
  as facts the way `fm1_clock.h` credits john's register findings (see `LICENSING.md`). Every fact is marked
  *verified on a unit* or *from analysis* until Kerem's Elite confirms it.
- **Can be reused as design:** Melodee's UX (its own, Kerem's), the colour grammar, the bank model, soft pickup,
  the header tags, the mock pipeline. Saadet's version is in `SMK37_UX.md`.
- **felucca-bsp has no SMK-37 board.** Its README lists the Elite and the Pro as "Planned. Not started."; fm1-base's
  `build.py` compiles `boards/fm1` only. The board layer is new work, MPL-2.0, in Kerem's forks of both.

Decided (2026-11-04): the board layer is written new from the facts, under MPL-2.0, with no relicensing request to
M. Geisler. His findings are credited as facts in the headers that use them; none of his code is read while the
layer is written (the facts are taken from section 3 and the fm1-to-smk37 README's region table, not from the
GPL sources).

## 3. The hardware (SMK-37 Pro / Elite / MKE-P37)

Same chip as the FM-1 (JieLi AC791N), same second-stage bootloader, update loader and chip key; only the panel
wiring differs. From Melodee's HAL headers and the fm1-to-smk37 README, unless marked otherwise.

| Part | Fact | Status |
| --- | --- | --- |
| Keys | 37 velocity keys C3..C6, two contacts each; the middle 27 (F3..G5) are the FM-1's, the outer 10 are more keys. Matrix rows 0..4. Velocity: the stock two-contact timing against five-point curves in USRTRIM (0xC1000; trimdat at 0xC1224). | verified (Elite, Melodee) |
| Buttons | 18 on rows 5..6: the FM-1's 14 functions plus KNOB BANK, FADER BANK, `<`, `>`. 19 button LEDs in two commons (SEQ PLAY has two colours), commons on the second USB PHY's pads as GPIO (SFR 0x16A0C bits 17/18). | verified |
| Encoders | 8 on row 7, quadrature, one detent = one cycle. | verified |
| Scan | 4 x 74HC595 (32 bits) on SPI2 group C (PA3 CLK, PA4 DO; IOMAP_CON1 bit 17), latch PA1, /OE PA0. Word bits 31..16 the column (active-low one-hot), bits 15..0 the button LEDs (active low) and the analog mux address (bits 3..5). Return rows PA2, PA5, PA6, PA7, PA9, PH6, PH8, PH9 with pull-ups. | verified |
| Pads | 16 FSR pads on two analog muxes (SARADC ch1 pads 0..7, ch8 pads 8..15), velocity from the first samples' peak, pressure after. RGB LEDs, WS2812 class, on PC10 (SPI1 group B) shared with the LCD. Hardware pads 8..15 are the bottom row. | verified |
| Faders, wheels, pedal, battery | Mux C (SARADC ch2): addresses 0..3 faders (1, 2, 3, 0), 4 pitch wheel, 5 mod wheel, 6 the sustain pedal jack, 7 the battery. Wheel travel is short (raw ~445..640 pitch on a unit). | verified |
| Screen | 1.54" 240 x 240, the FM-1's panel protocol, SPI1 group A (PB6 CLK, PB7 DO, PC8 CS, PB5 D/C), always on (no backlight pin). | verified |
| Audio | 3.5 mm headphone/line out; USB audio in stock is mono 24-bit 44.1 kHz. Whether Saadet's stereo USB audio and codec path work unchanged is unmeasured. | analysis (heise review of the stock Pro) |
| MIDI | USB, Bluetooth MIDI (the BT button), TRS-A 3.5 mm as MIDI OUT only (UART1 TX on PC7). No TRS IN. | verified (out), review (BLE) |
| Flash | 1 MiB. Parts differ by unit: 0xEB6014, 0x856014 or 0xB36014 are all accepted, as Melodee does; requiring one left an Elite RAM-only and refusing updates. Region table in section 1. | verified (behaviour on an Elite) |
| Power | 2000 mAh battery; no backlight rail; the matrix and LEDs are always powered. | review |
| Models | Pro, Elite and MKE-P37 are the same hardware; each reports its own model name to the updater. | Melodee: Elite run, Pro and MKE untried |

## 4. Architecture of the Saadet port

One firmware, the FM-1 build unchanged byte for byte, the SMK behind a board option, as Melodee. The layers, bottom
up:

### 4.1 felucca-bsp: `boards/smk37/`

A sibling of `boards/fm1/`, MPL-2.0, Kerem's, written from the facts in section 3. Same header names so `device/`
compiles unchanged with `-Iboards/smk37`:

- `fm1_input.h`: the 595 scan, keys with velocity, 18 buttons, 8 encoders, button LEDs with the two commons; the
  FM-1 API kept (`fm1_in.buttons`, `fm1_in.notes` as the 27 middle keys, `fm1_enc_take`, `fm1_led`), plus
  `fm1_in.keys37`, `fm1_in.key_vel[37]`, `fm1_in.pads`, `pad_vel`, `pad_press`, `fader[4]`, `wheel[2]`, `pedal`,
  `battery`. The analog state machine from the stock behaviour (baseline follow, peak over three samples, release
  below base + hysteresis), written new.
- `fm1_adc.h`: the three muxes behind the 595 address bits; MASTER is not a pot here (K5 is an encoder: a virtual
  master, kept with the settings).
- `fm1_lcd_hw.h`: SPI1 group A pins, no backlight; `fm1_padled_send` / `fm1_padled_done` for the WS2812 frame
  between LCD transfers.
- `fm1_flash.h` ranges: `FL_DATA`, `FL_OTA`, the protected windows, the SMK app area; `_Static_assert`s on every
  window as Melodee has them.
- `fm1_audio.h`: unchanged if the codec path is the FM-1's (to be measured first on the Elite).
- `fm1_uart.h`: UART1 TX only on PC7; RX never enabled (PH8 is a row).
- Its README row moves from "Planned" to the board's facts, as the BSP asks: "pin maps and other board facts are
  added only when they have been checked on a real unit."

### 4.2 fm1-base

- `fm1.toml`: `board = "smk37"` and `model = "pro" | "elite" | "mke"`; `tools/build.py` picks `boards/<board>`,
  the board's linker script (the SMK app area), the package identity (`SMK-37 Pro_9..`, `SMK-37 Elite_9..`,
  `MKE-P37_9..`) and the output name (`saadet-smk37[-elite|-mke].fwsc`).
- `include/plat.h`: the FM-1 enums stay (`NB` 14, `NE` 7, `NKEYS` 27). The SMK adds, behind `PLAT_SMK37`:
  `B_KBANK`, `B_FBANK`, `B_LEFT`, `B_RIGHT`; `plat_keys37()`, `plat_key_vel(k)`; `plat_pads()`, `plat_pad_vel(p)`,
  `plat_pad_press(p)`, `plat_pad_rgb(p, rgb)`; `plat_fader(i)`; `plat_wheel(w)`; `plat_pedal()`;
  `plat_battery()`. The host simulator (`host/plat_host.c`) gets the same with script words (`pad 3 down 100`,
  `fader 1 64`, `wheel pitch 20`), so Saadet's UI tests cover the SMK on a Mac.
- `device/`: the WS2812 frame after `lcd_sync`, the TRS out ring fed from the USB MIDI out path (`plat_midi_send`
  with `TO_TRS`), the pedal's rest level at power-on, `plat_lights` for 19 LEDs.
- `loader/` and the stub: the SMK's OCTAVE- scan by hand through the 595s, the package layout check before erase,
  the USB product string per model (`Saadet Elite SMK-37`, `Saadet Pro SMK-37`, `Saadet MKE SMK-37`, each with
  ` Update` in the loader): the model name is the product's, the SMK-37 family stays in the string for the stock
  updater and the installers.
- `tools/fm1_install.py` already checks the model; `fm1pkg_make.py` takes the identity. The web installer
  (`web/install`, `tools/make_site.py`) gets a channel per SMK model from the first SMK release, beside the FM-1's:
  the model picker reads the connected unit's identity and offers only its own package (decided 2026-11-04).

### 4.3 Saadet `platform/`

- `fm1.toml`: the board and model from `tools/build_base.py --board smk37 --model elite`, in a variant the build
  generates beside the base (`platform/fm1.toml` stays the FM-1's); the overlay (`tools/base_overlay.py`) takes the
  board and says where each patch has an SMK equivalent or is skipped.
- `app.c` `sd_input`: the 14 FM-1 buttons as today plus the four SMK buttons; `sd_input_smk` for keys, pads, faders,
  wheels, pedal; `sd_pad_lights(rgb[16])`; `sd_lights` for 19 LEDs.
- `instrument.c`: `SAADET_SMK37` for the UI, velocity into `trk_note_on` from the keys and pads (today every note
  is one velocity), the pedal into `pedal_down()` beside ARP held, the wheels into the bend and `MODW` paths MIDI
  already drives.
- The store: Saadet's live sectors remapped inside the SMK's data window. Today's objects run 0x93000..0xFFFFF;
  `0xFA000` (UPFM6_EXT B), `0xFC000`/`0xFD000` (two objects) sit inside the protected USR window (0xF4000..0xFDFFF)
  and must move; everything from 0xC8000 up must be checked against BTIF/USRTRIM (0xC0000..0xC1FFF). Saadet needs
  48 legacy sectors plus 6 platform sectors; the SMK has 60 in its pool, so it fits with no slot lost, in a new
  table behind the board flag. Backups are object-based (`saadet-backup` v1), so they restore across the two maps.
  `tests/storage_test.c` enumerates the SMK map against the protected windows as it does the FM-1's.
- `tests/build_targets_test.py` adds the three SMK packages; `tests/ui_test.c` runs again with `SAADET_SMK37` on
  the host stub.

### 4.4 Saadet firmware: `firmware/src/ui_smk.c`

The UX of `SMK37_UX.md`: banks (a bank is one of the existing `ui_stop_t` pages pinned to the knobs or the
faders), pad modes and their lights, the momentary pad maps, the header tag, the pickup gauge, the picker, the
GLOBAL > KEYS page. Included by `ui.c` behind the flag; the FM-1 build has none of it.

## 5. Order of work

Each step ends with a measurable "done". Hardware steps need Kerem's Elite and a full 1 MiB flash backup first
(the stock package for the model and the manufacturer's updater at hand: the way back).

0. **Design and mocks (this).** `SMK37_UX.md`, `smk37-ux/mocks.html`. Then the pixel-exact mocks the way Melodee
   made them: `tests/smk_ux_mocks.c` on `tests/ui_harness.c`'s `ui_png`, the additions drawn with the firmware's
   primitives, composed with the pads by a tool (needs Pillow in the build Python).
1. **Board layer, blind.** `boards/smk37/` in felucca-bsp and the fm1-base build option; the template app builds
   for all three models; the host simulator gets the SMK controls. Done: `tools/build_base.py --board smk37` makes
   a package, the FM-1 package is byte-identical to before, `licence_check` passes.
2. **First boot on the Elite.** Backup, install, screen up, keys and buttons through HARDWARE CALIBRATION, encoders,
   USB MIDI answers. Done: the editor's identity reply and a note played over USB. Measured here: the codec, the
   render deadline on this unit, battery reading, BLE advertising.
3. **Store and recovery.** The remapped store, a backup round trip, a return to the stock package with the
   manufacturer's updater. Done: `tools/fm1_backup.py` restores the FM-1 backup onto the SMK; stock boots after.

   The remap is built and tested on the host, against `boards/smk37/fm1_flash.h` (`tests/storage_test.c`,
   `tests/ui_erase_test.c`), not yet on a unit; the backup round trip and the return to stock remain. An object keeps
   its FM-1 address where that lies in a data window. 48 object sectors and 6 platform sectors, the same objects and
   the same format (`firmware/src/storage.c` `st_sector`, `platform/app.h` `APP_STORE_MAP`):

   | Object | Sectors | FM-1 | SMK-37 | SMK window |
   | --- | --- | --- | --- | --- |
   | Prophet banks (`OBJ_P5BANK0`..+4) | 10 | 0x93000-0x9CFFF | 0xA3000-0xACFFF | `FL_XDATA1` |
   | Last state (`OBJ_SYNTH`) | 2 | 0xA0000, 0xA1000 | 0xA1000, 0xA2000 | `FL_AUTO` |
   | FM6 bank (`OBJ_FM6BANK`) | 2 | 0x9F000, 0xFE000 | 0xE0000, 0xE1000 | `FL_DATA` |
   | FM6 user slots (`OBJ_UPFM6_EXT`) | 2 | 0xE9000, 0xFA000 | 0xE9000, 0xE2000 | `FL_DATA` |
   | Settings (`OBJ_SETTINGS`) | 2 | 0xFC000, 0xFD000 | 0xEB000, 0xEC000 | `FL_GLOB` |
   | Platform pairs (`OBJ_PANEL`, `OBJ_BLE`, `OBJ_BT_LAST`) | 6 | 0xA2000, 0xA4000, 0xA6000 | 0xC2000, 0xC4000, 0xC6000 | `FL_DATA` |
   | Shared slots (`OBJ_SHARED_EXT`) | 2 | 0x9D000, 0x9E000 | unchanged | `FL_XDATA0` |
   | CZ-1 banks (`OBJ_CZBANK0`..+7) | 16 | 0xC8000-0xD7FFF | unchanged | `FL_DATA` |
   | Native FM (`OBJ_NATIVEFM0`, +1) | 4 | 0xD8000-0xDBFFF | unchanged | `FL_DATA` |
   | User presets (`OBJ_UPRESET0`, +1) | 4 | 0xDC000-0xDFFFF | unchanged | `FL_DATA` |
   | User presets, more (`OBJ_UPRESET_EXT0`, +1) | 4 | 0xE5000-0xE8FFF | unchanged | `FL_DATA` |

   Why each moved: 0x93000-0x9CFFF is the SMK's app area; 0xFA000 and 0xFC000-0xFDFFF are its protected USR window;
   0xFE000 is outside every window the board's flash driver writes; the platform pairs need a window the base's store
   accepts (`FL_STORE_OK`). The update loader reads bytes 0xF00.. of a sector as a possible record, so only objects
   that leave their last 256 bytes erased sit in the XDATA windows, which is why the FM6 bank moved to `FL_DATA`. Free
   after this: 0xE3000-0xE4FFF, 0xED000, 0x9F000-0xA0FFF, 0xAD000-0xBAFFF.
4. **Controls.** Velocity from keys and pads (the USRTRIM curves), faders, wheels, pedal, TRS out, pad LEDs.
   Done: each one measured on the unit (USB test scripts for the velocity's timing).
5. **UX phase 1: banks** (knobs, faders, picker, gauge, tags). Ships alone.
6. **UX phase 2: pads** (CHORDS, SOUNDS, the momentary maps; no KEYS mode, by Kerem's call), one commit per mode,
   each tried on the Elite.
7. **Release.** The three packages (`saadet-smk37-pro-X.fwsc`, `saadet-smk37-elite-X.fwsc`,
   `saadet-smk37-mke-X.fwsc`) built and attested, the web installer's three SMK channels, `SMK37.md` for users,
   `RELEASE_NOTES.md`. The Pro and MKE-P37 packages ship marked untried until a unit of each has run them.

Image size is not the constraint it was for Melodee: the SMK app area (0x9D000) is 40 KiB larger than the FM-1's
(0x93000), and Saadet is the smaller firmware. RAM is the FM-1's; the pad state and maps are under 200 bytes.

## 5a. Audit against Melodee's SMK-37 build (2026-11-05, nightly `3fe570d2`)

Every `BOARD_SMK37` branch was read against this port, for what the hardware needs. Fixed: the scan order (rows are read
before the latch, one column behind the 595 bit: the cell codes assume it); the key contacts and re-strike; flash parts
(`0xEB6014`, `0x856014`, `0xB36014`; the Elite here has `0x856014`); the store owner tag at `0xAF000`; update records only
below `0xC0000`; a package for another board refused before anything is touched (-15, -16, -17); the update record's model
name; ADC timing (`0x400D`); the pad baseline; pad LED chain order and a 250-byte reset; the fader filter; wheels that learn
their range; the pedal's rest and re-learn; TRS without SysEx, with a switch; PLAY's LED colours; button release time;
MASTER's level and step.

Saadet: all 37 keys through the normal note path (`KB_N`), velocity and VELOCITY modes, the pedal jack with CC64, wheels with
bend and CC1 out, the CZ-1's wheel vibrato, `<` `>`, GLOBAL > KEYS, HOME's keyboard as the keybed.

Different on purpose: the encoders keep the FM-1's detent decoder (Melodee uses the stock one; same direction by the tables);
the owner check clears the store at once, rather than keeping the stock mapping until the first write; Saadet's store layout
is its own. Keys, buttons and knobs are confirmed on the Elite (2026-11-05). The pads, faders and BANK buttons (`SMK37_UX.md`) are built
and host-tested, not yet tried on a unit. CLOCK still offers TRS, which has no input here.

Install fm1-base packages with the base's own installer, `build/fm1-base-smk37-platform/tools/fm1_install.py`, not
`tools/fm1_install.py`. If the running firmware refuses an update, `tools/fm1_recover.py` writes the app area from ROM update
mode (sysex `F0 22 24 35 7D F7`, needs sudo on macOS; the tool must already be waiting, because macOS bounces the device).

## 6. Risks and unknowns

- **Audio path.** The stock Pro's USB audio is mono; Saadet's base does stereo in both directions on the FM-1. The
  codec pins may differ. Step 2 measures before anything depends on it.
- **Bluetooth.** The chip is the same and the BT button exists on every model, but BLE on an SMK has not run any
  open firmware yet. The base's radio code reads its address from the store object `OBJ_BLE`; the SMK keeps its
  pairing data in BTIF (0xC0000), which the port never writes.
- **MASTER.** The FM-1's pot is K5, an encoder, on the SMK: a virtual master kept with the settings, shown on
  GLOBAL > OUTPUT.
- **Flash wear and the USR window.** Everything above 0xF4000 moves; the new table must leave the stock marker
  sector (0xEA000) alone until the first write, as Melodee does, so a return to stock rebuilds the stock mapping.
- **Velocity on every engine.** Saadet's engines read velocity through the common path (`voice_start`, the
  matrix's VEL, P5 VEL AMP, FM6 operator KVS, CZ-1 line velocity); JUNO-106 had no velocity, so J106 maps it to
  VCA level only when the sound asks (open question in the UX).
- **The installer.** Three browser channels beside the FM-1's: four packages per release, each attested, and
  `tests/site_test.py` and `tests/install_test.py` grow a case per model. The model check is the installer's
  existing one; the risk is release labour, not safety.

## 7. Decisions (Kerem, 2026-11-04)

1. The board layer is written new under MPL-2.0 from the facts; no relicensing request.
2. All three models ship from one firmware: Pro, Elite and MKE-P37, the untried ones marked so.
3. The web installer carries a channel per SMK model from the first SMK release.
4. The product is **Saadet Elite**, **Saadet Pro** and **Saadet MKE** (the model in the name, on the screen's
   boot and GLOBAL pages and in the USB product string), the SMK-37 family kept in the USB string and the package
   identity for the stock updater's sake. The FM-1 stays plain Saadet.

Nothing is open. Step 1 of section 5 can start.
