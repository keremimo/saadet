# Saadet validation — 23 October 2026

## USB recording holds, `FM1B_UAC_DEEP` (measured on an SMK-37 Elite, 5 November 2026)

The recording's ring is written when a render ends, so a long render ran it dry and its packets repeated the last frame.
`FM1B_UAC_DEEP` (`platform/app.h`) keeps a half more in hand: 128 frames, +2.9 ms of recording latency. The ring stays in
POOL and grows from 4 to 8 KB. On all five targets flash and low RAM do not change; the engine arena gives up 4 KB.

One Elite, a chord held on Prophet's Unicorn Dreams, recorded with sox. The packages differ by the flag alone: `d9f952fd` was
built from a clone that first reproduced `303684bd` byte for byte. Both predate `eb5358b`, whose Prophet code differs slightly.

| Package | Flag | Voices | Length | Average load | Longest render | Holds | Flat runs before the corner test |
|---|---|---|---|---|---|---|---|
| `303684bd` | off | 8 | 26 s | 62 % | 2151 us | 390 | 490 |
| `303684bd` | off | 4 | 26 s | 46 % | 2154 us | 129 | 153 |
| `d9f952fd` | on | 8 | 26 s | 61 % | 2149 us | 0 | 107 |
| `d9f952fd` | on | 4 | 26 s | 46 % | 2148 us | 0 | 34 |
| `303684bd` again | off | 8 | 10 s | 61 % | 2195 us | 157 | 181 |

macOS opens the capture at 16 bits even with `-b 24`, and the PCM16 path re-dithers each sample it writes, so a hold arrives
as the same value +-1 or 2 LSB, not as an exact repeat. A hold is counted as 2 or more steps flat in both channels, with a
channel moving over 8 LSB a step in the same direction on both sides, and reaching that speed at once (the step next to the
run at least 0.6 of the step beyond it). That last test was chosen after seeing these takes. A limit of 0.8 also gives 0 with
the flag on. Against it: 300 holds injected into each 26 s take are found at 88 to 93 %, the takes with the flag on included.

The flat runs left with the flag on have soft shoulders (0.37 to 0.46 where holds measure about 1), so they read as the
signal's own flat spots. Runs of 4 or more steps with the flag off fall at whole packets from each other (phase
concentration 1.00), as a hold's end must.

In the host simulation of the ring (`tests/base_port_test.py`) the old ring underruns once a render passes 60 to 70 % of its
half, and the deep one holds through a whole half.

Not measured: an FM-1, a package built from `eb5358b`, a played performance, and the firmware's own underrun counter, which
no USB command reads.

## Limiter level, CZ-1 level and dither (host only, not installed)

Melodee's `a93dd50f` (the limiter from -5.2 to -2 dBFS, CZ-1 down 6 dB) and `94fe79a8` (TPDF dither on the cuts to whole
samples), ported to Saadet's Q23 path. Both are Kerem Kilic's commits. `tests/master_output_test.c` covers them at Q23 and
in the Q15 fixtures' precision. Reverting each piece, one at a time, fails it: 26 mutations in all.

The limiter keeps Saadet's own 46 ms hold (`92a9091`), which Melodee does not have. Saadet applies MASTER before the
limiter and Melodee after it, so below full MASTER Saadet's limiter does less than Melodee's. MASTER is cut where it is.

Gain the limiter takes, in dB, before and after (the mean of 9 factory sounds and the worst one; LAYER: 4 + 4 notes):

| | Triad, MASTER 4096 | Triad, MASTER 2048 | Six notes, MASTER 4096 | LAYER, 4096 | LAYER, 2048 |
| --- | --- | --- | --- | --- | --- |
| Prophet | -2.8 / -6.3 -> -0.9 / -3.1 | -0.1 / -0.4 -> 0 | -6.0 / -10.6 -> -3.1 / -7.4 | -7.7 -> -4.6 | -2.1 -> 0 |
| FM6 | -3.0 / -6.2 -> -1.1 / -3.0 | 0 / -0.2 -> 0 | -7.3 / -11.5 -> -4.5 / -8.3 | -3.5 -> -0.3 | 0 -> 0 |
| CZ-1 | -10.9 / -13.7 -> -2.2 / -4.5 | -5.0 / -7.7 -> 0 | -16.0 / -19.8 -> -6.8 / -10.6 | -14.0 -> -4.8 | -8.0 -> 0 |
| JUNO-106 | -7.6 / -12.7 -> -4.6 / -9.5 | -2.2 / -6.7 -> -0.6 / -3.5 | -12.0 / -17.7 -> -8.9 / -14.5 | -10.4 -> -7.2 | -4.3 -> -1.1 |

A chord can still be limited at full MASTER. JUNO-106 is not changed and stays above the others: a single note peaks about
4 dB over Prophet and FM6. The base simulator (`platform/tests/basic.txt`) reads, in dBFS after the -6 dB ceiling: Prophet
-14.6, FM6 -15.0, CZ-1 -13.3 (was -11.4), JUNO-106 -10.5 (was -11.9, the limiter now passing its peak).

The cuts dithered, in the shipped path: Prophet, FM6, CZ-1 and JUNO-106 voices; the JUNO-106's low shelf, high-pass and
chorus; the part's level; MASTER (exact at full); and the -6 dB ceiling's one bit, which takes one random bit a frame
(`platform/ceiling.h`), so it has no offset and leaves an exact 0 alone. A cut's LSB is Q23's: about -138 dBFS, 48 dB below
Melodee's Q15 cuts. No physical noise floor was measured, and the added render time was not measured on a unit.

The FM-1 BLE package had 844 B of flash left and has 124 B left: 581440 of 581564 B. To fit, the three pan paths share one
level kernel (`mix_level` in `mix.c`), which works in place, and adds a loop per part per block; the JUNO-106's chorus uses
one dither row for both its cuts; and MASTER moved into `master_samples`, so `mix_l` no longer carries MASTER.

Not dithered, as in Melodee: pan, the limiter's gain and the master curve, which rounds. `uac_play`'s `(int32_t)` in the
pinned base cuts only the computer's audio. The base's post-tap MASTER is not reached, since Saadet never calls
`plat_usb_audio_full`. `voice_amp` in `dsp.c` is only used by retired engines. `usb_audio_stream.c` and `audio.c` are
not in the fm1-base build (`MELODEE_USB_AUDIO` 0); the shipped PCM16 capture has its own dither in `tools/base_overlay.py`.

`tests/fm6_test.c` now reads the key scaling depth by RMS: at about 25 LSBs a peak reads the dither.

## Panel SysEx send and native live editing (built, not installed)

J106 3.4.2 lists Saadet's USB MIDI endpoints as `FM-1`. After the user selected
them, sending the current tone through existing editor command 96/op 4 made
J106's receiver show `A11`. This confirms a real outbound tone was received
by that app on the installed firmware. It does not validate the new panel
control or new incoming live-edit behavior below. The app's
[connection guide](https://www.jarvik7.net/juno-106/) describes individual tone
reception rather than automatically retrieving the instrument's whole library.

Every nonempty LIBRARY list now exposes K3 SEND. Turning it clockwise exports
the selected factory or user sound in its manufacturer's native format. The
playing selection includes its current native edits. Sending does not load
a selection or write a user slot; disconnected USB and transfer errors have
explicit feedback. The new sender uses the existing native encoders and
retains each engine's INIT/factory ordering and the configured output channel.

Validated single Prophet, DX7, CZ-1 and JUNO-106 messages now select their engine
and update its RAM tone immediately. Repeated native buffers preserve held
notes for ordinary parameter edits and retain the chosen SAVE destination.
DX7 voice-parameter and JUNO-106 single-control updates also refresh a pending
single-tone import, so SAVE keeps the latest edit. Banks remain staged without
replacing the playing tone; user slots are written only by explicit storage.
Normal idle last-sound recall remains enabled. J106's published manual-buffer
framing and Roland's MIDI implementation were consulted for protocol facts;
no external synthesis code was used.

The ASan/UBSan base SysEx fixture passes actual USB packet paths, exports and
round-trip reception for every engine's first/last factory sounds and INIT,
edited current sounds, stored user sounds, channel selection, empty lists and
offline feedback. It also renders held notes through every native edit-buffer
path and verifies J106-style repeated manual buffers, single-control edits,
destination retention and saving the final edit. The SEND view passes layout
lint and visual inspection (`build/j106-send-ui/library_send.png`).

Host simulator, legacy target and BLE target builds pass. The full host and
base suites end with **ALL HOST TESTS PASSED** and **ALL BASE PORT TESTS PASSED**.
Logs: `build/j106-live-{host-build,legacy-build,target-build,host-tests,base-tests}.log`.
The source/test snapshot `build/j106-live-source.json` has no drift through
completion. It includes the concurrent JUNO-106 audio changes in this checkout,
alongside the existing local BLE and USB receive fixes, at `ui-reset` / `9db8c66`.

BLE package: `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
`6a677f9f3fc25555a2873982c9d36239f0e4628eaf5965c6308cc75cd1595b2a`.
App 566,240 B; low RAM 93,564 B; resident audio 83,920 B; POOL 222,944 B;
engine arena 121,120 B. CPU1, bootstrap, resident-code, packed-image, arena
and boot-layout checks pass. No installation was performed; physical K3 SEND
and live edits from J106 and other librarians require this candidate and retesting.

## USB SysEx patch reception (built, not installed)

The user reported that the DX7II web librarian transferred patches to Melodee
but not Saadet. Comparison with Melodee found a missing USB receive branch:
the fm1-base receiver accepted CIN 4–7 SysEx packets but discarded CIN F
single-byte data and treated single-byte F0/F7 as interruption statuses.
The checked overlay now sends those bytes through the existing native and
framed collectors, while preserving realtime messages and interruption checks.
The matching Melodee fix is `998ca1a3040ed0ca05e25c95a7e10a8bffa791d2`,
authored by Kerem Kilic; retained dependency notices remain intact.

`tests/base_sysex_test.py` executes the actual overlaid USB routines and
Saadet's dispatcher with the production native parsers, import UI and NOR
storage model. The old receiver passes normal packetization, then fails on
the single-byte DX7 voice fixture (`build/base-sysex-regression.log`). With
the correction, DX7 single voices and 32-voice banks pass normal, all-single-
byte and mixed packetization with interleaved realtime clocks. Patches appear
in SAVE, do not write flash before confirmation, and retain their exact native
bytes after saving and cache-reset readback. CZ-1, Prophet, JUNO-106 and generic
preset imports also pass; bad checksums, interruptions, recovery, cancellation
and stray data are covered under ASan/UBSan. This test is in the base suite.

The host simulator and BLE target build pass. `tests/run_base_tests.sh` ends
with **ALL BASE PORT TESTS PASSED**; `tests/run_tests.sh` ends with
**ALL HOST TESTS PASSED**. Target CPU1, bootstrap PC guards, SRAM code, packed
image, arena and boot-layout checks pass. Logs:
`build/sysex-fix-{host-build,target-build,base-tests,host-tests}.log`.

Package: `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
`767a5de6c4ea980d90a7370119d79e63fb2cc65b3f8cc5dddc28a2f820f7878b`.
App 557,824 B; low RAM 93,564 B; resident audio 83,888 B; POOL 222,912 B;
engine arena 121,152 B. Source is `ui-reset` at `9db8c66` plus the existing
local BLE corrections and this SysEx fix. No firmware was installed in this
step; physical retransmission from the user's librarian remains pending.

## Melodee CPU startup and fp32 safeguards

The BLE base port armed `FM1_GUARD_PC` in `fm1_cstart`, before
`app_init` launches CPU1. Melodee already handles this in
`firmware/src/main.c` (introduced in `f12b310` in the Felucca checkout): CPU1
executes chip mask-ROM before reaching the application's worker entry, and
the application-only PC windows trap that bootstrap. Melodee records the
original device fault as DBG bit 10 (`c1_pc_limit_err_r`). The previous Saadet
command 72 report exposes only the fault count, so it does not independently
identify this device's fault bit.

`tools/base_overlay.py` now applies the same ordering: stack, write and bus
guards stay active in `fm1_cstart`; PC limits are enabled unconditionally
after `app_init` returns and before `audio_init`. This also arms the PC guard
when CPU1 is skipped or held after a handshake timeout. Worker validation,
timeout handling and retirement are unchanged.

`tools/check_base.py` rejects the previous generated boot sequence and accepts
the corrected BLE target. It checks that the early guards exclude PC limits,
that CPU1 starts within instrument initialization, and that the PC guard
follows initialization and precedes audio. Existing SRAM placement, packed
image, CPU1 stack and memory checks pass. The MIDI-ingress host fixture now
declares the MIDI-thru setting and callback used by its included header.

Build: `platform/build/saadet.fwsc`, identity `FM-1_900`, source `ui-reset`
at `c4cf703` plus the existing uncommitted work and this fix. SHA-256:
`3494197fda1bdbf81ee60cb06a77ff7be1066fa4f3b22e12b3b94bf5f91f541f`.
Application 555,492 B; low RAM 93,468 B; resident audio 83,856 B; engine
arena 121,184 B.

`tests/run_tests.sh` ended **ALL HOST TESTS PASSED**, including native-Q23
concurrent worker parity and address/undefined-behaviour checks.
`tests/run_base_tests.sh` ended **ALL BASE PORT TESTS PASSED**, including
the repaired MIDI-ingress fixture and command 72 diagnostics.

Installed with `tools/flash.sh --no-build --port FM-1`: the installer finished
at 100%, restarted the device and verified `FM-1_900` running (log:
`build/core1-fix-flash.log`). The first USB reading reports faults 0,
jobs 0, CPU1 offline, missed renders 0 and longest render 239 us. The previous
worker retirement sets `fm1_core1_bar` in `.noinit`, which survives this
update at the same address. A full power-off is required to clear that safety
lock; CPU1 startup, completed jobs and Prophet polyphony verification remain
pending that power cycle. The safety lock has not been bypassed.

The user then reported a crash when playing more than five Prophet voices.
The USB platform log retained CPU0 exception vector 1 at `0x01C51EBA`
(`p5_samples`, a floating-point subtraction) with return address `0x01C4F6D6`
and uptime 6,155 ms (`build/core1-fix-device-log.txt`). After that crash,
CPU1 was barred again. The old USB log did not include its recorded EMU/DBG
cause, so the instruction address alone does not establish the fault type.

A further comparison found another missing Melodee safeguard: CPU0 cleared
only the divide-by-zero trap, whereas Melodee also clears all five floating-
point trap enables (`firmware/hal/fm1_irq.h`, `97357ab`). CPU1 already used
the latter policy. The base overlay now applies it to CPU0 too, preserving
the stack guard and other EMU bits. The USB platform log now includes the
already-recorded EMU and DBG cause words. The worker mailbox, launch, safe
hold, retained retirement and exception-restore assembly match the Melodee
checkout byte for byte; no voice cap or scheduling limit was raised.

`tests/base_cpu_test.py` compiles the generated CPU0 setup with mocked
registers under address/undefined-behaviour sanitizers and checks five initial
EMU states, including all enables set. Floating-point and divide traps are
cleared while stack/unrelated bits, exception setup and branch trace are
preserved. The target checker enforces this policy, and the complete base
suite again ended **ALL BASE PORT TESTS PASSED**.

The combined BLE package SHA-256 is
`94b836980d60159dfcd9fcdc7f53da5c6aee243edbcd5484cfa54310247f2615`;
application 555,604 B, memory sizes unchanged. It was installed successfully
(`build/core1-fix-fp32-flash.log`). Its expanded log read the retained crash
cause as **EMU `0x00000002`, DBG `0`**: SDK `illeg_err`, an illegal instruction,
not a floating-point arithmetic exception (`build/core1-fix-fp32-device-log.txt`).
The fp32 mask remains appropriate Melodee parity, but does not explain that
crash. Stability after a cold start was not established by this package.

The next comparison found that the base only raised VDC14/SYSVDD; Melodee
uses the full stock V15 supply sequence in `firmware/hal/fm1_power.h`, with
DCDC/LDO handover, I/O supplies and final read-back verification. Saadet's
copy of that initializer matched Melodee byte for byte before this change.
The base now includes that same helper through an external-HAL option and
calls it with IRQs off and CPU1 held, before peripherals and app init. CPU1
requires its complete verified result, rather than just two voltage fields.
No PLL/clock retune or cache reconfiguration was added. This closes a concrete
power-initialization difference; it is not yet proof of the illegal-instruction
crash's cause.

The stock cold/warm transaction trace, every transfer timeout and incorrect
read-back checks pass under sanitizers (`tests/power_test.c`, now also in the
base suite). The base CPU test verifies the actual worker startup gate for
verified/unverified power and early failed boots. Target checks enforce power
before peripherals/CPU1; the complete base suite passed again.

Final BLE package: SHA-256
`c5bda602f490ff8fdc2df2866e926c20492b8ce1a038137c3e2a8e2bfa5b641c`,
application 555,860 B, low RAM 93,500 B; resident audio, POOL and engine arena
unchanged. Installed successfully (`build/core1-fix-stock-power-flash.log`):
100% written, restarted, `FM-1_900` verified running. Initial counters: CPU1
offline, jobs/faults/rejections/timeouts/missed renders all zero, longest render
268 us. After the user's full power cycle, CPU1 came online and completed
22,971 jobs with zero worker faults/rejections/timeouts or missed renders.

The first controlled Prophet test reached six held voices without a crash,
then the seventh note stole a voice. The user increased the saved voice budget
to 16. The repeated test then measured 4, 5, 6, 7 and 8 held voices with CPU1
online and completed jobs increasing; no worker faults/rejections/timeouts.
Before that repeat, the device had accumulated three missed-render events
during user operation; the test records this baseline rather than claiming
zero lifetime missed renders. The eight-voice chord then stayed held for 30 s:
CPU1 jobs increased from 56,781 at the stepped eight-note reading to 98,953
after the sustain, then 100,345 after note-off. Faults/rejections/timeouts
stayed zero and missed renders stayed at three (no additions). The longest
render was 2,166 us against the 2,902 us half-buffer budget; longest worker
job 421 us, join wait 163 us. All generated notes received note-offs, and the
final reading had zero held voices. `build/core1-fix-load.log` ends
**DEVICE PROPHET LOAD CHECK PASSED**. The user reported playback okay after
raising the budget. This is a measured cold-boot and short loaded check of
the current Prophet sound, not a worst-case-patch or long-term stability proof.

## Audio diagnostics (editor command 72) on the fm1-base build

Source: `ui-reset` at `c4cf703` with the uncommitted JUNO-106 factory work
below and this change. No device was flashed and no repository commit was made.

Command 72 was compiled only with the legacy USB audio, so the fm1-base build
could not report whether CPU1 came up. It now answers there in the legacy
layout: `audio_late` is the base's missed renders (`plat_xruns`),
`audio_max_us` the longest render its deadline hook measured (flag 1 resets
it), `cpu_q8` the load already kept there, then voices, the CPU1 worker
(online, jobs, job/wait maxima, timeouts, faults), memory (cache RAM reported
disabled) and per-part voice cuts. The base owns USB audio, so fields 0–15 and
17 read 0. `platform/deadline.h` now precedes the editor, whose report reads
its overload counters. `tools/usb_audio_stats.py` finds the port by part of
its name (default `Saadet`).

`tests/base_stats_test.c` (in `tests/run_base_tests.sh`, under
address/undefined-behaviour sanitizers) compiles the editor as the base does
and checks the schema 6 and 2 replies, zero USB fields, xruns, the longest
render and load after three deadline calls, two sounding voices, the CPU1
fields offline and online, memory (190,464 B capacity, the JUNO-106 part in
use, no cache) and the reset. `tests/run_tests.sh` ended **ALL HOST TESTS
PASSED**; `tests/run_base_tests.sh` without `tests/base_port_test.py` (whose
fixture predates MIDI thru; fixed on a separate branch) ended **ALL BASE PORT
TESTS PASSED**. The legacy package is byte-identical to the build before this
change.

| Build | Package | App image | Low RAM | Resident audio | Engine arena | SHA-256 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `./build.sh` (BLE) | `platform/build/saadet.fwsc` | 555,444 B | 93,468 B | 83,856 B | 121,184 B | `8a99307d…40bf` |
| `./build.sh --no-ble` | `platform/build/saadet-usb.fwsc` | 419,272 B | 45,736 B | 83,408 B | 146,208 B | `0ded5f2a…97d8` |
| `./build.sh --legacy` | `build/saadet.fwsc` | 287,412 B | 37,272 B | — | — | `b4f2aef8…568f` |

To read the device after installing (USB):

    build/venv/bin/python tools/usb_audio_stats.py --voices --cores --memory

`core1_online` 1 means the second core is rendering.

Installed on the user's request: `tools/flash.sh --no-build --port FM-1` wrote
`platform/build/saadet.fwsc` (SHA-256 `8a99307d…40bf`) over the running
`FM-1_900`: 100 % written, restarted, `FM-1_900` returned (log:
`build/stats-flash.log`). Two idle readings over USB (port `FM-1`) after the
restart agree: **`core1_online` 0, `core1_faults` 1, `core1_jobs` 0**,
`core1_rejected` and `core1_timeouts` 0; `audio_late` 0, `audio_max_us` 271
then 740, `cpu_q8` 0; no voices; resource capacity 121,184 B, used 1,460 B, no
cache. The fault counter is cleared at boot, so CPU1 faulted once during this
boot without completing a job and was retired: the installed build renders on
one core and PROPHET plays five voices. The fault's address and cause are not
in this report.

## JUNO-106 factory patches

Source: `ui-reset` at `c4cf703` with uncommitted changes: Roland's 128 Juno-106
factory patches as JUNO-106's presets 1–128 after INIT, in place of Saadet's 16
starter tones (removed at the user's request), and VCA LEVEL as the panel's
-5..+5 trim. No device was flashed and no repository commit was made.

Data (`tools/make_j106_factory.py`, see `assets/j106-factory/README.md`): all
three cassette recordings pass their checksums; the two bank A recordings agree
in 62 of 64 records; A74 and A88 come from hinzen.de's recording (the Sunshine
Jones bank's A74 is a copy of its A82); all 128 patches equal hinzen.de's
`Factory_Patches.pat`. `j106-factory.syx` SHA-256 `635c40b1…93f3`.

Host checks (`tests/j106_test.c` in the Q15 fixtures, the shipping Q23 build and
under address/undefined-behaviour sanitizers):

- Presets 1–128 equal the asset file byte for byte as programs 0–127, and each
  tone's name equals its preset name (the web editor shows a tone's own name
  only when they differ). A11 BRASS holds the bytes both recordings hold;
  choosing JUNO-106 plays A11.
- Every preset sounds within a second, stays bounded and ends. FX RISE 2 first
  sounds at 0.28 s (a falling negative envelope opens a self-oscillating
  filter), so the window grew from 0.22 s. FRONTIER ORG (VCA 127, HPF 0) peaks
  at 163,105 in Q15, over the former three voices × 2 × `VOICE_FS`; the bound
  now includes HPF 0's +4 dB shelf.
- Five factory patches (B23 ORCH PAD, B27/B28 FX RISE, B36 FX SWEEP 1, B85
  ROCKET MEN) have VCA LEVEL 0 and were silent under the former linear level.
  The panel scale is -5..+5 (Analogue Renaissance's test procedure sets "VCA:
  +5"), so 0 is now about -6 dB (measured -6.1 dB against 64), 64 and 127
  unchanged. The ±6 dB span is an estimate.
- Envelope law. Played on the device, BRASS had an impact at its start,
  TRUMPET sounded like an electric piano and FLUTES like a pluck. The former
  exponential law between the published ends (attack 1.5 ms–3 s, decay and
  release 1.5 ms–12 s) made the factory decays clicks. With the key held, to
  1 %: PIZZICATO, SNARE DRUM, STEEL DRUMS and MELODIC TAPS 0.01 s, PIANO I
  0.16 s, FLUTES 0.47 s; BRASS opened with an 8 dB spike. A square law between
  the same ends gives PIZZICATO 0.09 s, SNARE DRUM 0.47 s, PIANO I 3.25 s,
  FLUTES 4.99 s and CHORUS BELL2 10.2 s, and BRASS settles over about a second
  (the first three are tested). The law is fitted to the patches' names, not
  measured on an instrument. The LFO rate now spans Roland's published
  0.1–30 Hz (it was 0.1–20 Hz).
- A star row tagged for the former starter tones (J6F 1) shows no star and is
  cleared at the first star (now J6F 2).
- `web/test_synth.mjs` imports the asset with the editor's own reader: 128
  tones, PATCH A11 .. PATCH B88.

`tests/run_tests.sh` ended **ALL HOST TESTS PASSED**. `tests/run_base_tests.sh`
stopped at `tests/base_port_test.py`, whose ingress fixture predates MIDI thru
(`c4cf703` changed `platform/midi_ingress.h`; the fixture lacks `sd_midi_thru`
and `settings_thru`). Untouched here; every later step of the script, run
separately, passed (**ALL BASE PORT TESTS PASSED**). Logs:
`build/j106-factory-*.log`.

| Build | Package | App image | Low RAM | Resident audio | Engine arena | SHA-256 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `./build.sh` (BLE) | `platform/build/saadet.fwsc` | 554,132 B | 93,468 B | 83,856 B | 121,184 B | `976a61b9…b415` |
| `./build.sh --no-ble` | `platform/build/saadet-usb.fwsc` | 417,928 B | 45,736 B | 83,376 B | 146,240 B | `cedef5a6…4607` |
| `./build.sh --legacy` | `build/saadet.fwsc` | 287,412 B | 37,272 B | — | — | `b4f2aef8…568f` |

Unmeasured: the factory patches against a Juno-106, the VCA LEVEL span, what
the cassette's SW1 bit 7 means, and playing them on the device.

## JUNO-106 engine and the BLE default build

Source: `ui-reset` with the uncommitted JUNO-106 engine (MPL-2.0, see
`LICENSING.md`) and `./build.sh` now building the fm1-base BLE package. No
device was flashed and no repository commit or publication was made.

Host checks (`tests/j106_test.c`, in the Q15 fixtures, the shipping Q23 build
and under address/undefined-behaviour sanitizers):

- Patch messages: 500 random frames round-trip every byte; short, foreign,
  out-of-channel and 8-bit frames are refused atomically; `41 30 .. pp` is named
  by its A11–B88 label and `41 31` MANUAL. The switch bits follow the published
  layout (SW1 82 = chorus I, saw, no pulse, 8'; inverted HPF bits), and every
  switch field edits only its own bits.
- Voice: A4 at 8' measures 440.0 Hz, 16' 220.0 Hz, 4' 880.0 Hz, the sub alone
  220.0 Hz. Attack 0 completes in 2.2 ms and 127 in 3.01 s; release to 1 %
  takes 2.2 ms at 0 and 12.05 s at 127. GATE closes within a few blocks. Full
  resonance self-oscillates with the mixer shut; resonance 80 stays silent.
  Every starter tone sounds, stays bounded and ends.
- Part: chorus I and II produce a decorrelated stereo pair through the mixer;
  off is mono. For a low note the HPF orders 0 > 1 > 2 > 3.
- Library, state and editor: store/load/import in shared slots, 8-bit records
  refused, favourites in the tagged row, the last state with the tone, and a
  pre-JUNO-106 state and backup upgrading with the INIT tone. Command 96 GET,
  PUT, field, range refusal and name; INFO tag `4A 01`; favourites namespaces.
  The sanitizer run found and led to the fix of a one-byte over-read in the GET
  reply's name.
- Existing suites with JUNO-106 included where they iterate every engine
  (`synth_test`, `ui_test` layout lint of every page, `audio_precision_test`);
  `web/test_synth.mjs` covers the web tone format, INFO and library kind 4.
- The base simulator steps PROPHET, FM6, CZ-1 and JUNO-106 with ALGORITHM;
  JUNO-106 peaks at -13.4 dBFS (the others -11.4 to -15.0).

`tests/run_tests.sh` (with the new sanitizer step) ended **ALL HOST TESTS
PASSED**; `tests/run_base_tests.sh` ended **ALL BASE PORT TESTS PASSED**. Every
target build passed its layout checks; the audio RAM graph includes the
JUNO-106 voice, block, post and stereo stages.

| Build | Package | App image | Low RAM | Resident audio | Engine arena | SHA-256 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `./build.sh` (BLE) | `platform/build/saadet.fwsc` | 550,580 B | 93,452 B | 84,144 B | 120,896 B | `21fe1a3f…629b` |
| `./build.sh --no-ble` | `platform/build/saadet-usb.fwsc` | 414,392 B | 45,720 B | 83,664 B | 145,952 B | `31fb963c…71f6` |
| `./build.sh --legacy` | `build/saadet.fwsc` | 284,140 B | 37,272 B | — | — | `4ab985b1…0bb3` |

Installed on the user's request: `tools/flash.sh --no-build --port FM-1` wrote
`platform/build/saadet.fwsc` (SHA-256 `21fe1a3f…629b`, commit `dfa0322`) over the
running `FM-1_900` with the base installer: 100 % written, restarted, `FM-1_900`
returned. Its editor INFO then reported `SAADET v0.1.0-base-dev`, engines
PROPHET, FM6, CZ-1 and JUNO-106, and the `4A 01 15 16 60 1A` tag. Log:
`build/j106-flash.log`. Playing the engine on the device was not checked.

Unmeasured: the engine's sound against a Juno-106, its CPU cost on the FM-1
(the 2-unit voice charge and the batch cost reuse estimates), chorus/HPF
calibration, and SysEx from a real instrument. TRS MIDI input drops SysEx, so
patch and control messages reach Saadet over USB only.

## Direct BLE MIDI devices and forgetting

The default BLE target now enables the base's simultaneous central/peripheral
roles: one external MIDI peripheral, plus one phone/computer connected to
Saadet. GLOBAL > DEV uses Charles Vestal's unchanged btdev helper to scan,
select and connect/disconnect. GLOBAL > BLE adds phone-only DROP and confirmed
FORGET ALL PAIRINGS through the SDK's shared bond-table clear API. The radio
address/calibration store is not erased. See UI.md for controls and
BLE_MIGRATION.md for source attribution and implementation boundaries.

Validation on 23 October 2026:

- BLE target: app 533,364 bytes, low RAM 93,404 bytes, resident audio 75,788
  bytes, POOL 214,752 bytes, engine arena 129,312 bytes. CPU1, resident-code,
  packed image, engine-arena and boot-layout checks pass.
- BLE package `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
  `5ad1ee6589f49c2a0491bf9316a114ee2011aba4fb49b4be0d852c2626ee5c06`.
- USB-only target also passes: app 397,212 bytes, low RAM 45,672 bytes,
  resident audio 75,328 bytes, POOL 189,696 bytes. The BLE target is built last
  so the shared sidecars describe the BLE package.
- `tests/run_base_tests.sh`: ALL BASE PORT TESTS PASSED. New scenarios exercise
  page-driven discovery, SAVE/K4 connection actions, outgoing MIDI with only
  the external device connected, simultaneous phone and external holds,
  independent disconnects including external sustain, shared BLE clocks,
  phone-only DROP and confirmed/cancelled/expired forget requests.
- Sanitized helper tests execute the real MIDI parser with separate source
  tags, the SDK bond-clear guard in both role configurations, and the actual
  target adapter's wait-for-links, success, SDK-failure and timeout paths.
  They also check RETRY preserves a pending client search's advertising pause.
- The new UI states and scrolling list pass screen layout checks. The existing
  legacy UI and erase regressions report UI test passed and UI ERASE TESTS PASSED.

Logs: `build/base-ble-central-{host-build,build,usb-build,tests,unit-tests,devices-test,ui-regression,erase-regression}.log`.
Screens are in `build/base-ui-ble/` and `build/base-ble-devices/`.
The source update was committed as `472b60c` on `ui-reset`, then the exact
BLE package above was installed on USB MIDI port `FM-1` without rebuilding.
The installer completed and reported `done: the FM-1 runs FM-1_900`; an
independent request confirmed `FM-1_900 [running]`. The new boot's log reports
`le_config:2 1 1 1`, advertising as Saadet, heap peak 11,280 of 24,576 bytes,
and no recorded crash. Remote panel commands opened GLOBAL > DEV and waited
before another log read; no crash was reported. These diagnostics confirm
startup, not real peer interoperability. No physical forget operation was
invoked and no saved-state readback was performed.

Receipts: `build/base-ble-central-flash.log`,
`build/base-ble-central-after-device-info.log`,
`build/base-ble-central-after-device.log`, and
`build/base-ble-central-scan-device.log`.
Real peer connection, simultaneous RF/MIDI operation and persistent bond
removal across a reboot remain pending hardware validation.

### iOS peripheral handshake correction (installed, 23 October 2026)

The user connected from GLOBAL > DEV to midimittr advertising as `iOS`.
Saadet displayed CONNECTED while midimittr's Advertise status remained
`Now discoverable as iOS`. A live USB diagnostic read showed repeated
`central: MIDI ready` and disconnect events, without a recorded crash.
These observations do not identify the exact ATT/security failure.

Source inspection found that the client set READY immediately after queuing
a notification request, ignored the request's result, assumed the CCC handle
was the MIDI value handle plus one, and omitted the initial MIDI read. The
checked overlay now uses the discovered CCC descriptor and SDK per-operation
GATT callbacks: read MIDI, request notifications, then set READY only after a
successful subscription acknowledgement. Explicit ATT authentication/key-size/
encryption errors permit one Just Works pairing attempt, a 30-second wait for
the peer's user, and another handshake after successful encryption. Other
errors, cancellation and timeouts cannot mark the link ready. Profile search
and GATT setup remain on separate main-loop turns. Upstream notices remain
in the dependency; the new independent handshake helper carries Kerem's credit.

The host and BLE target builds pass. Target checks pass for CPU1, bootstrap
PC guards, SRAM code, packed image, arena and boot layout. The package is
`platform/build/saadet.fwsc`, SHA-256
`e572e548e2def23f57e64c38021beb4ff9d89c7968c494a5cdc38948b0f24ba1`.
App: 556,688 B; low RAM: 93,516 B; resident audio: 83,856 B;
POOL: 222,880 B; engine arena: 121,184 B.

`tests/run_base_tests.sh` reports ALL BASE PORT TESTS PASSED, including new
sanitized target-helper tests for delayed/failed read and subscription
acknowledgements, a nonadjacent CCC descriptor, peer-specific security errors
at either handshake step, encryption failure, pairing refusal, bounded
retries, cancellation, stale/malformed events and setup/pairing timeouts.
Logs: `build/base-ble-ios-{host-build,build,tests}.log`.
The user then requested installation. The exact tested package above was
installed without rebuilding from `ui-reset` at `9db8c66` plus the uncommitted
handshake correction. The installer completed with `done: the FM-1 runs
FM-1_900`; a separate identity request confirmed `FM-1_900 [running]` on
USB-MIDI port `FM-1`. The new startup reports advertising as Saadet, heap peak
11,280 of 24,576 bytes and no recorded crash. Old connection messages precede
the new `BT: starting` marker and do not validate the updated client.

Receipts: `build/base-ble-ios-flash.log`,
`build/base-ble-ios-after-device-info.log`, and
`build/base-ble-ios-after-device.log`. iOS pairing, MIDI port publication,
actual notes in both directions and compatibility with non-pairing hardware
still require physical retesting with this installed build.

### Encryption event handoff follow-up (23 October 2026)

With the first correction installed, the user saw an iPhone pairing request
and accepted it, but Saadet stayed on FINDING MIDI and timed out. The captured
`build/base-ble-ios-pair-failure-device.log` reports encryption status 0 and
subsequent setup timeouts, without the expected MIDI restart or a crash.

Inspection of the linked SDK's `le_hci_emit_event` and its callers in the
target disassembly established a callback convention mismatch: the SDK
reports HCI payload length plus one, so a complete six-byte encryption event
arrives with size 5. The first helper required size 6 and discarded it. GATT
operation callbacks use the complete event length. The overlay now normalizes
this SDK convention at the client's encryption event boundary; the helper
retains its bounds check. The client also tracks encryption when it precedes
an outstanding ATT authentication-error callback, allowing one MIDI retry
without starting a redundant pairing request on an already encrypted link.
Explicit pairing and security retries remain bounded. New phase/queue logs
distinguish discovery, read, subscription and encryption stalls.

The regression fixture now executes the shipping encryption event branch and
both-role router with the SDK's size 5, standard size 6, truncated events,
and encryption arriving before or after the ATT error/main-loop pairing
request. The earlier fixture called the helper directly with size 6 and did
not cover the SDK boundary. Sanitized helper checks, host/target builds and
`tests/run_base_tests.sh` pass (ALL BASE PORT TESTS PASSED).

Installed BLE package: `platform/build/saadet.fwsc`, SHA-256
`bbb8219e7cdde129d8f5ddb47e2a5fb14061bd0a8b8025b8f11f3125674addbe`.
App 557,024 B; low RAM 93,532 B; resident audio 83,856 B; POOL 222,880 B;
engine arena 121,184 B. Target CPU1, bootstrap, SRAM, packed-image, arena and
boot-layout checks pass. Logs: `build/base-ble-ios-encryption-{host-build,unit-tests,build,tests}.log`.
Installation and a separate identity request confirmed `FM-1_900 [running]`;
startup advertised as Saadet with no recorded crash. Receipts:
`build/base-ble-ios-encryption-{flash,after-device-info,after-device}.log`.
The user then reported a brief screen flash and a return to the unconnected
list. `build/base-ble-ios-encryption-retry-device.log` showed encryption
succeeding followed by MIDI setup rejection. The base folds consecutive log
lines sharing their first five characters and truncates folded lines at 26
characters, so the `central:` prefixes hid the error code.

A diagnostic-only logging revision used distinct short prefixes with error
codes first. Its target and sanitized client checks passed and it was
installed as package SHA-256
`399010f3684cff0396737f5bbd94fc697463f9ec3b14503fcea8b18416f11fab`.
Receipts: `build/base-ble-ios-diagnostic-{build,unit-tests,flash,after-device-info,after-device}.log`.
The user's subsequent failed retry is captured in
`build/base-ble-ios-diagnostic-retry-device.log`: MIDI value handle `0x40`,
CCC `0x42`, initial ATT error 5, successful pairing/encryption, another read
returning ATT error 5, then `pairing 10` (SDK bond-key storage complete).
This motivated testing whether waiting for bond completion would help:
the helper disconnected before the later bond-complete event could be used.
The following physical retest shows that waiting alone was insufficient.

### Bond completion settling (23 October 2026)

An authentication rejection on an encrypted link now waits for the SDK's
`SM_EVENT_PAIR_SUB_ADD_LIST_SUCCESS` (0x10), then retries MIDI. A 200 ms retry
also supports peers that do not store a bond or report that event. At most
five such retries occur per link, and none issues another pairing request.
The original per-operation timeout and subscription acknowledgement guard
remain. A late bond event cannot restart cancelled setup or another link.

The sanitized regression executes the actual shipping SM callback, HCI
encryption branch and both-role router. It reproduces the observed
encryption → ATT 5 → bond-complete order and verifies successful completion;
it also covers timer fallback without a bond event, persistent rejection at
the retry limit, foreign-handle bond events and cancellation. Host and target
builds pass; `tests/run_base_tests.sh` reports ALL BASE PORT TESTS PASSED.
Logs: `build/base-ble-ios-bond-{host-build,unit-tests,build,tests}.log`.

Package `platform/build/saadet.fwsc`, SHA-256
`975c3629cb7578db79e3c4cc1e59c5ea3e50251c5e20f9bb978cfcf632679d20`.
App 557,088 B; low RAM 93,532 B; resident audio 83,856 B; POOL 222,880 B;
engine arena 121,184 B. Target layout/CPU1 checks pass. The exact tested
package was installed without rebuilding on `ui-reset` at `9db8c66` plus
the local BLE corrections. The installer completed; a separate request
confirmed `FM-1_900 [running]` on `FM-1`. The new boot advertises as Saadet
and reports no recorded crash. Receipts:
`build/base-ble-ios-bond-{flash,after-device-info,after-device}.log`.
The user reported another failed physical iPhone retest. The captured log
`build/base-ble-ios-bond-retry-device.log` shows encryption enabled, five
200 ms retries returning ATT 0x05, then failure. A later attempt timed out
in discovery. Bond settling alone did not fix iOS connectivity.

### Initial-read compatibility fallback (23 October 2026)

The iPhone reports its pairing peer as Saadet and runs current iOS (exact
version not supplied). The persistent failure occurs on the initial MIDI
characteristic read, before any CCC write. Android's BluetoothMidiDevice
proceeds from its initial read callback to acknowledged CCC writing regardless
of read status:
https://chromium.googlesource.com/android_tools/+/HEAD/sdk/sources/android-25/com/android/bluetoothmidiservice/BluetoothMidiDevice.java

Saadet now tries the discovered CCC after bounded encrypted read security
rejections, or read-not-permitted/request-not-supported errors. It still
requires a successful CCC acknowledgement before READY. A rejected CCC fails
closed; subscription security errors retain the existing pairing/settling
path. Tests replay persistent ATT 0x05 on the read followed by accepted and
rejected CCC writes, all three security errors, unsupported reads, and a CCC
that first requires pairing. These are controlled protocol fixtures, not
physical evidence that the iPhone accepts the fallback.

A temporary Secure Connections flag experiment was built but never installed
and has been removed. Inspection of the pinned SDK's `btstack.a` `sm.c.o`
(bitcode decoded by its own toolchain) found `sm_setup_tk` always clearing
`sm_use_secure_connections`; the compiled setup uses the legacy association
table and has no SC public-key/DH-key implementation. Merely enabling the
runtime configuration flag is insufficient. The SDK checkout was unchanged.

Correction (5 November 2026): only the experiment's source was removed. Its
`config_le_sm_sub_sc_enable = 1` stayed in the ignored
`platform/build/ble/sdk` copy, which fm1-base refreshes only while `.copied`
is absent, so later builds from that checkout used it. They include the
read-fallback and displayed-passkey installs below and the published Saadet
0.1.0 (`1fb8e6f59e682d02…`). Saadet 0.1.1 came from a fresh worktree and uses
the SDK's `0`. Same-commit builds with each value differ only in the boot line
`fail select SC!!!,need config_le_sm_sub_sc_enable = 1`, printed at `0`, and
in code addresses; no other code in the BT libraries reads the flag. That line
is expected, not a fault. Displayed-passkey pairing has not yet been recorded
on hardware at `0`. `tools/build_base.py` now recopies the SDK files on every
target build.

Host and target builds pass. The sanitizer protocol fixture and full base
suite pass (`ALL BASE PORT TESTS PASSED`). Logs:
`build/base-ble-ios-read-fallback-{host-build,unit-tests,build,tests}.log`.
Package `platform/build/saadet.fwsc`, SHA-256
`67d2d0fca98cff89e7380e071ae68985c172f996be3e1174a1d0568acbe2ec8a`.
App 557,072 B; low RAM 93,532 B; resident audio 83,856 B; POOL 222,880 B;
engine arena 121,184 B. Target layout/CPU1 checks pass. The exact tested
package was installed on `ui-reset`, `9db8c66` plus local BLE corrections;
the installer completed and a separate query confirmed `FM-1_900 [running]`
on `FM-1`. Startup advertises Saadet and reports no recorded crash. Receipts:
`build/base-ble-ios-read-fallback-{flash,after-device-info,after-device}.log`.
The user reported another failure. Captured hardware log
`build/base-ble-ios-read-fallback-retry-device.log` confirms that the fallback
was executed (`MCRSK read 5, try CCC`, `MCSBQ handle 42`) and the iPhone also
rejected that CCC with ATT 0x05. Other attempts hit stale-bond encryption
status 6 or discovery timeouts. This rules out the initial read alone as the
cause; no physical subscription acknowledgement or MIDI traffic succeeded.


### Displayed passkey pairing (23 October 2026)

The previous helper advertised no input/output capabilities and ignored
`SM_EVENT_PASSKEY_DISPLAY_NUMBER`. Saadet has a display: the outgoing/master
role now advertises DISPLAY_ONLY and requests BONDING | MITM_PROTECTION when
pairing is required. It does not proactively force pairing on MIDI devices
that permit unpaired access; the incoming/peripheral role keeps its existing
Just Works policy. This is authenticated legacy pairing, not LE Secure
Connections support. Whether iOS accepts this association method for its MIDI
service remains a hardware question.

GLO → DEV shows the SDK-generated six-digit code (including leading zeros)
and asks the user to enter it on the other device. No fixed PIN is used and
the PIN is not logged. Events from another connection and malformed/out-of-
range codes are ignored. Encryption completion, display cancellation, setup
failure, user disconnect and timeout clear the code. Displaying a code allows
30 seconds for user input; no CONNECTED status appears without CCC success.
An ATT rejection while the user is entering a code waits for encryption
without issuing a duplicate pairing request. Setup timeout now resets the
helper immediately so late events cannot revive its pending operation.

The sanitizer regression executes the shipping SM callback for generated
codes, leading zeros, valid zero/max code, malformed/foreign events,
cancellation, completion, timeout, and ATT during passkey entry. UI layout
checks and rendered inspection cover the code and instructions without
overlap; images are in `build/base-ui-ble/ble_device_pairing*.png`.

Host and target builds and the full base suite pass (`ALL BASE PORT TESTS
PASSED`). The focused protocol regression was rerun after the final invalid-
code handling adjustment; it passes under ASan/UBSan. Logs:
`build/base-ble-ios-passkey-{host-build,build,unit-tests,ui-tests,tests}.log`.
Package `platform/build/saadet.fwsc`, SHA-256
`eb0f1731c28e1b70769966b3750299d3e454cd895555110507dd3d670335a215`.
App 557,792 B; low RAM 93,564 B; resident audio 83,856 B; POOL 222,880 B;
engine arena 121,184 B. Target layout/CPU1 checks pass. The exact package
was installed on `ui-reset` at `9db8c66` plus these local corrections. The
installer completed, a separate query verified `FM-1_900 [running]` on `FM-1`,
and startup advertises Saadet with no recorded crash. Receipts:
`build/base-ble-ios-passkey-{flash,after-device-info,after-device}.log`.
The user confirmed a fresh pairing displayed a code and then connected.
The hardware trace `build/base-ble-ios-passkey-retry-device.log` independently
confirms the complete path: MIDI value 0x40 / CCC 0x42, initial ATT 0x05,
`MCPIN enter code on peer`, successful encryption, successful read and CCC
write, SDK bond-complete 0x10, then `MCOK MIDI subscription confirmed`.
This establishes actual iPhone MIDI subscription setup using displayed-code
authentication. The read fallback was not needed in this successful attempt.
Actual MIDI notes/audio, sustained connection, disconnect/reconnect and
bond persistence across reboot have not been physically validated.

## Computer/phone BLE connection repair

Reviewed Charles Vestal's fm1-ctl at `e181b4a1ac1510ce18ebf6e271a143c2fe364ccd`
and the shared base's connection/pairing history. The live unit's pre-change
log (`build/base-ble-connect-before-device.log`) contains repeated
`post: no task sys_timer`, with advertising reported and no recorded crash.
The patch repairs named default-service timer dispatch in a BLE-only build,
adds GLOBAL > BLE connection state and RADIO/RETRY controls, corrects GAP name
reads, resets negotiation/subscription per link, and respects OFF during
asynchronous startup. See `BLE_MIGRATION.md` for provenance and boundaries.

Software validation:

- The BLE target passed CPU1, resident-code/packed-image, engine-arena and
  boot-layout checks. The app is 522,148 bytes, low RAM 91,792 bytes, resident
  audio 75,684 bytes and POOL use 214,624 bytes.
- `tests/run_base_tests.sh`: **ALL BASE PORT TESTS PASSED**. This includes the
  actual patched timer source (BLE and WiFi ownership behavior), startup-ready
  handler, ATT name reads, fresh-link state, disconnect/re-advertising, UI
  layout and controls, and host connection/reconnection with BLE-note release.
  Existing native USB, storage, MIDI overlap and bootloader/updater checks pass.
- The separate legacy UI regression reports **UI test passed**.
- The USB-only comparison target also passes; app 394,468 bytes, low RAM
  45,368 bytes, resident audio 75,336 bytes and POOL use 189,696 bytes.
- The BLE package is `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
  `7af16e7bdbef2bf761cbdab937aa7bcc866fee84a67af56c0595884d4ef8a828`.

Logs: `build/base-ble-connect-{host-build,build,tests,unit-tests,ui-regression,usb-build}.log`.
Screen fixtures are in `build/base-ui-ble/` and the platform host screenshots
in `build/base-ble-connect/`. The target build runs last so its shared sidecar
files describe the BLE package rather than the USB comparison.

All outstanding source changes were committed as `dd9d0f8` on `ui-reset`,
then the exact BLE package above was installed through the base's normal app
installer on USB MIDI port `FM-1`. The installer completed its write and
reported `done: the FM-1 runs FM-1_900`; a separate identity request confirmed
`FM-1_900 [running]`. The package was reused without rebuilding.

Post-install diagnostics report no recorded crash and `BT ready: advertising
as Saadet`. The retained log also contains earlier boot history; after the
latest `BT: starting` marker, no `post: no task sys_timer` warning appears,
including the subsequent delayed log read. This establishes startup and
advertising diagnostics, not a physical BLE connection. Receipts:
`build/base-ble-connect-flash.log`,
`build/base-ble-connect-after-device-info.log`, and
`build/base-ble-connect-after-device.log`.

A Mac/iPhone/iPad connection, host pairing/bond persistence, actual BLE MIDI
traffic, timing and physical audio remain unverified. No saved-state readback
was performed. The causal role of the missing callback in the user's failed
physical connection is not yet established. Installation receipts in the next
sections refer to earlier firmware.

## m-bootloader integration

The platform now pins fm1-base `6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431`
and felucca-bsp `0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f`.
The new BLE package is 611,354 bytes, SHA-256
`49bb09219c1a92aafe871442b5f876fb09d01d435df5473ae3fa8b86cc90a3d2`.
Its app is 520,468 bytes, low RAM 91,792 bytes, resident audio 75,684 bytes
and engine arena 129,440 bytes. Target boot-order, packed-code, CPU1 and RAM
checks passed. After the user's explicit update request, this new app was
installed over USB MIDI. The updater completed and verified `FM-1_900`, and
native editor INFO returned `SAADET v0.1.0-base-dev`.

Builds prepare the unmodified m-bootloader 1.2 FM-1 artifacts, their MIT notice
and a source/checksum manifest. Their hashes match the author's v1.2 release:

- Installer `.fwsc`: `0e89141f52d1f2d62198d4f69147a2c15c8360806e64d48002990a6fdab6d6dd`.
- Plain `.boot`: `9e5ec0e39dc55c167f8e289cfdac612d7d340b5f46100e1be2540ef4d6ec2f13`.

`ALL BASE PORT TESTS PASSED` includes the updated host simulator, native USB
sanitizer fixtures, eight bootloader-wrapper tests, the real BSP OTA staging
tests under ASan, upstream MIDI app-installer tests and the upstream browser
bootloader-installer simulation. Coverage includes refusal by an older app,
disconnects, out-of-range reads, a missing/wrong app after restart, altered
bootloader files, corrupt writer bodies and wrong-board/damaged boot areas.

A separate offline check compiled m-bootloader v1.2's actual `core.c` under
ASan/UBSan against the new Saadet app combined with this unit's saved boot
head. Its boot replacement preserves the app, mounts the app at `0x4000`
with entry `0x02000120`, accepts all file CRCs and rejects an intentionally
corrupted app. The synthetic boot head inside Saadet's app-only `.fwsc` does
not have the stock allocation required for boot replacement; it must not be
raw-flashed. The normal app updater preserves the unit's existing boot head.

Logs: `build/base-mboot-build.log`, `build/base-mboot-host-build.log`,
`build/base-mboot-port-tests.log`, `build/base-mboot-app-check.log`.
The pinned m-bootloader installer was then staged with
`tools/base_bootloader.py --install --port FM-1 --yes`. Staging completed and
Saadet returned as `FM-1_900`. Receipts are
`build/base-mboot-device-update.log`, `build/base-mboot-device-app-info.log`
and `build/base-mboot-device-bootloader.log`. The user confirmed the PLAY
recovery menu appeared at power-on, establishing that m-bootloader is active.
REC at power-on and the menu's DOWNLOAD MODE both led to a black screen.
The direct USB tool briefly found the expected `4c4a:8057` ROM device but
failed while detaching its kernel driver (`USBError: Access denied`);
subsequent enumeration found no ROM device. The user reported that Saadet
returned without a requested power-cycle. No flash, RAM-loader upload or
reboot command was sent during these download checks. The reason for the
return is not established. Trace: `build/base-mboot-menu-download.log`.

Final checks again reported `FM-1_900 [running]`, native editor INFO
`SAADET v0.1.0-base-dev`, no recorded crash and BLE advertising startup
(`build/base-mboot-final-device.log`). No boot-area or saved-state readback
was performed during this installation. The user deferred further terminal
download-mode checks until later; end-to-end USB recovery on this Mac remains
unverified. See `BUILDING.md` for the app-first installation order and explicit
bootloader command.

## First device flash and startup repair

The user authorized installation after the host/target checks. The base
installer wrote the BLE package with SHA-256
`c2cdbbd37fe1ab7f07c8841d7d7e4a89061041c39a4953ac09bcdb869a71e45d`
through 100%, then timed out waiting for MIDI to return. The device was absent
from both MIDI and ROM USB at the subsequent checks. This is a failed startup,
not a verified installation. The receipt is `build/base-flash.log`.

Review identified an early-boot dependency: resident P33 helpers and their
far-call pointers could run before SRAM/data initialization. The corrected
base startup stops CPU1 first, clears buffers, copies initialized data and
both RAM code sections, then enters hardware/time/watchdog setup. The actual
target disassembly was inspected to confirm those loops precede the first
P33 call; `tools/check_base.py` now enforces this boot-order contract.

The corrected BLE package is `520,340` app bytes, SHA-256
`d90034c3217393cf3cde6f78b8a1f32fa19e8205a485bc6955cf5033a25d5ec7`. Its target checks, offline recovery-package / chip-key /
loader checks, and USB/MIDI/storage sanitizer fixtures pass.

The FM-1 was recovered through a Seeed XIAO RP2040 running the verified
Transporter firmware. Recovery reported `key=980F type=3 id=856014`.
Two full flash reads were identical (SHA-256
`ed5130c30520587b98fecc5b36c67944a6f3d951314fc8e438290cc9e580adca`).
After a successful dry run, 128 application sectors were written from the
corrected BLE package. The final full 1 MiB read exactly matched the expected
image (SHA-256
`5054e3353d2769dbaae02fb651aad0948fbfdb39e6e417cb232c7fc798de3abc`),
including unchanged bootloader `0x0000..0x3FFF` and device-data
`0x93000..0xFFFFF` regions. Receipts are `build/transporter-dump.log`,
`build/transporter-restore-check.log` and `build/transporter-restore.log`.
After removal of the recovery wiring and a normal power-cycle, the user
confirmed that Saadet booted. USB MIDI returned as `FM-1`; the base updater
reported `FM-1_900 [running]`, and native editor INFO returned
`SAADET v0.1.0-base-dev`. The diagnostics reported no recorded crash, MIDI BLE
profile initialization, and `BT ready: advertising as FM-1`, with radio heap
peak 6,472 of 24,576 bytes. These are device-reported startup observations,
not an over-the-air pairing or musical test. The startup log also contains
`post: no task sys_timer` messages and nonzero timing-late counters; sustained
audio/deadline behavior is not yet qualified. Receipts are
`build/base-post-recovery.log` and `build/base-post-recovery-diagnostics.log`.

The original pre-installation measurements below describe the earlier
package; they are retained as the record of the failed artifact's validation.

## Experimental fm1-base / BLE port

Source: `ui-reset` with the existing local UI/audio work, completed engine
preparation and the uncommitted `platform/` port. No device was flashed and no
repository commit or publication was made.

Both target variants passed upstream build checks and Saadet's additional
CPU1, SRAM-copy, packed-code, engine-arena and boot-entry checks. Package
identity is `FM-1_900`; each package is 610,136 bytes.

| Target | App image | Low RAM used / 98,304 B | Resident audio | Engine arena |
| --- | ---: | ---: | ---: | ---: |
| BLE MIDI + USB | 520,356 B | 92,304 B | 75,796 B | 129,312 B |
| USB-only comparison | 392,872 B | 45,880 B | 75,336 B | 154,368 B |

The BLE target leaves 6,000 bytes of ordinary low RAM.
Its linked radio buffers/tasks are already included in those figures; physical
radio startup and runtime heap/deadline behavior still need measurement.
CPU1 has private 8 KiB stacks; its polling loop and native synth graph are
copied into SRAM before worker launch. The guaranteed engine arena exceeds the
16 KiB engine-state requirement. Optional cache RAM is disabled in this port.

Packages:

- `platform/build/saadet.fwsc` (BLE), SHA-256
  `c2cdbbd37fe1ab7f07c8841d7d7e4a89061041c39a4953ac09bcdb869a71e45d`.
- `platform/build/saadet-usb.fwsc`, SHA-256
  `7337c23c1e37aff22c4d10bc471be16124f454ec31a70d1b5bf74d7d6f41548e`.

### Completed checks

- All 43 named legacy host checks passed, ending with
  `ALL HOST TESTS PASSED`. These include native-Q23 integration, concurrent
  engine worker parity, UI/persistence/editor/recovery and sanitizer checks.
- `tests/run_base_tests.sh` passed, ending with `ALL BASE PORT TESTS PASSED`.
  The base simulator produces audio from Prophet, FM6 and CZ-1; it verifies
  BLE note/sustain disconnects, overlapping USB/TRS holds, keyboard output to
  both USB and BLE, and BLE clock transport while ignoring USB transport.
- The actual patched USB routines passed native PCM24 edge/low-bit packet
  round trips, PCM16/dither bounds and silence, both format descriptors,
  playback decode/mix and standard mute/volume controls under sanitizers.
- Input overflow clears pre-panic port ownership; the next note-on/off pair
  remains releasable. Non-SysEx status reaches the native import-abort hook;
  real-time clock does not abort the frame.
- All 48 live legacy flash sectors were enumerated, checked for unique
  addresses and verified separate from the base's panel/pairing objects and
  OTA area. This verifies the address plan, not saved data on a physical unit.
- The simulator's engine-library screenshot was inspected for display output.

Logs: `build/base-ble-build.log`, `build/base-usb-build.log`,
`build/base-host-build.log`, `build/base-port-tests.log`,
`build/base-legacy-regressions.log`. Target footprint/hash records are
`platform/build/saadet-validation.json` and
`platform/build/saadet-usb-validation.json`.

### Still unmeasured or incomplete

Actual BLE advertising/pairing, disconnect behavior, physical controls/audio,
USB 16/24-bit host interoperability and volume, sustained render deadlines,
radio calibration, native import/backup round trips, and migration update /
recovery have not been tested on the FM-1. The base simulator runs the
instrument serially; the separate legacy tests exercise concurrent engine
workers, while the new target's actual dual-core lifecycle remains unmeasured.

The port currently omits the legacy `ED_AUDIO_STATS` capability. BLE central
and BLE SysEx/editor transport are not implemented. The linked-source and
preset-asset distribution audit remains open, including retained
ChanceTheMaker MIDI contributions; see `docs/BLE_MIGRATION.md` and
`LICENSING.md`. These results do not establish release readiness.

---

# Historical Q23 restoration — 22 October 2026

Source: `9f6e8ad` plus the restored native 24-bit audio path and existing local
interface work. The precision rollback `e51e199` is reversed selectively;
newer committed and in-progress interface changes are retained.
The original Felucca checkout was not modified.

## Audio and effects

The default signal path is Q23: signed 24-bit audio in 32-bit containers, with
wide gain/filter products and DC histories. Prophet, FM6 and CZ-1 retain their
fp32 cores and quantize their output at higher precision. SID retains its
chip-scale model and gains the higher-precision amplitude and output paths.
The master saturation curve uses fp32 knots and cubic interpolation.

USB PCM24 preserves the low byte in both directions. PCM16 capture applies
TPDF dither at final reduction; exact silence stays silent. USB format
selection does not change synthesis precision or the 44.1 kHz rate.
Delay histories use packed PCM24 with unchanged lengths. Echoverb's diffusion
state and history use separate fixed arena handles. Precision helpers remain
always inlined, limiter release retains the original Q15 time scale and ROOM
runs in POOL SRAM to preserve the low DSP arena tail.

One serial chain still contains three inserts. Punch DSP and send buses remain
removed. Reserved send parameter numbers remain inert for state compatibility.
Engine scheduling, voice admission, worker fallback and sound persistence are
unchanged by the precision restoration.

## Interface

The newer Stage theme and existing local interface work are retained. This
audio restoration does not edit the interface sources or UI tests.

## Package

The target build completed RAM, audio placement, MMIO, boot/update and package
checks. `build/saadet.fwsc` is 610,542 bytes, identity `FM-1_900`;
the app is 267,872 bytes and loader 7,386 bytes.
Package SHA-256: `ee325dd83dfc17b5cee4fe00b8dd2547f13603d4c51021ac94e31255db60c092`.

The main audio arena has 184,128 bytes against its enforced 172,032-byte
minimum. Low, data and retained SRAM tails have 8,948, 55,784 and 15,440 bytes
respectively. Guaranteed SRAM banks total 264,300 bytes; the optional cache bank
adds 28,672 bytes after its boot self-test.

## Checks

The host suite passed all checks through native-Q23 concurrent-worker parity,
including engines, effects, MIDI, persistence, USB, UI, recovery and editor
checks. Historical DSP fixtures use their original Q15 scale; separate
shipping-Q23 integration and numerical fixtures exercise the native path.

The restored precision fixture initially failed its obsolete assertion that
all held buttons leave the keys musical. The newer UI intentionally uses held
buttons to select pages, as its existing UI test verifies. Only that precision
fixture was adapted: it checks musical keys on the released FX page and the
three-insert contract. Both remaining checks then passed when rerun:
`build/host/native-audio_precision_test` and `build/host/precision_asan`.
The latter enables address and undefined-behavior checks, excluding inherited
signed-shift and host alignment assumptions.

PCM24 edge/random round trips, sub-PCM16 synthesis from all four engines,
active audio effects, dither, the master curve, limiter release and concurrent
Echoverb/chorus allocation passed. Native precision reports a maximum master
curve error of 2.297 Q23 LSBs. All pre-existing local UI and flash-script edits
were verified unchanged byte for byte.

Logs are local ignored build artifacts: `build/q23-restore-build.log` and
`build/q23-restore-tests.log` (including the initial stale-test failure).
The completed reruns are in `build/host/log/native-audio_precision_test.log`
and `build/host/log/precision-address-overflow.log`. Reproduce the complete
suite with BUILDING.md.
Host and package checks do not establish physical noise floor, audible quality
or sustained on-device deadlines. Device CPU use has not been measured here.
No repository or site was published, and this package has not been installed.

## UI row F — 23 October 2026

Source: `9402c54` plus local delete/erase changes. Earlier sections describe the
22 October audio restoration; effects were subsequently removed by `42ac8f5`.

Library user sounds now expose K4 DELETE, with an expiring FX confirmation,
cancellation gestures, a warning header and a breathing FX LED. Deletion keeps
the current sound playing and removes its slot and favourite. GLOBAL ends with
ERASE: K1 arms, a fresh three-second GLO hold confirms, and release cancels
without paging. Successful erasure clears settings/calibration retention and
reboots; failures keep the error screen and do not report success or reboot.

The target build passed all RAM, audio placement, MMIO, boot/update and package
checks. The app is 294,592 bytes, `.data + .bss` is 44,920 of 98,304 bytes,
and the loader is 7,386 bytes. `build/saadet.fwsc` is 610,542 bytes,
identity `FM-1_900`, SHA-256
`3307b17e390d9a2411a855bb2775a9f20033e7bc2be304c448eb2f671f459fa6`.

`tests/run_tests.sh` finished with `ALL HOST TESTS PASSED`. The additional
`ui_erase_test` passed address and undefined-behavior checks, with the same
inherited shift/alignment exclusions used by the audio fixture. It exercises
all four engine lists, favourites, slot 384, cancelled/expired/stale delete
confirmations, failed writes, delete persistence, the erase countdown,
release suppression, unavailable flash, and reported/silent/partial erase
failures. Both A/B copies of all 24 live objects are erased and read back.
An independent expected sector map verifies every other byte of the 1 MiB
flash model remains unchanged, including firmware and staged-loader storage.
The new screens passed the layout lint and were visually checked at 240×240.

Logs: `build/ui-row-f-build.log`, `build/ui-row-f-tests.log`,
`build/host/log/ui_erase_asan.log`. Screen captures: `build/ui-row-f/`.
This package has not been installed. Physical controls, audio continuity and
device erase/reboot behavior remain unverified.

## Full-charge reference support — 23 October 2026

Before installation, the connected device identified as `SAADET v0.1.0-dev`,
`FM-1_900`, MIDI port `FM-1`, and did not answer the new battery-read command.

The prepared update adds command 81 for a fresh ADC sample and a saved full
reference. It uses tagged reserved settings bytes within the existing 604-byte
PER5 record; sounds, favourites and panel mapping retain their layouts. The
capture tool takes 31 samples and saves the median. No empty point or battery
percentage is inferred. Settings and integration fixtures verify round trips,
write idempotence, invalid values, transport blocking and failed flash writes.

The target package is `build/saadet.fwsc`, identity `FM-1_900`, 610,542 bytes;
the app is 294,880 bytes. SHA-256:
`73747ede0c2c8f20711e6b8cf93c6200d317ffae1617d1b8b6a8b047edb094ba`.
Build log: `build/battery-reference-build.log`. The complete suite finished
with `ALL HOST TESTS PASSED` (`build/battery-reference-tests.log`), including
Q23 integration and address checks.

Installed that package on 23 October 2026 using `tools/fm1_install.py`;
the installer completed and confirmed running identity `FM-1_900`. With the
user reporting a full battery, 31 fresh ADC samples ranged from 627 to 630,
median 628. Command 81 saved 628 as the full-charge reference and confirmed
it by readback. Receipt: `build/battery-full-reference.json`. The empty point
and a percentage curve remain uncalibrated. This installation and calibration
do not establish physical audio quality or performance deadlines.

## 23 October 2026: BLE migration engine preparation

This stage retains the existing GPL platform and does not enable Bluetooth.
SID/reSID and PHASE/CrispyZebra implementations and SID waveform inputs are
removed. CZ-1 has standalone state/reset handling. FM6 uses native MSFA-style
operators, accepts historical renderer IDs on import through MSFA, and uses the
common instrument's allocation/mono priority with new retrigger/legato hooks.
MARK I/OPL sound parity is intentionally not claimed. Sources, licensing limits
and remaining platform work are recorded in `BLE_MIGRATION.md`.

The target build passed the image, RAM, resident-code, dual-core, register-access,
boot/update and package checks. The app is **271,180 bytes**; `.data + .bss`
is **37,224 of 98,304 bytes**. The contiguous main resource arena is 199,264
bytes; the enforced engine-state minimum is now 16,384 bytes, replacing obsolete
effect budgets. Native tests render all three engines with that 16 KiB bank
alone and no optional cache or auxiliary SRAM tails. These figures are for the
current non-radio build; they do not establish the final BLE memory layout.

`tests/run_tests.sh` completed all 43 named checks with **ALL HOST TESTS PASSED**,
including native Q23, dual-core serial/concurrent parity, bounds and overflow
sanitizers, UI/layout, storage/update/recovery, and editor tests. New coverage
checks FM6 state migration (native patch bytes and unrelated controller settings
preserved) and retired SID state rejection. The MSFA routing fixture passes
91,392 edge cases plus 200,000 changing blocks against the historical MODERN
reference; the envelope wrapper passes 816,000 state transitions. The final
three-engine delete fixture was rerun with slot 384 on the last engine and passed.

The rebuilt package is `build/saadet.fwsc`, 610,542 bytes,
identity `FM-1_900`, SHA-256 `365af9e3363826dd8095d4681764dc93c364721b96980f1d62f9471140aee7d7`. It has not been flashed.
Physical audio, control behavior and sustained hardware deadlines remain
unverified. Build/test logs: `build/migration-build.log`,
`build/migration-tests.log`, `build/host/log/`.

## 5 November 2026: HYDRA on the SMK-37 Elite

Built on the own Bluetooth stack (`own-ble-stack`, a snapshot of its uncommitted worktree). Package
`build/saadet-smk37-elite.fwsc`, SHA-256 `05cf4f62f4ba027f1f3c4c33c00050c3551250de9644526b69441fac0a7c13de`, flashed over USB with the base's installer. It was the first time that stack
ran on a unit: it booted, USB MIDI and the installer worked, no Bluetooth peer connected. All numbers are this unit at 360 MHz
with CPU1 online, from `ED_AUDIO_STATS` (`audio_max_us`, `audio_late`), and no Bluetooth traffic. The deadline is 2902 us.

What a voice costs, per voice, from chords of 8 notes (the budget let 6 play with a mutant):

| Patch over INIT | At the sample rate (MIDI 36-60) | At twice the rate (MIDI 72-96) |
| --- | --- | --- |
| OSC 1 | 155 us | 233 us |
| OSC 1 + 2 | 180 us | 282 us |
| OSC 1 + 2 + 3 | 192 us | 308 us |
| FILTER 2 in use | 172 us | 248 us |
| Ring and noise | 193 us | 308 us |
| One PW, SQUEEZE or WARP | 231-234 us | 376-381 us |
| One FM | 239 us | 396 us |
| One STACK | 237 us | 389 us |
| PW on both oscillators | 322 us | 556 us |
| STACK on both oscillators | 333 us | 582 us |
| STACK and PW on one oscillator | 416 us | 749 us |
| Both scanning, STACK + FM and STACK + SYNC, all sources on | 1104 us | 1439 us |

An idle block takes about 330 us. Melodee's rule, three units with any mutant, gave 3078 us and 706 missed renders in 4 s on
SUPERSTACK and up to 8965 us on the last row. So `hy_cost` (`firmware/src/eng_hydra.c`) estimates a voice from these rows, and:

- A voice takes `cost × 72 / 2270 us` steps of the budget's 72 (a quarter unit each, `VQ` in `firmware/src/core.h`), so a full
  budget renders within 2600 us: eight voices of a plain patch or one mutant, six with mutants on both oscillators, five with
  STACK and PW on one. (Whole units, at least two and three with any mutant, left up to 125 us a voice unused: one mutant
  played six.)
- A voice on its own takes about 1.6 times its share: the batch gives CPU1 a part's first two voices and then every other one,
  so `n` voices take `n - (n - 1) / 2` one after another. `hy_cap_of` bounds a patch's voices by that, and a voice that cannot
  pair takes its whole time from the budget. The last row plays one voice.
- Twice the rate needs room: what the block has left once the part plays all it can at the sample rate (MONO and LEGATO: one
  voice and its fade) is shared among the voices that use it, in both parts, each one's extra at its time alone: it lands on
  one core. A plain patch has all eight, FM BELL one, SUPERSTACK none; a lead's high notes have it. Counted in shares, one
  AUTOMATA ET voice at twice the rate fitted with 224 us to spare and took 2970 us, every block late; HIT C2 ! LA's twice the
  rate adds 704 us to a voice alone where its share says 371.
- A chord's first block counts each part's voice cost once (`voice_units_block` in `seq.c`): before, 8 notes costed the patch
  with every macro at its worst dozens of times in one block and ran up to 700 us long. The twice-the-rate check asks for it
  at each high note, so `hy_cost_most` keeps each part's by the patch's hash: high chords' first blocks came down from up to
  2964 us (DUAL FILTER, one missed render) to 2756 at the most (FM BELL), none missed in 30 starts on five sounds.
- The cores' pairing measured exactly on HIT C2 ! LA, a converted patch: 814 us a voice alone, 1627 for two, 2441 for four
  (CPU1 runs three of four). FM in a STACK costs more than the table's STACK and PW: 300 and 288 us a share on two converted
  patches where the rule took 261, and HIT's twice the rate 440 where 371, so the rule adds 30 and 70.
- A sound loaded over tails keeps as many of them as it can play (`hy_tails_fit`): counted in shares, two AUTOMATA ET tails
  stayed and took 3092 us.
- Where the cores bind before the budget, the cap's voices take all of it, so a stolen voice's fade waits instead of
  sounding a voice beyond the cap: HIT C2 ! LA played three and a fourth fading, 2818 us.
- A note with nothing to take, no fade to wait for and more than the CPU carries waits (`voice_alloc`): LAYER with a voice
  bigger than a part's half on both parts started the second part's beyond the budget, every block late.

**Why:** overload cuts nothing here, so the budget is the only protection. A note beyond it takes a voice as any other does.

6 November, the steps and the rules above, package SHA-256 `1a1364dba957bb4b4334919367ec4d98e9d9ceebfe0331a07cad5648e41fa4bb`
on the Elite. Missed renders count from just before the notes start; the longest render is after they settle:

| Test | Longest render | Missed renders |
| --- | --- | --- |
| All 17 sounds: low, high, struck (51 runs) | 2508 us (FM BELL, struck) | 0 |
| 7 converted Hydrasynth patches, the heaviest at each voice count from 1 to 8: low, high, struck (21 runs) | 2665 us (WCOG, 4 voices, struck) | 2 |
| The same 7 in LAYER, on both parts: low, high (14 runs) | 2596 us | 5 |

Each missed render is one block at a chord's start, on two converted WCOG patches and in LAYER; on the first build of these
steps the same 35 converted runs missed 4698 (AUTOMATA ET at twice the rate, and LAYER beyond the budget, every block). The
LAYER pairs with other engines, USB capture and the 33 loads below were not run again. On this code the three suites pass:
`tests/run_smk37_tests.sh`, `tests/run_base_tests.sh` and `tests/run_tests.sh` (57 checks), ending ALL SMK-37 TESTS PASSED,
ALL BASE PORT TESTS PASSED and ALL HOST TESTS PASSED.

With the whole-unit rule, on its final code:

| Test | Longest render | Missed renders |
| --- | --- | --- |
| All 17 sounds: 8 notes low, 8 notes high, 8 notes with the top one struck 5 times a second at changing velocities (51 runs) | 2722 us (SUPERSTACK, high) | 0 |
| LAYER: 7 pairs of parts (HYDRA with itself, Prophet, FM6, JUNO-106, CZ-1), low, high, struck (21 runs) | under 2600 us | 0 |
| USB capture running, 5 heavy sounds, high and struck (10 runs) | 2738 us (SUPERSTACK, high) | 0 |
| 33 sound loads under 8 notes, three times | 2771 us | 0 |

A load releases what sounds, and a cheap patch's eight tails would then render at a heavy patch's cost: 2985 us and 4 missed renders
in those 33 loads. `hy_tails_fit` fades the tails beyond what the block can carry, without their mutants for those 23 ms. Held notes
are left alone, so adding heavy mutants from the panel over eight held notes can still run late until a note ends.

For comparison on the same build: Prophet 8 voices 2282 us, FM6 1678 us, CZ-1 1732 us, JUNO-106 1299 us. An earlier run of the
LAYER test counted one missed render as 16 notes started at once; the final runs did not repeat it.

Audio over USB (sox, triad C4 E4 G4, velocity 100), peak and RMS while it sounds: P5 -3.9 / -11.7 dB, CZ-1 -3.6 / -14.2, J106
-12.0 / -21.6, FM6 -14.6 / -28.8; HYDRA's sustained sounds run from -5.5 / -16.3 (PWM STRINGS, INIT) to -23.1 / -27.2 (MS-20 ACID). Eight re-presses at
velocities from 30 to 127 on four sounds show no step above the waveform's own: the largest ones repeat with the wave. Nobody
has heard it yet. Upgrade on the unit: the last state from before HYDRA (`SDS2`, 1536 B) restored through the backup path and
came back as `SDS3`, 1824 B, with its sounds and parts mode; settings and the user bank kept their CRCs.

Flash on this build: Elite 495,808 B of 622,524 (126,716 free), FM-1 483,404 B of 581,564 (98,160 free), about 58 KB for HYDRA.
Resident audio 107,956 B and the engine arena 102,816 B on the Elite; a part's state takes 5.4 KB, 9 KB or 12.5 KB by its wave copies.

Not checked: the FM-1 (built, not run), HYDRA under Bluetooth traffic, the panel's pages and knobs on the unit itself (host
renders only), and the sound by ear.

## 6 November 2026: HYDRA's user waves (host only)

Branch `hydra-user-waves` off `main` at c333332: 14 user waves (`hydra_bank.h`, `hydra_bank.c`), wave ids 57-120 reserved for
them, command 98, the backup archive's objects 39 and 40, and the web editor's USER WAVES panel. Not flashed, not run on a unit.

All three suites passed on the branch: `ALL HOST TESTS PASSED`, `ALL SMK-37 TESTS PASSED`, `ALL BASE PORT TESTS PASSED` (the
base suite after `./build.sh --host`: its `saadet_host` from 5 November predated HYDRA and counted four engines). New host
checks in `tests/hydra_test.c`: a user wave holding BELL's points renders bit-exact as BELL; a bank change re-copies only the
parts' user rows at the next block; an id past the bank is silent; command 98 LIST, GET, PUT, STORE, REVERT and CLEAR; the bank
survives a power cycle and a torn write keeps flash's copy; the at-rest autosave waits while a note sounds. The sanitizer build
found an over-read in command 98's name field on a short factory name, fixed before this run. `tests/storage_test.c` and
`tests/ui_erase_test.c` place the new sectors on both boards (64 live sectors); `web/test_synth.mjs` parses the firmware's real
LIST and GET replies and covers packing, the file message, `.wav` import, the import plan and backup versions 1-3.

Sizes: FM-1 app 492,408 B of 581,564, engine arena 95,360 B; SMK-37 packages 505,168-505,360 B of 622,524, engine arena
93,824 B. The bank takes 7,352 B of the pool (the arena was 102,816 B on the Elite before). Not measured against a clean build
of c333332: by symbol sizes the new code is about 3 KB, and the audio path gains `hy_ufresh` (204 B) and a larger `hy_want`.

Not checked: any unit, timing with user waves (the audio work per block is unchanged: a bank change copies 512 B per user row,
as a patch load does), the web panel against a unit (its drawing was checked in a harness with a simulated unit; the repaint on
a light / dark switch was not seen), and the sound by ear.
