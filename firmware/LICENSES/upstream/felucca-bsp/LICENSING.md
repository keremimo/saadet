# Licensing

**Status: settled.** The contributors whose Felucca code is here have answered: Leo Kuroshita for the code he owns
(https://github.com/hugelton/felucca-bsp/issues/4) and keremimo for his part of `chip/usb.c` (below).

Every file under `chip/`, `boards/`, `loader/` and `examples/` is MPL-2.0 (SPDX header in each file). The full text
is in `LICENSE`. The files carry no "Incompatible With Secondary Licenses" notice, so they can
be combined with GPL code under MPL-2.0 section 3.3.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.

The JieLi AC79 SDK is Apache-2.0 and is not part of this repository. See `NOTICE`.

Files were first published in Felucca under GPL-3.0-only. The copyright holder relicensed
these files here; copies already distributed under GPL-3.0-only stay under that licence.

## chip/usb.c

`chip/usb.c` is MPL-2.0 like the rest. One part of it came into Felucca from another contributor under GPL-3.0-only:
the MIDI clock and transport sync by keremimo (Felucca commit 019e2ad, through Felucca pull request 8). keremimo
agreed to license it, and the TRS ring fix 57f2aa0, under MPL-2.0: https://github.com/hugelton/felucca-bsp/pull/3. Firmwares and loader images built
from this tree no longer become GPL-3.0 through it. Felucca's own copy of `usb.c` stays GPL-3.0-only (see below).

## Forks of Felucca

Using this repository does not relicense a fork of Felucca. A fork still contains Felucca's code, which is
GPL-3.0-only, and only its copyright holders can change that. Files copied from Felucca's tree are GPL-3.0-only even
when this repository carries the same file under MPL-2.0; only the copy taken from here is MPL-2.0.

To publish under MPL-2.0 (or any other licence), start a new project, import this repository, and write the rest
yourself. Do not copy Felucca's code into it. The other direction is fine: MPL-2.0 files from here can go into a
GPL project.

## Tools

`tools/fm1pkg.py` (package builder) and `tools/fm1_install.py` (USB-MIDI installer) are MPL-2.0, moved here from Felucca
by their only author. `tools/fm1_rescue.py` and `fm1_rescue.sh` (rescue) are MPL-2.0, from fm1-x0x by their author; the
rescue downloads, when it runs, JieLi's flash loader from jl-uboot-tool and M-VAVE's stock firmware, both checked by
hash, neither in this repository. Packages built by `fm1pkg.py` contain three files of the JieLi AC79 SDK (`uboot.boot`,
`cfg_tool.bin`, `eq_cfg_hw.bin`), which are Apache-2.0: ship `LICENSES/Apache-2.0.txt` with every package.

The JieLi SDK is never copied into this repository. `tools/get_sdk.sh` fetches it from JieLi's repository at a fixed revision and
checks the hashes of the files this repository depends on. Its libraries (`*.a`) carry no separate licence statement
of their own in the SDK; the SDK as a whole is declared Apache-2.0. That has not been confirmed with JieLi.
