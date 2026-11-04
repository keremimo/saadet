#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# The SMK-37 build: the three packages with their licence check (tools/check_base.py), then the host suite against the
# SMK's store map and simulator. Installs nothing. Needs the board layer's worktree (tools/build_base.py SMK_SOURCE).
set -eu
cd "$(dirname "$0")/.."
PYTHON=${PYTHON:-build/venv/bin/python}
CC=${CC:-cc}
export PYTHON
BASE=build/fm1-base-smk37-platform
OUT=build/host-smk37
mkdir -p "$OUT"
for model in pro elite mke; do
    ./build.sh --board smk37 --model "$model"
done
./build.sh --board smk37 --model elite --host
SAADET_BOARD=smk37 SAADET_BASE="$PWD/$BASE" "$PYTHON" tools/licence_check.py
SAADET_REQUIRE_SMK37=1 "$PYTHON" tests/build_targets_test.py
# The store: the board's own windows against every live sector, then the board layer's test of those windows.
"$CC" -O1 -g -w -fsanitize=address,undefined -DSAADET_SMK37=1 "-I$BASE/bsp/boards/smk37" tests/storage_test.c -o "$OUT/storage_test"
"$OUT/storage_test"
"$CC" -O1 -g -w -I"$BASE/bsp/boards/smk37" -I"$BASE/include" "$BASE/tests/smk37_flash_test.c" -o "$OUT/smk37_flash_test"
"$OUT/smk37_flash_test"
# The instrument on the SMK's map: its settings, erase and UI suites.
for name in ui_test ui_erase_test settings_test preferences_compat_test; do
    "$CC" -O2 -DAUDIO_FRAC_BITS=0 -DSAADET_SMK37=1 -w -Ibuild/gen -Ifirmware/src "tests/$name.c" -lm -pthread -o "$OUT/$name"
    mkdir -p "$OUT/$name.out"
    UI_PNG="$OUT/$name.out" "$OUT/$name" >"$OUT/$name.log" 2>&1 || { cat "$OUT/$name.log"; exit 1; }
    echo "ok $name"
done
# The simulator on the SMK's panel: the same scripts as the FM-1's.
for script in basic ble-midi ble-connect ble-devices midi-thru; do
    mkdir -p "$OUT/$script"
    platform/build/smk37-elite/host/saadet-smk37-elite_host "platform/tests/$script.txt" "$OUT/$script" >"$OUT/$script.log" 2>&1 ||
        { cat "$OUT/$script.log"; exit 1; }
    echo "ok $script"
done
printf '%s\n' 'ALL SMK-37 TESTS PASSED'
