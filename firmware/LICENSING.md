# Saadet licensing

Saadet is a single-synth fork of Melodee, which is a modified version of Felucca by Leo Kuroshita (@kurogedelic),
Hügelton Instruments (<https://github.com/hugelton/Felucca>). New work is by Kerem Kilic (Ellic Studio). Saadet is
licensed file by file: each source file's `SPDX-License-Identifier` line is authoritative. The licence texts are
`LICENSE` (MPL-2.0) and the files in `LICENSES/`.

## At a glance

| What | Licence |
| --- | --- |
| The firmware's files in the fm1-base build (`platform/`, and what it compiles from `firmware/`): the app, engines, editor protocol, storage, UI and MIDI | MPL-2.0 (`LICENSE`), with permissive third-party portions. The FM6 files with MSFA portions are `MPL-2.0 AND Apache-2.0`. No GPL exceptions remain in this build. |
| Retained source of the retired platform (its update loader, boot stub, OTA path and clock/watchdog HAL) | GPL-3.0-only. No legacy firmware build or flash option is available. |
| The build tools, host tests and web pages, unless a file says otherwise | GPL-3.0-only |
| Bundled and ported third-party code, fonts and SDK files | Their own licences (Third-party material, below) |
| Factory sound data (Roland, Sequential, Casio) | The vendors'. No licence is granted by this project. |

`tools/licence_check.py` runs after every fm1-base build (from `tools/check_base.py`). It lists the repository
files that build compiles, and fails if one is neither MPL-2.0 nor permissive. Its known-GPL exception list is empty.

## Why these files can be MPL-2.0

- **Kerem Kilic's code** (Saadet and Melodee, including keremimo's Felucca commits) is licensed by its author.
- **Leo Kuroshita's code.** He gave permission for anyone to use the Felucca code he personally owns under
  MPL-2.0, in [felucca-bsp issue 4](https://github.com/hugelton/felucca-bsp/issues/4). His notices remain on the
  files. The permission covers only his own code, not other contributors'.
- **keremimo's MIDI clock and transport sync and his TRS ring fix** (Felucca `019e2ad` and `57f2aa0`) were granted
  under MPL-2.0 in felucca-bsp pull request 3.
- **Charles Vestal's code** comes from fm1-base, which is MPL-2.0.
- **ChanceTheMaker's MIDI input** (Felucca pull request 8: pitch bend, sustain, RPN and panic handling) has been
  replaced by a clean-room implementation, recorded in [docs/cleanroom/PROCESS.md](docs/cleanroom/PROCESS.md).
  `midi_clock.c` contains none of his lines; its earlier credit to him is corrected.
- **john (@johnskiller)'s system clock work** (pull request 27): his retune code is gone. What remains of his in
  this build is not expressive: the clock header's include guard, an `#include <stdint.h>`, blank lines, and one
  diagnostic assignment in `editor.c`. His register findings are credited as facts in `fm1_clock.h`.
- **Felucca contributors' pull requests checked.** Saadet's history contains Felucca only up to its 1.0 source.
  The code of every other pull request to Felucca was compared line by line with this build's files. Their authors
  are mediaworkbench, ordenador, gospodindilly-ops, selenologist, ugai, echoe, JB-Interactive, spinkham,
  renebohne, ugotworms, jasonpersinger, zednaked and andreahaku. Only generic lines match: language idioms,
  hardware register writes, and Leo's own code that the pull requests moved. `fm1_core1_stop.h` follows Leo's
  Felucca 1.6.1 CPU1 hold. Pull request 247, which it mentions, contributed only the hold's two register writes,
  which are hardware facts.

## Independent replacements of the last four base-build GPL files

| Replaced file | Earlier contribution replaced |
| --- | --- |
| `firmware/src/editor_preferences.c` | ChanceTheMaker's device-preferences and favourites editor commands (Felucca pull requests 10 to 12, through Felucca 1.0) |
| `firmware/src/favorites.c` | ChanceTheMaker's favourites storage (same pull requests) |
| `firmware/src/settings_persist.c` | ChanceTheMaker's favourites persistence (same pull requests) |
| `firmware/src/scratch.h` | Andre Rezemini's shared main-loop scratch, adapted from SLOOP pull request 111 |

On 23 October 2026 all four files were independently replaced under MPL-2.0, using a behaviour-only specification
and a fresh implementer given a tree without the old implementations or Git history. The replacements credit
Kerem Kilic (Ellic Studio). The earlier implementations remain GPL-3.0-only in history; their authors' code has
not been relicensed by this replacement. The specification, separation rules, compatibility checks and verification
are recorded in [docs/cleanroom/preferences-process.md](docs/cleanroom/preferences-process.md).

The base build's 97 repository source dependencies now pass the MPL/permissive check with no GPL exceptions,
including the subsequently added BLE reconnect policy.
The dependency and binary-library licensing information remains in [docs/BLE_MIGRATION.md](docs/BLE_MIGRATION.md);
factory sound data and other third-party material retain the separate terms below.

## GPL-3.0-only elsewhere

The retired platform's source keeps parts by other contributors that no MPL-2.0 permission covers:
the ChoralRoot-derived boot stub and loader, Andre Rezemini's `firmware/src/ota.c`, and john's watchdog read-back
in `firmware/hal/fm1_sys.h` (see Third-party material, below). The legacy firmware is therefore GPL-3.0-only as a
whole. Its build/release commands and local firmware outputs have been removed; only fm1-base BLE, USB-only
and simulator targets are available. Shared files carry their own MPL-2.0 notices. Historical source and
recovery-format helpers keep their original notices and licences.

You may use, study, change and share the GPL-3.0-only parts under that licence. If you distribute firmware that
contains them, give your recipients its complete corresponding source under GPL-3.0. That includes devices that
ship with modified Saadet inside. MPL-2.0 files stay MPL-2.0 file by file: anyone distributing them must keep that
licence and make their source available as MPL-2.0 requires.

Copyright notices follow the code's actual origin. Independently written files credit Kerem Kilic. Mixed files
also identify the retained Felucca portions and any other contributor. Unchanged upstream files and third-party
ports retain their original notices.

## JUNO-106 engine (MPL-2.0)

The JUNO-106 engine is new work by Kerem Kilic (Ellic Studio), licensed under the
Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`) file by file:

| File | What |
| --- | --- |
| `firmware/src/j106_patch.h` | The native tone: the instrument's patch-message layout, switch bits, decoding and encoding |
| `firmware/src/eng_j106.c` | The voice, the part's LFO, high-pass and chorus, and the presets (Roland's factory patches themselves are vendor data, below) |
| `firmware/src/j106_edit.h` | Panel field descriptors and edits |
| `firmware/src/editor_j106.c` | Editor command 96 and the instrument's SysEx messages |
| `tests/j106_test.c` | Host tests |

It was written from the Juno-106's published architecture and from its MIDI
implementation as publicly documented (the owner's-manual layout reproduced in
ST-Log issue 33, Peter Cassidy's 1994 analogue-list summary in the Hyperreal
archive, and the hinzen.de Juno-106 SysEx page) and, for the VCA LEVEL
slider's -5..+5 scale, Analogue Renaissance's JUNO-106 test procedure. Those
were read for facts only: message layouts and parameter meanings. No code, tables or data from KR-106,
fm-106, TAL-U-NO, the Felucca forks' Juno engines or any other emulation were read
or used; those are GPL-licensed and incompatible with this file-level licence.
The DSP is generic (polynomial band-limited steps, a trapezoidal four-pole
low-pass, one-pole filters, a modulated delay line) and uses the MPL-2.0 fm1-base
maths already in the tree (`firmware/src/x0x/fastmath.h`, Charles Vestal). Its
times, ranges, curves and chorus rates are engineering estimates, not measurements
of an instrument.

The presets are INIT and Roland's 128 factory patches. Those patches are Roland's
data, kept in `assets/j106-factory/` and generated into a build header; they are
not part of the MPL-2.0 files and no licence is granted for them (see Third-party
material below). Patches a user imports remain the user's material. The hooks
that connect the engine to the existing instrument (engine table, library, state, mixer, panel and
editor) are now MPL-2.0, like the rest of the fm1-base build. The web editor's hooks stay
GPL-3.0-only with the web editor.

## Retained Hügelton Instruments material

The following original material is by Hügelton Instruments (Leo Kuroshita).
Melodee's additions and modifications are credited separately in the source files:

| What | Licence | Where |
| --- | --- | --- |
| Original Felucca portions retained in the firmware, tools, web pages and tests | MPL-2.0 in the fm1-base build's files (his permission, above); GPL-3.0-only elsewhere | Source notices; `LICENSE`, `LICENSES/GPL-3.0-only.txt` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the web editor's icons | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| ChoralRoot FM-1 contributors (<https://github.com/Quixotic7/ChoralRootFM1>, v0.16, commit `9c7445cdd7b767c779741d28654fb59cca3acd97`): historical boot stub, interrupted-update write ordering, commit-marker/CRC compensation, and recovery tests, adapted for Melodee. The shared `CRSTUB` / `CRBM` format is retained in the retired source and recovery-format helpers. | GPL-3.0-only | `firmware/stub/`, `firmware/loader/ldr_core.c`, `tools/fm1pkg_make.py`, `tests/boot_stub*`, `tests/loader_powercut_test.c` |
| Andre Rezemini (SLOOP RAM branch): retained update-path contributions, not covered by Leo's own-code permission. This path remains only in retired source and is excluded from the fm1-base target. | GPL-3.0-only | `firmware/src/ota.c` |
| Archivo font by The Archivo Project Authors (<https://github.com/Omnibus-Type/Archivo>): the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py --preset stage`; the generated tables are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/Archivo[wdth,wght].ttf`, `LICENSES/OFL-Archivo.txt` |
| Rubik font by The Rubik Project Authors (<https://github.com/googlefonts/rubik>): the `rubik` preset of `tools/gen_aa_font.py` (the firmware's text up to the Saadet redesign; the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/Rubik[wght].ttf`, `LICENSES/OFL-Rubik.txt` |
| Inter Tight font by The Inter Project Authors: the `inter-tight` preset of `tools/gen_aa_font.py` (the firmware's text was Inter Tight up to Melodee 0.13) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| MSFA (Google Inc., Pascal Gauthier, Jean Pierre Cimalando): DX7 synthesis, scaling, envelopes, algorithms, LFO, frequency/sine data, controllers and portamento, from explicitly Apache-2.0 source files. Saadet uses native fp32 operators and its own voice lifecycle. MARK I/OPL renderers and tables and the Dexed-style shadow mono bank have been removed. Sources and exact revisions are in `docs/BLE_MIGRATION.md`; Dexed's GPL engine files were never retained. | Apache-2.0 upstream portions, in firmware files that are MPL-2.0 AND Apache-2.0 (`tools/gen_tables.py` stays GPL-3.0-only with the tools) | `firmware/src/fm6_core.c`, `firmware/src/eng_fm6.c`, `firmware/src/fm6_msfa_ops.h`, `tools/gen_tables.py`, `LICENSES/Apache-2.0-msfa.txt` |
| fm1-base by Charles Vestal: explicitly MPL-licensed single-precision maths, imported at `93bee3fe9def08127dd5dff638df5bd57cebcc80`; Kerem's unsigned exponent-bit adjustment avoids a signed-shift issue. This replaces the earlier GPL copy of the same helper. | MPL-2.0 | `firmware/src/x0x/fastmath.h`, `LICENSES/MPL-2.0.txt` |
| MAME uPD933 device model by Devin Acker: native CZ phase functions, envelope rate law and logarithmic amplitude adapted to C with native fp32 playback | BSD-3-Clause | `firmware/src/cz_native.c`, `LICENSES/BSD-3-Clause-uPD933.txt` |
| Casio CZ-1 preset tones A-1..H-8 (Casio Computer Co., Ltd., 1986): the 64 factory sounds as native tone data, combined from the "64 original CZ-1 patches" SysEx (GeoCities CZ-101 page, archived 2009) and Oli Larkin's VirtualCZ conversions of the same presets (LCD names, line levels, velocity). Included as sound data for CZ-1 compatibility; the rights stay with Casio. See `assets/cz1-factory/README.md` | Casio's (no licence granted) | `assets/cz1-factory/cz1-factory.syx`, `tools/make_cz1_factory.py`, `tools/gen_cz1_factory.py` |
| Sequential Prophet-5/10 factory programs v1.03 (Sequential, 2025; <https://sequential.com/support/download/prophet-5-10-sounds/>): the 200 programs of `P5_Factory_Programs_USER_v1.03.syx`, unchanged native data, as PROPHET's factory presets and its default native user collection. Included as sound data for Prophet-5 compatibility; the rights stay with Sequential. See `assets/prophet5-factory/README.md` | Sequential's (no licence granted) | `assets/prophet5-factory/prophet5-v1.03.syx`, `tools/gen_prophet_factory.py` |
| Roland Juno-106 factory patches A11..B88 (Roland Corporation, 1984): the 128 factory patches as patch messages, decoded from two recordings of Roland's factory data cassette (sunshine-jones.com, hinzen.de) and checked against hinzen.de's patch file, as JUNO-106's presets; names from the factory list (synthmania.com). Included as sound data for Juno-106 compatibility; the rights stay with Roland. See `assets/j106-factory/README.md` | Roland's (no licence granted) | `assets/j106-factory/j106-factory.syx`, `tools/make_j106_factory.py`, `tools/gen_j106_factory.py` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| JieLi AC79 SDK by JieLi Technology: three external files go into every `.fwsc` package; the optional RAM recovery helper is in `assets/recovery/` | Apache-2.0 | `LICENSES/Apache-2.0.txt` |

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages (Apache-2.0)

A current `.fwsc` package made by `tools/build_base.py` through the pinned fm1-base packager
(the package the web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); those three SDK files are not in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

They are licensed under the Apache License, Version 2.0 (`LICENSES/Apache-2.0.txt`), not under
the GPL, and their copyright stays with JieLi Technology. The SDK has no NOTICE file. Apache-2.0
files may be distributed together with Saadet's MPL-2.0 and GPL-3.0-only code.

`tools/prepare_public_release.py` prepares a versioned firmware package, matching source ZIP and installer
site. `tools/make_site.py` includes Saadet's licence texts, dependency/font notices and a source-download
notice alongside the packages. Public sites require a clean source tree and a matching build receipt/source
archive. The ZIP supplies MPL-covered source under MPL-2.0; other files retain their own terms and notices.

## Standalone ROM recovery tool

The optional `fm1-recover` executables include `assets/recovery/wl82loader.bin`,
an unmodified RAM recovery helper from JieLi's AC79 SDK under Apache-2.0. Its
source version and checksum are recorded in `assets/recovery/README.md`; it is
loaded into RAM, not installed in flash. Copyright remains with JieLi Technology.
This SDK file is included in the source tree and the recovery distributions;
the three SDK files in firmware packages are still read from an external checkout.

Recovery archives carry Melodee's GPL-3.0-only licence, the SDK's Apache-2.0 licence,
and the bundled Python/PyUSB/PyInstaller/libusb notices. libusb is dynamically
loaded under LGPL-2.1-or-later; its corresponding source archive is included,
and `--libusb PATH` allows using a replacement library. PyInstaller's bootloader
exception permits distributing the generated executable under Melodee's licence.

## No vendor firmware material

No M-VAVE material is part of Melodee. The firmware links no vendor code and contains no vendor
data, and the packages hold only Melodee and the three SDK files above. The installer's
"Return to official V15" holds only the official file's size and SHA-256: you select the
official firmware file you downloaded yourself, and it is checked and installed in your browser,
never uploaded or redistributed.

## Contributions

Contributions are welcome under the licence of the file they change. New firmware files are MPL-2.0.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. "Juno-106" and
"Roland" are trademarks of Roland Corporation; JUNO-106 names the instrument this
engine's tones are compatible with, and Saadet is not affiliated with Roland. Melodee is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners, nor by Hügelton Instruments.

## Removed material

SID/reSID code, its measured waveform assets and generators, and PHASE/CrispyZebra code have been removed from the current tree. Their notices still apply to historical revisions that contain them. CZ-1 now has independent runtime state and reset handling around the BSD-3-Clause native renderer.

## Radio

Saadet's default fm1-base target enables Bluetooth MIDI, including the peripheral and central roles.
`./build.sh --no-ble` produces the USB-only target. Wi-Fi is not enabled.


## fm1-base target (updated 23 October 2026)

`platform/`, `tools/build_base.py` and `tools/base_overlay.py` create a separate
local fm1-base/felucca-bsp build. Dependency revisions and retained notices are
recorded in `docs/BLE_MIGRATION.md`. New files keep their stated per-file
licences and Kerem Kilic (Ellic Studio) attribution; included legacy modules
keep their actual licences and contributor notices. CPU1 and overload policy
adaptations identify their retained Saadet/Felucca provenance.

This target excludes the retired ChoralRoot boot stub/update transaction and `firmware/src/ota.c`.
The contributed MIDI input and the last four GPL app modules have independent MPL-2.0 replacements;
the MIDI clock credit was corrected from source history. The compiler-dependency check now finds all
97 repository source files MPL/permissive, with no GPL exceptions. The app manifest declares MPL-2.0.
See the clean-room records under `docs/cleanroom/` for provenance and verification.

JieLi's linked Bluetooth libraries retain their own terms and notices, recorded in the pinned fm1-base
dependency's `LICENSING.md`. The vendors' factory sound data remains separate from the source-code licence;
this project does not grant rights to that data. Completing source relicensing does not establish hardware
release qualification; the preparation tools supply the release-site licence/source bundle described above.

The optional recovery bootloader is **m-bootloader 1.2** by Julius Vitkauskas
(<https://github.com/jvitkauskas/m-bootloader>), under MIT. The pinned fm1-base
dependency provides its unmodified FM-1 installer and boot image. Base builds
copy these into `platform/build/bootloader/` with their full original MIT
licence and a source/checksum manifest. It is installed separately from Saadet;
its copyright and licence remain Julius Vitkauskas's. The Saadet preparation
and installation wrapper is new MPL-2.0 code by Kerem Kilic (Ellic Studio).
