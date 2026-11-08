# Saadet platform and BLE migration

Work started on 23 October 2026. The separate `platform/` target now builds
Saadet on pinned fm1-base/felucca-bsp with the Bluetooth MIDI SDK linked.
It is an experimental local build. The first authorized flash wrote completely
but failed to return over MIDI. A startup-order fix was installed through
RP2040 recovery with complete flash readback verification. The device now
boots Saadet, answers USB MIDI/editor requests and reports BLE advertising. A prior build also achieved
displayed-code iPhone pairing and a confirmed MIDI subscription. Actual BLE notes/audio, sustained links,
reconnects and bond persistence across reboot still need physical qualification on the release candidate.
See `docs/VALIDATION.md` for hardware validation. The later source-licensing audit and independent
replacements are recorded in `LICENSING.md` and `docs/cleanroom/preferences-process.md`; the base build's
repository sources now pass the MPL/permissive check without GPL exceptions. Third-party libraries and
factory sound data retain their own terms.

## Completed engine preparation

- SID is removed, including reSID waveform inputs, generators and its two
  dedicated fixtures. Stored engine ID 3 is reserved and retired; it is never
  reassigned. Existing SID library entries remain unsupported. Last-sound and
  backup validation reject SID synth state rather than playing a different engine.
- The selectable engines are Prophet (19), FM6 (12), CZ-1 (15) and, since
  23 October 2026, JUNO-106 (21; MPL-2.0, see `LICENSING.md`). ID 20 stays
  reserved: it is the Prophet's user namespace in the editor protocol.
- PHASE/CrispyZebra is removed. Native CZ-1 has its own runtime state and note
  reset and keeps the BSD-3-Clause uPD933 renderer. Shared sine data and maths
  have their own provenance below. Native 144-byte tones remain unchanged. History checks attribute the extracted
  state/init/accessor definitions to Kerem (`5f71930a`, `fa64ef07`, `6da744c7`);
  the new native reset does not extract the PHASE oscillator.
- FM6 renders through the MSFA-style fp32 sine operators. MARK I/OPL operator
  code, their log-sine/exponent tables, and the multi-operator feedback special
  cases have been removed. Only MSFA appears in the renderer control.
- FM6 uses the instrument's common allocation and mono priority, with newly
  written retrigger/legato integration. The Dexed-style 16-entry shadow mono
  bank and its allocation/transfer logic are removed. Released voices may still
  advance their envelope/phase control state without evaluating sample kernels.
- FM6's existing patch formats, engine ID and function-byte layout remain.
  Historical renderer IDs 1/2 are accepted on state import and normalized to 0
  (MSFA). The patch bytes and unrelated controller settings are preserved.
  This does not promise the old MARK I/OPL sound: the rendering and mono/stealing
  policy have intentionally changed. There is no Dexed sample-parity claim.
- The unused undo handle and preserving-history resize code are removed.
- The old effects-related linker minima are replaced by a 16 KiB engine-state
  bank, checked against each supported engine type at compile time. Native
  precision tests use that bank without cache or auxiliary SRAM tails. This
  budget covers engine state only, not UI, USB, code, stacks or a Bluetooth stack.

## The own Bluetooth stack (5 November 2026)

Saadet no longer links JieLi's Bluetooth libraries or the base's shim. `platform/fm1.toml` sets `ble_stack = "own"` and
`ble_pairing = true`. The sections below describe the JieLi build where they mention the SDK, its two links or `le_midi.c`;
their history stands.

**The stack is Melodee's copy, in `platform/ble`,** with fm1-base's link fixes merged in, and `tools/base_overlay.py` puts it
in place of the base's `bt/ble` and of `fm1_ble.h` and `fm1_ble_rf.h`. It is Henri David's stack from Optimist. What Melodee
adds to it is Kerem's (`fef995cd`, `0170859a`, `5320bc0d`, `ca6c5837`), and Kerem took it here under MPL-2.0 on 5 November 2026:

- the MIDI characteristic is read before it is subscribed (`BLE_MIDI_INITIAL_READ`), and only a Write Response counts as the
  subscription's answer;
- an iPhone that refuses ATT while its bond settles gets five tries 200 ms apart, with no second pairing prompt;
- a phone Saadet connected out to can connect back: the keys handed out are derived from one kept secret, and a central with an
  unknown key is told Key Missing, shown as FORGET ON PEER;
- the baseband block, the PLL table and the calibration's buffers in the pool; an iterative start-up replay, not a recursive one.

What the base has and Melodee's copy lacked comes from Henri David's stack (fm1-base `0eec347` and its `bt/`): the window
widening's floor and rounding, the transmit window after a connection update, the steady window, only a good CRC counting as the
peer heard (a switched-off central otherwise stays connected for ever), and a null guard in `ble_att.c`. Every line of
`platform/ble` is either in fm1-base's dual-licensed copy or one of Kerem's: `tests/base_ble_own_test.py` checks the marks of
both, and that the build uses these files. The files carry `GPL-3.0-only OR MPL-2.0`, as fm1-base's do, taken here under
MPL-2.0. `fm1_rf_cal.h`, the radio's measurement (Charles Vestal, Apache-2.0), and the start-up tables stay felucca-bsp's:
they are the same in Melodee.

**`platform/ble_own.h` is the bridge,** on the base's glue (`device/bt_own.c`), with Melodee's rules (`firmware/src/midi_ble.c`):

- **Calibration.** The stock firmware's stored trims when they are still there, a copy kept in `OBJ_BLE` so they outlive that
  sector, else measured at every start, as Melodee does.
- **One link at a time,** a computer's or a device's; JieLi's kept both. A scan or a connection out stops advertising. The pages
  say so (DEVICE LINK IN USE, COMPUTER CONNECTED).
- **The last device is looked for, not called blind:** 10 s of scanning every 30 s, and a link once it is heard, behind a private
  address too (its IRK). Between the rounds a computer can find Saadet, and with GLOBAL > BLE open there are no rounds, which
  stands in for Melodee's VISIBLE mode without a new setting. It is kept with its bond in `OBJ_BT_LAST`; JieLi's `SBT1` record
  reads as none.
- **Never a new pairing loop:** a lost link is looked for again; a refusal, a wrong key or a code nobody entered is not, and the
  page says why (PAIRING FAILED, FORGET ON PEER, NO MIDI SERVICE, CONNECTION TIMEOUT). An attempt gets 10 s, or 45 s while a
  code waits. A device that refuses Just Works gets one new link that asks for the passkey.
- **Settings from Melodee:** stock's radio-tested 27-octet PDUs, no diagnostics. `BLE_DIAG 0` also takes the "BT ll" line off the
  radio's log; set it to 1 in `tools/base_overlay.py` when bringing up a new link problem.

**What changes for a user.** `OBJ_BLE` is the stack's own record, so the radio address changes and old bonds are gone: forget
Saadet on the phone or computer once.

**The rest of the room.** `boot_pack = true` stores every load image LZMA-packed, including `.audio_text`, which the base's option
does not cover: the overlay adds its record and the boot unpacks it into the pool, clearing around it. `APP_SYS_TEXT` draws the
base's own screens in Saadet's Archivo, through Saadet's renderer, so the base's Barlow faces are not linked.

Measured on these targets (the slot is 581564 B on the FM-1, 622524 B on the SMK-37):

| | FM-1 image, free | Elite image, free | Low RAM (FM-1) | Pool (FM-1) |
| --- | --- | --- | --- | --- |
| JieLi's libraries (`2d13622`) | 577308 B, 4256 B | 594452 B, 28072 B | 92 KB | 238 KB |
| The own stack | | 515468 B, 107056 B | 57 KB | 217 KB |
| and every load image packed | 466576 B, 114988 B | 479040 B, 143484 B | | |
| and no Barlow faces, Melodee's copy and settings | 425336 B, 156228 B | 437872 B, 184652 B | 52 KB | 220 KB |

Not measured yet, and owed on a device: the boot's added time unpacking 94 KB, a Mac and an iPhone connecting with MIDI both ways,
a device connecting and being found again, a pairing code, and the render deadlines with MIDI flowing over Bluetooth (the stack
runs in the radio's two interrupts, below the audio and the tick). The simulator still models two links.

## Source records

These upstream revisions were checked directly. MSFA and Dexed checkouts are
provenance references. The pinned fm1-base checkout and its nested BSP are
inputs to the separate platform build; the wrapper copies them into a disposable
overlay and leaves their original sources unchanged.

| Source | Revision | Material and terms |
| --- | --- | --- |
| [Google MSFA](https://github.com/google/music-synthesizer-for-android) | `f67d41d313b7dc85f6fb99e79e515cc9d208cfff` | Apache-2.0 DX7 algorithms/operator model, pitch and scaling, sine/frequency generation, envelopes and LFO. |
| [Dexed](https://github.com/asb2m10/dexed) | `fea2f0f0f2c397799f59b27eb11c038cd34b22c5` | Only explicitly Apache-2.0 MSFA files provide retained synthesis/data references: `source/msfa/{dx7note.cc,env.cc,lfo.cc,pitchenv.cc,controllers.h,porta.cpp}` and MSFA kernel/table files. `source/engine/EngineMkI.cpp` and `EngineOpl.cpp` are GPL and are not retained implementations. |
| [fm1-base](https://github.com/charlesvestal/fm1-base) | `93bee3fe9def08127dd5dff638df5bd57cebcc80` | `include/fastmath.h`, MPL-2.0, Charles Vestal. Imported at `firmware/src/x0x/fastmath.h` with Kerem's unsigned exponent-bit adjustment. The original Charles attribution is restored; no Leo authorship is inferred from shared licensing. |
| fm1-base platform dependency | `3c4a57eea659f942b0c80a99949dddafc094a57b` | MPL-2.0. Upstream's, with the own Bluetooth stack (`bt/`). The earlier fastmath import above retains its original source record. |
| felucca-bsp nested in that base | `41a6e86d921f842243bd01a48111832f7e55b4e2` | The BSP linked by the `platform/` target, with the radio drivers the own stack runs on; the legacy build is retired. |
| [m-bootloader](https://github.com/jvitkauskas/m-bootloader/releases/tag/v2.3) | v2.3, source `6bc8f459b94f75cf8c0a94e4fb018b09b2cc7689` | MIT, Julius Vitkauskas. Unmodified FM-1 `.fwsc` and `.boot` kept in `platform/m-bootloader/` (the pinned base carries 1.2); SHA-256 checked against the author's release. Its full MIT notice accompanies the prepared artifacts. |
| [MAME uPD933](https://github.com/mamedev/mame/blob/master/src/devices/sound/upd933.cpp) | Existing port; see `LICENSING.md` and `docs/CZ1_SYSEX.md` | BSD-3-Clause, Devin Acker. Existing native phase/envelope/amplitude implementation remains, with its notices. No new MAME import was made in this step. |

Source files were inspected, so the new integration is not represented as a
formally separated clean-room implementation. This work uses permissively
licensed source with attribution and independently written instrument policy.

## Implemented platform port

`tools/build_base.py`, which `./build.sh` runs by default, builds the tracked
`platform/` integration against the pinned dependency revisions. `--no-ble`
produces a USB-only target; the default includes BLE MIDI. The old platform's build and flash paths and local
firmware outputs have been retired. Its boot stub, loader and OTA transaction are not linked into the target.
Shared asset generation now lives independently in `tools/generate_assets.py`.

The port includes:

- The three native engines, Q23 mixing, the SRAM audio graph, bounded CPU1
  worker launch/join, fault retirement and retained overload-victim policy.
  CPU1 uses private 8 KiB stacks. Clock-dependent voice admission reads the
  existing PLL settings without changing them.
- The existing UI, raw physical panel calibration, LED duty planes, display
  sleep, saved-object formats and the 384-slot library. The base has separate
  A/B objects at `0xA2000..0xA7000` for panel/radio data and the last MIDI device. Tests enumerate all
  48 live legacy sectors and verify no platform or OTA overlap.
- USB and TRS MIDI plus BLE MIDI peripheral input/output. BLE advertises
  **Saadet**; central mode supports one external MIDI device beside a phone/
  computer link. BLE is started by the base's existing
  boot/crash guard. GLOBAL > TEMPO adds BLE to the clock-source choices.
- Per-port physical note/sustain holds before the shared channel controller
  path. Disconnecting BLE releases its notes, while USB/TRS holds on the same
  pitch survive. Existing per-channel expression remains shared.
- Stereo 44.1 kHz USB PCM16/PCM24 in both directions, using native sample
  rings in POOL SRAM. PCM24 retains low bits; PCM16 capture keeps TPDF dither.
  Synth capture precedes computer playback. The physical output keeps its
  inherited -6 dB ceiling after capture/mixing, its one-bit cut dithered
  (`platform/ceiling.h`). Standard USB playback master
  mute/volume is retained. Endpoint/interface numbering and the playback
  clock servo now follow the base, so physical host interoperability must be
  measured again.
- The existing USB editor/custom SysEx and raw USB DX7/CZ bank parsers. Raw
  byte hooks let full bank dumps reach their bounded parsers even when larger
  than the base's small command-frame collector. Native bank imports and the
  editor are USB facilities; BLE SysEx/editor transport is not implemented.

Editor command 72 (audio diagnostics) answers on this port in its legacy
layout: render time and missed renders from the base's deadline hook, CPU
load, voices, the CPU1 worker and memory; the USB-audio fields read 0, as the
base owns USB audio. The base also keeps its own render/xrun/radio
diagnostics. Existing editor preset, settings and backup commands still use
Saadet's protocol. No physical backup
round trip or BLE connection has yet been measured. The initial migration
required RP2040 recovery; its measured write/readback and subsequent boot are
recorded in `docs/VALIDATION.md`.

The updated dependency also supports m-bootloader as a separate, explicit
installation after the new app. `tools/base_bootloader.py` checks the exact
v1.2 FM-1 hashes before serving the boot-only staging transaction, then requires
the original app identity to return. Build preparation and tests do not write
the device. The new app and bootloader were subsequently installed on this
unit, and the user confirmed the PLAY recovery menu. USB download was briefly
detected, but macOS denied kernel-driver detach and the device returned to
Saadet. The user deferred further terminal checks; end-to-end direct USB
recovery on this Mac remains unverified.
The device installation receipts are recorded in `docs/VALIDATION.md`.
Installation order and keys are in `BUILDING.md`.

## Computer and phone connection repair (23 October 2026)

Reference: [Charles Vestal's fm1-ctl](https://github.com/charlesvestal/fm1-ctl)
`e181b4a1ac1510ce18ebf6e271a143c2fe364ccd`, especially its Bluetooth UI and
`docs/MANUAL.md` connection/troubleshooting sections. That app pins fm1-base
`b16c177a9086be955cd3e418383f2fd92c6d658b`; Saadet's newer base already
contains the same MIDI service, optional Just Works pairing and bond storage.
No separate authentication protocol or central-device scanner was imported.
Existing Charles Vestal and JieLi notices remain in the dependency overlay;
Saadet's integration additions carry Kerem's credit.

The connected unit's pre-change log reports `post: no task sys_timer`
repeatedly after advertising starts. The BLE-only shim has no such task, so
SDK timers explicitly assigned to that service never reach their callbacks.
The checked overlay routes those timers to the existing main-loop timer pump,
while preserving other named task owners and the WiFi build's task semantics.
This repairs an observed callback failure; it does not yet establish the
cause of the user's failed physical connection.

Additional connection adaptations:

- GLOBAL > BLE shows advertising, startup/stopped/off, a link waiting for
  MIDI subscription, or MIDI connected. K1 turns the radio off/on; turning
  K4 RETRY clockwise drops a held link and re-enables advertising. These radio
  controls are session-only; BLE still starts at the next normal boot, subject
  to the inherited crash guard.
- A fresh connection clears its notification subscription, restores the
  default ATT payload size, and restarts connection-parameter negotiation.
  A link counts as MIDI ready only after notifications are enabled.
- GAP name reads accept a host buffer larger than the name, support partial
  reads, and return zero beyond the name. Startup diagnostics now say Saadet.
- Turning the radio off during initialization remains effective when the
  asynchronous Bluetooth-ready event arrives.
- The last outgoing device is saved in a separate atomic A/B object only
  after its MIDI subscription is acknowledged. On boot, after the radio is
  ready and a one-second startup delay, Saadet reconnects to that address and
  address type without opening GLOBAL > DEV. Failed attempts wait 2, 4, 8,
  16, then 30 seconds between attempts, continuing at 30 seconds; unexpected
  link loss also retries. Connecting and MIDI setup retain their timeouts.
  Radio Off and an explicit device Disconnect stop retries for the session;
  Radio On or RETRY resumes them. Forget clears the saved target along with
  the bonds, and a failed Forget leaves retries stopped. Unchanged targets
  are not rewritten on each connection. Failed storage writes retry no more
  than once every 30 seconds while connected.
  This applies to the outgoing MIDI-device role. Incoming phone/computer
  links still connect from their MIDI app. Peers that rotate private BLE
  addresses may need to be selected again in GLOBAL > DEV; reconnect does
  not guess identity from a device name.

For a Mac, open Audio MIDI Setup > Window > Show MIDI Studio > Bluetooth and
connect to Saadet. If the page says WAITING FOR MIDI, quit and reopen Audio
MIDI Setup, then use RETRY and Connect again. On iPhone/iPad, connect from a
music app's Bluetooth MIDI settings. The retained Just Works handler accepts
pairing if the host asks; the peripheral does not force a PIN exchange.

Tests execute the patched timer source, radio-ready handler, ATT read and
connection/disconnect helpers with controlled IO under address/undefined
behavior sanitizers. The UI fixture checks all displayed states and text
layout. The host scenario exercises the actual platform bridge, link-only
recovery, disabling/re-enabling, reconnecting and release of BLE-held notes.
These are software checks. The repair was subsequently committed as
`dd9d0f8` and installed on the FM-1. Its running identity returned, the current
boot reports advertising as Saadet with no recorded crash, and the missing
`sys_timer` warning no longer appears after the latest startup marker. A
physical BLE connection remains unverified. Installation receipts and the
package hash are in `VALIDATION.md`.

## Direct MIDI devices and forgetting (23 October 2026)

The default now enables fm1-base's `ble_central` configuration and `APP_BTDEV`.
It reuses Charles Vestal's unchanged `lib/btdev.{c,h}` at the pinned base
revision, already used by CTL, for discovery selection, connection actions and
events. Its existing MIDI client handles service/characteristic discovery,
subscription, connection retry and search timeouts. No CTL UI drawing code was
copied. Saadet's new bridge/UI code is credited to Kerem; the upstream helper
and JieLi-derived service retain their original notices.

GLOBAL > DEV scans while open, shows up to eight devices in a scrolling list,
and uses K1 to choose and SAVE/K4 to connect or disconnect. An established
connection survives page changes. The peripheral role stays available to a
phone/computer. Saadet's keys send to both BLE destinations. The two incoming
links carry distinct internal source tags through physical note/sustain
ownership, then share the existing BLE clock-source selection. Losing either
link releases only its holds, preserving USB/TRS and the other BLE link.

The checked overlay also accepts enhanced connection-complete events in the
client, resumes advertising after cancellation, prevents cancellation from
re-entering the retry loop, and respects the client's advertising pause during
phone-link RETRY. Connection requests retain the upstream no-forced-pairing
policy with optional Just Works acceptance when the peer asks.

GLOBAL > BLE adds a phone-only DROP control and FORGET ALL PAIRINGS. Forget
requires a clockwise K3 arm and a SAVE press; a backward turn, leaving the page
or 15 seconds cancels the arm. After confirmation, both links close and the
SDK's documented `ble_list_clear_all()` runs only when no link or connection
attempt remains, after a short settling interval. It clears the SDK's shared
master/slave bond table, not Saadet sounds, calibration, or the radio address.
The radio resumes its previous requested state on completion/error. Failure
to reach quiescence within six seconds reports FORGET FAILED. The SDK API's
return value controls success/failure reporting. External automatic reconnect
and selectively forgetting one saved bond are not implemented.

Tests cover the real parser's source tags, separate overlapping holds and
sustain, both roles together, MIDI output with only the client connected,
shared BLE clock selection, page-driven scanning, and connection actions.
The target adapter tests confirmation, expiry/cancellation, waiting for links,
SDK success/failure and timeout handling. Both SDK role configurations exercise
the actual bond-clear guard/API delegation. UI fixtures verify all new screen
states and scrolling. Real radio interoperability and persistent bond removal
still require physical peer validation; software tests do not prove those.

## Licensing boundary (updated 23 October 2026)

The retired legacy source retains its GPL-3.0-only terms; it has no supported build or flash path.
The base target's repository sources are now
MPL-2.0 or permissively licensed, including independent replacements of the contributed MIDI input,
preferences, favourites, persistence and shared scratch. The process records under `docs/cleanroom/` document
the replacements; the earlier implementations in history keep their existing licences. The BLE target also
links JieLi's Bluetooth binaries through the base; those libraries and factory sound data retain separate terms.

Leo's [recorded permission](https://github.com/hugelton/felucca-bsp/issues/4#issuecomment-6216224984)
says anyone may use Felucca code for which he personally owns the copyright
under MPL-2.0; it also identifies his PHASE waveform code as MIT in
Felucca-Engines. It expressly excludes other contributors' code. This record is
broader than an assumption based only on which files a downstream base copied,
but is not blanket permission for the entire Felucca/Melodee tree.

Audit outcome and remaining release validation:

1. The source audit covered all 96 repository files compiled by the base target. ChanceTheMaker's MIDI input
   was independently replaced in `637cbbd`; the credit in `midi_clock.c` was corrected using source history.
   The last four GPL exceptions were independently replaced subsequently. `tools/licence_check.py` checks
   actual compiler dependencies on every base target build and now permits no GPL exceptions. See
   `LICENSING.md` for provenance, third-party material and factory-data terms.
2. Measure the move from the legacy CRSTUB/update layout to the base's entry
   at `0x02000120`, including interrupted updates, reboot/recovery and saved
   library/state retention. The old ChoralRoot-derived loader and Andre
   Rezemini's retained `firmware/src/ota.c` remain only in retired source.
3. Measure actual FM-1 radio calibration/startup, connection loss, simultaneous
   USB/TRS/BLE traffic, physical keys/LEDs/display, native SysEx/editor/backup,
   USB host-volume and format changes, sustained polyphony and DMA deadlines.
   Simulator output and linked footprint cannot establish these properties.
4. Finish the editor audio-diagnostics integration and any issues found during those hardware checks.

The subsequent user-authorized flash attempt and startup repair are recorded
in `docs/VALIDATION.md`. These source and build checks do not establish the remaining hardware properties.

## Validation

The engine preparation uses the existing target build and full host suite,
including native Q23, concurrent worker parity and sanitizer checks. Additional
integration coverage exercises historical FM6 renderer-state imports and SID
state rejection. The MSFA routing fixture compares the new operator plan with
the retained pre-migration MODERN plan over all algorithms, feedback settings
and changing gain states; legacy renderer arguments map to MSFA.

Current run results and footprint belong in `docs/VALIDATION.md`. Logs are
`build/migration-build.log`, `build/migration-tests.log`, and `build/host/log/`.
The header/notice inventory is `build/migration-provenance.json`; it is an
inventory aid, not a substitute for contributor and source-history checks.

The platform-specific checks live in `tests/run_base_tests.sh` and
`tests/base_port_test.py`. They exercise the patched USB stream/descriptor and
volume-control routines under address/undefined-behavior sanitizers, validate
storage ranges, and drive the base host through all three engines, BLE note and
sustain disconnects, USB/TRS overlap, keyboard MIDI output and BLE clock-source
selection. `tools/check_base.py` verifies the actual target symbol map and
packed resident-code bytes. See `docs/VALIDATION.md` for run results.
