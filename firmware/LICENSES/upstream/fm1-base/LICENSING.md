# Licensing

**fm1-base is MPL-2.0** (full text in `LICENSE`), file by file: every source file carries an
`SPDX-License-Identifier: MPL-2.0` header. If you change one of these files and distribute it, publish your change to
that file under MPL-2.0. Your own files, such as an app built on fm1-base, may use any licence, closed ones included.
So a firmware made of felucca-bsp, fm1-base and its own code may link JieLi's binary Bluetooth and Wi-Fi libraries.
A firmware on fm1-base's own Bluetooth stack (`ble_stack = "own"`) links none of them; the build checks it.

## Where the code comes from

| Code | Origin | Why it is MPL-2.0 |
| --- | --- | --- |
| Leo Kuroshita's code (from Felucca, through the firmwares below) | [Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only there | his permission to use the Felucca code he owns under MPL-2.0: [hugelton/felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4) |
| Charles Vestal's code | the platform layer of [fm1-x0x](https://github.com/charlesvestal/fm1-x0x), [fm1-fomni](https://github.com/charlesvestal/fm1-fomni) and the other FM-1 firmwares, GPL-3.0-only there | his own |
| Chris Farrell's code in `web/fm1ota.js` and `web/fm1pkg.js` (each file's header says which) | [fm-static](https://github.com/seajaysec/fm-static), GPL-3.0-only there | his permission to use it under MPL-2.0: [seajaysec/fm-static#1](https://github.com/seajaysec/fm-static/issues/1#issuecomment-6410795859) |
| `device/boot_lzma.h`, `tools/boot_pack.py` and `boot_image` in `device/main_fm1.c` (the LZMA-packed load images, `boot_pack = true`). Copyright (C) 2026 Kerem Kilic (Ellic Studio) and Charles Vestal | [Melodee](https://github.com/keremimo/melodee) (`firmware/src/boot_lzma.h`, `tools/boot_audio.py`, `firmware/src/main.c`), GPL-3.0-only there | its author's permission to use Melodee's code in fm1-base under MPL-2.0 (stated by fm1-base's maintainer, 2026-10-31) |
| `device/res.c`, `tools/fm1_resources.py` and the resource step of `tools/fm1_install.py` and the installer page (the resource region, `resources = FILE`); the pooled store in `device/storage.c` (APP_STORE_POOL). Copyright (C) 2026 Charles Vestal | written for fm1-base after [Melodee](https://github.com/keremimo/melodee)'s resource region (`firmware/src/res.c`, `tools/fm1_resources.py`, `web/fm1res.js`) and shared storage pool, by Kerem Kilic (Ellic Studio), GPL-3.0-only there | MPL-2.0, with its author's permission (as above) |
| `bsp/` | [felucca-bsp](https://github.com/hugelton/felucca-bsp), a git submodule | MPL-2.0 there |
| `ble/` (the Bluetooth shim and the MIDI service; `ble/wifi`, the WiFi glue) | Charles Vestal's fm1-ble and fm1-radio, used by fm1-ctl | his own; a few parts repeat the JieLi AC79 SDK (below) |
| `bt/` (the own Bluetooth stack: `bt/ble` BLE MIDI, link layer, host, SMP, central; `bt/br` BR/EDR and the A2DP source: SSP, L2CAP, AVDTP, AVRCP, SDP, AFH), its docs `docs/bt/`, tools `tools/bt/` and tests `tests/bt/`, but the third-party parts below. Copyright (C) 2026 Henri David (hdavid) | [Optimist](https://github.com/w0ts/optimist) (`3274ce5`), where its files carry `GPL-3.0-only OR MPL-2.0` | offered under either licence ([w0ts/optimist#55](https://github.com/w0ts/optimist/issues/55)), taken here under MPL-2.0. The files keep their dual SPDX header; the MPL's Exhibit B does not apply |
| `device/bt_own.c`, `device/bt_own_central.c`, `device/bt_own_audio.c` (the glue between the own stack and the platform) | Charles Vestal | his own |
| felucca-bsp's radio drivers (`bsp/chip/fm1_ble.h`, `fm1_ble_rf.h`, `fm1_bredr.h`) | Optimist, by Henri David | MPL-2.0 there, from the same offer (`bsp/NOTICE`) |

The screen reader's firmware side, `device/announce.c` and `tools/gen_words.py` (Charles Vestal's, from
[fm1-reader](https://github.com/charlesvestal/fm1-reader), where they were MIT), is MPL-2.0 like the rest, and
fm1-base holds their one copy. The older firmwares that are not on fm1-base (X0X, FoMni-1, MONK-1 and Mocky, which are
GPL) carry copies made before, which stay under their repositories' licences until fm1-reader's
`tools/sync_announce.sh` copies them again. MPL-2.0 is compatible with the GPL (its section 3.3), so the new copies
can go into those firmwares too.

## The own Bluetooth stack's other parts

Not everything an own-stack build uses is MPL-2.0. These parts keep their own licences, and their files keep their
own headers:

| What | Licence | Where |
| --- | --- | --- |
| The SBC encoder of Android's Bluetooth stack (Copyright 1999-2012 Broadcom Corporation, AOSP), for Bluetooth audio. Two files are changed and marked so (a coefficient written in decimal; `<string.h>` on a host only) | Apache-2.0 (`LICENSES/Apache-2.0.txt`) | `bt/br/sbc/`, set up by `bt/br/bt_sbc.c` |
| AOSP's SBC decoder (Copyright 2014 The Android Open Source Project, 2002-2006 Open Interface North America), unchanged, used by a host test only and not built into a firmware | Apache-2.0 | `tests/bt/ref/sbc_dec/` |
| micro-ecc by Kenneth MacKay (<https://github.com/kmackay/micro-ecc>, Copyright 2014-2015), the P-192 keys of Secure Simple Pairing | BSD-2-Clause (`LICENSES/BSD-2-Clause-micro-ecc.txt`) | `bt/br/uECC.c`, `uECC.h`, `uECC_vli.h`, `types.h`, `curve-specific.inc`, `platform-specific.inc`, `asm_*.inc` |
| The radio's start-up tables: JieLi's `rf_init` as M-VAVE's stock V15 firmware runs it (Copyright (c) 2024-2026 Zhuhai Jieli Technology Co., Ltd., AC79 AIoT SDK; Copyright (c) 2026 Zhuhai Shengke / M-VAVE, `fm1_open`), packed by Optimist; the header states the changes | Apache-2.0 | `bsp/chip/ble_rf_tables_v15.h` (`bsp/NOTICE`) |
| The radio's calibration at every start: JieLi's SDK calibration (`wl_rf_common.a`, Copyright (c) 2024-2026 Zhuhai Jieli Technology Co., Ltd.) rewritten in C by Charles Vestal; the header states the changes | Apache-2.0 | `bsp/chip/fm1_rf_cal.h` (`bsp/NOTICE`; how it works: `docs/bt/RF-CALIBRATION.md`) |
| Register writes recorded in an emulator from stock V15 and the SDK's `demo_ble` (the same JieLi and M-VAVE sources as the tables above), for reference | Apache-2.0 | `docs/bt/ble-traces/` |
| The Bluetooth Core Specification's sample data, for the tests | the specification's | `tests/bt/bt_crypto_vectors.h` |

A firmware built with the own stack carries Apache-2.0 and BSD-2-Clause code: ship `LICENSES/Apache-2.0.txt` and
`LICENSES/BSD-2-Clause-micro-ecc.txt` with it (BSD-2-Clause asks for its notice in the documentation of a binary).
The MPL-2.0 text is `LICENSE`.

Some files in `ble/` repeat parts of the JieLi AC79 SDK, Copyright (c) JIELI (Zhuhai Jieli Technology Co., Ltd.),
under the Apache License 2.0 (`LICENSES/Apache-2.0.txt`): the connection handling of `le_trans_data.c` in
`ble/midi/le_midi.c`, interface details in `ble/shim/shim.h` and `ble/shim/misc.c`, and the radio's calibration
values in `ble/shim/rf_const.c` (Apache-2.0 only). Each file's header says which parts, and carries
`MPL-2.0 AND Apache-2.0`.

The GPL copies in Felucca and in those firmwares stay under the GPL; the copies here are MPL-2.0. No code by other
Felucca contributors is in fm1-base: the history of every file was checked when it was imported (from fm1-fomni
`7e7547f`).

## Not part of fm1-base

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors) | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK files a package needs (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | Apache-2.0 | read from your SDK checkout at build time; never in this repository |
| JieLi's Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, ...: binary only) and the few SDK sources and headers they need | Apache-2.0 | copied from your SDK checkout into `build/` by a build with `ble = true` on JieLi's stack (not with `ble_stack = "own"`); never in this repository |
| JieLi's WiFi libraries (`wl_wifi_sta_sfc.a`, `wpasupplicant.a`, `hsm.a`: binary only) and the SDK's lwIP 2.1.3 sources | Apache-2.0 (lwIP: BSD-3-Clause) | copied from your SDK checkout into `build/` by a build with `wifi = true`; never in this repository. `ble/lwip/lwipopts_sfc.h` and `ble/wifi/rf_moderate.c` are the SDK's (Apache-2.0), the first changed where marked "FM-1" |
| m-bootloader 1.2 (Julius Vitkauskas, github.com/jvitkauskas/m-bootloader): its FM-1 installer package and bootloader, which the web installer offers to install | MIT | `web/m-bootloader/` (with its `LICENSE`); a site copies the package and the licence into `firmware/` |
| The browser emulator's voice: eSpeak NG, through fm1-reader | GPL-3.0-or-later | taken from an fm1-reader checkout when the emulator is built; a separate program the page loads |
