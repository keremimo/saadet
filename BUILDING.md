# Building Saadet

Use the JieLi pi32v2 Linux toolchain and AC79 V1.2.1 SDK used by Melodee.
On macOS, Docker or OrbStack must be running for the Linux compiler.
Python requires Pillow with Raqm; the build also needs Node.js for editor tests.

```sh
python3 -m venv build/venv
build/venv/bin/pip install Pillow
PYTHON="$PWD/build/venv/bin/python" ./build.sh
npm ci --prefix web
PATH="$PWD/build/venv/bin:$PATH" tests/run_tests.sh
```

`./build.sh` builds the fm1-base port with Bluetooth MIDI (below):
`platform/build/saadet.fwsc`, development identity `FM-1_900`. `--no-ble`
builds the USB-only `platform/build/saadet-usb.fwsc` instead, `--host` the
simulator, and `--release X.Y[.Z]` a release identity.

The legacy platform is retired. Its compiler/release commands and local firmware outputs have been removed;
only fm1-base BLE, USB-only and host targets are available. Host tests need no legacy package. Target checks
cover resident audio, RAM capacity, boot/update layout and package structure; updater integration is exercised
by `tests/run_base_tests.sh`. Host test logs are under `build/host/log`.

Serve the editor without publishing:

```sh
python3 -m http.server 8846 --bind 127.0.0.1
```

Open `http://127.0.0.1:8846/web/editor.html`. Web MIDI requires a browser that
supports it and user-granted SysEx permission. The local editor contains no
multi-track simulator. `web/install/?demo` runs the web installer against a
simulated FM-1 (see "Release site and web installer" below); nothing is written.

Installation is a separate, explicit operation. `tools/flash.sh` builds the
default package and installs it with the base's installer. Recovery is `tools/fm1_recover.py`. Use the actual
Saadet package and detected FM-1 port. Read the storage note in
README before switching between Saadet and Melodee. Host and target checks are
not physical audio/control/deadline validation.

The shipping audio path is Q23 (24-bit signed samples in 32-bit buffers).
The inherited DSP fixtures run with `AUDIO_FRAC_BITS=0` to preserve their Q15
numeric contracts. The suite also runs native Q23 integration, numerical,
concurrent-worker and precision/overflow checks. `tests/audio_precision_test.c`
checks signed PCM24, low-level synthesis, dithered PCM16 export, and all four
engines in a single 16 KiB resource bank without cache or auxiliary SRAM tails.
The linker enforces that engine-state budget, with matching compile-time size
checks. These checks do not measure physical noise floor or on-device render
deadlines. See `docs/BLE_MIGRATION.md` for the migration's current limits.

## fm1-base / BLE port (the default build)

The `platform/` target uses the pinned fm1-base and felucca-bsp
revisions in `tools/build_base.py` (fm1-base `6c0b47e4bfc5`, BSP `0f08fcaa6610`).
`./build.sh` runs that wrapper; it can also run directly. The wrapper fetches
those revisions if absent, verifies a clean dependency checkout, regenerates
Saadet's assets through `tools/generate_assets.py` and applies the checked overlay to a disposable build copy.

```sh
PYTHON="$PWD/build/venv/bin/python" ./build.sh --no-ble
PYTHON="$PWD/build/venv/bin/python" ./build.sh
PYTHON="$PWD/build/venv/bin/python" ./build.sh --host
tests/run_base_tests.sh
```

Device packages are `platform/build/saadet-usb.fwsc` and
`platform/build/saadet.fwsc` (BLE). Both have development identity `FM-1_900`.
The BLE target needs `AC79_SDK` (default `~/fw-AC79_AIoT_SDK`) and the same
JieLi toolchain (`JIELI_TOOLCHAIN`, default `~/.jieli/toolchain`). Simulator
builds require neither the SDK nor the toolchain. Builds must run sequentially:
the two variants share a disposable overlay and compiler outputs.

The wrapper also checks CPU1's private stacks and resident polling loop,
resident audio placement, packed image bytes, engine arena and new boot entry.
Logs for this migration are `build/base-{usb,ble,host}-build.log`,
`build/base-port-tests.log` and `build/host/log/`.
The base's installer is in `build/fm1-base-platform/tools/`.
Neither these build commands nor the tests install firmware.

### Recovery bootloader and installation order

This pin supports **m-bootloader 1.2**. Every base build prepares its unmodified
FM-1 installer, plain boot image, MIT licence and source/checksum manifest in
`platform/build/bootloader/`. The checksums match Julius Vitkauskas's v1.2
release. The Saadet app package remains separate: a normal app update preserves
the device's bootloader, including m-bootloader once installed.

For the one-time migration, install the newly built Saadet app first. Its newer
BSP updater accepts the bootloader-only package; the previous Saadet build
rejects it. Then explicitly install the checked bootloader:

```sh
build/venv/bin/python tools/base_bootloader.py --check
build/venv/bin/python build/fm1-base-platform/tools/fm1_install.py platform/build/saadet.fwsc --port FM-1 --yes
build/venv/bin/python tools/base_bootloader.py --install --port FM-1
```

The last command writes the boot area and asks for confirmation (`--yes` skips
the prompt for an already authorized install). Keep power and USB connected.
It serves only the pinned FM-1 package, requires running Saadet (`FM-1_900` or a
release identity) and checks that the same app returns. It does not use the normal two-stage app
installer, which would wait for an update loader that m-bootloader's installer
does not provide. A return to Saadet alone does not prove bootloader contents:
switch off, hold **PLAY**, and switch on to verify the recovery menu. Hold
**REC** while switching on for USB download mode. OCT− + OCT+ held in the app
no longer enters update mode in this upstream revision.

The recovery menu and REC download path run before Saadet, so a broken app can
be recovered without relying on its MIDI updater. A power loss during the
bootloader replacement itself can still require the RP2040 USB_KEY recovery
route. Use the app updater for subsequent Saadet updates; do not raw-flash the
package's synthetic flash head over the device's preserved boot area.

The 0.1.1 package is prepared locally for public release, with final hardware acceptance
tracked in RELEASE_NOTES.md. BLE is a MIDI peripheral advertised as
**Saadet** in its GAP name and startup diagnostics, with
keyboard output and channel-message input. The default also enables BLE central
mode for one external MIDI device alongside one incoming phone/computer link.
GLOBAL > DEV provides discovery/connect/disconnect; GLOBAL > BLE manages the
phone link and can forget all BLE bonds and the saved reconnect target. The last
successfully connected outgoing MIDI device is remembered and retried on boot
in the background, with delays increasing to 30 seconds. Radio Off or device
Disconnect stops retries for the session. Peers with rotating private addresses
may require reselection. The existing editor and native bank imports continue over USB.
The distribution/provenance audit and physical checks are tracked in
`docs/BLE_MIGRATION.md`.

## Release site and web installer

`tools/prepare_public_release.py` builds a versioned BLE package and prepares its matching source archive,
licence bundle, checksums and static installer site. Commit the complete source tree first. Its build receipt
records the source file hashes and pinned dependency revisions; changed firmware, dirty source or a mismatched
source archive is refused. The source ZIP includes the exact fm1-base and felucca-bsp source copies as well.

```sh
build/venv/bin/python tools/prepare_public_release.py 0.1.1
python3 -m http.server 8846 --bind 127.0.0.1 --directory build/releases/0.1.1/site
```

The output directory contains the firmware, source ZIP, uploadable site ZIP, release notes, licences,
BUILD_INFO.json and SHA256SUMS. `--no-build` reuses an existing versioned package only when its build receipt
still matches. `--checks FILE.json` carries completed automated check results as VERIFICATION.json. Existing
release directories are preserved; select a new `--out` to prepare again.

Preparation is local: it does not flash, tag, push or publish. RELEASE_NOTES.md tracks the outstanding
hardware acceptance steps for the exact package. Extract the site ZIP as the web root; the
install/ page links to firmware, source and licence downloads. An unmodified source ZIP can be rebuilt without
Git history; its SOURCE_INFO.json supplies provenance, while the wrapper fetches the pinned dependencies.

The release identity must be `FM-1_9` and XXYYZZ for the given version (0.1.1: `FM-1_9000101`); the
installer checks the downloaded package's SHA-256 and identity, and the FM-1's
report after the install. `--dev` accepts the development package for local
testing. The site's installer page is one file: its modules (`web/install/`)
are bundled into it, so a cached page never mixes two releases' code.

The installer follows fm1-base's: m-bootloader is optional, off by default, is
offered only when its package matches the pinned SHA-256, and is written after
the firmware, so a refused bootloader leaves the new firmware installed. It can
also be added later to an FM-1 already running the release. Saadet's additions
make an install hard to break from the browser:

- Every answer to the FM-1's read requests is sent from the MIDI event itself,
  with the protocol's 10 ms gap as the MIDI send timestamp
  (`web/install/transfer.js`). fm1-base's loop waits on a page timer for each
  answer; a background tab clamps those to a second, then a minute, which
  stalled installs when the tab was switched.
- Before writing, a dialog asks the player to keep the tab open and in front,
  keep the cable and power connected, and confirm a backup. The install then
  takes the whole window with that warning on top; switching away changes the
  tab's title, and coming back says whether the transfer kept going.
- While writing, the page holds a Web Lock (one install per browser), a screen
  wake lock, a leave confirmation, and routes the protocol's waits through a
  worker's timers (`web/install/keepalive.js`).
- An install interrupted after the FM-1 entered update mode is resumable. The
  update record survives a power cycle (felucca-bsp loader), the page detects
  the update-mode identity, and a mark in local storage recalls it on the next
  visit.

`node web/test_install.mjs` runs the transfer against a simulated FM-1
(`web/install/fakefm1.js`) with the page's timers throttled, interrupts and
resumes an install, and installs m-bootloader after it; `tests/site_test.py`
checks the site. Both are in `tests/run_base_tests.sh`. They do not replace an
install on an FM-1 in Chrome and Edge, including with the tab in the background.
`web/install/fm1ota.js` and `fm1pkg.js` are fm1-base's, byte for byte; the test and site builder compare them
with the pinned revision. Public sites also include the firmware and editor licence texts, upstream notices,
and matching source download. Hardware validation remains recorded separately in `docs/BLE_MIGRATION.md`.

