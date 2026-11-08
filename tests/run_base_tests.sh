#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Run after tools/build_base.py --host and a target build of the base port.
set -eu
cd "$(dirname "$0")/.."
PYTHON=${PYTHON:-build/venv/bin/python}
"$PYTHON" tests/base_port_test.py
"$PYTHON" tests/base_sysex_test.py
"$PYTHON" tests/base_cpu_test.py
"${CC:-cc}" -O1 -g -fsanitize=address,undefined tests/power_test.c -o build/base-power-test
build/base-power-test
"$PYTHON" tests/base_ble_own_test.py
"$PYTHON" tests/ble_calibration_test.py
"$PYTHON" tests/base_bootloader_test.py
"$PYTHON" tests/licence_check_test.py
"$PYTHON" tests/build_targets_test.py
# Exercise the actual pinned updater with Saadet and the boot-only package.
BASE=build/fm1-base-platform
"${CC:-cc}" -O1 -g -w -fsanitize=address -o build/base-mboot-ota-test "$BASE/tests/ota_test.c"
build/base-mboot-ota-test platform/build/saadet.fwsc platform/build/bootloader/fm1-uboot-2.3.fwsc
"$PYTHON" "$BASE/tests/install_test.py"
node "$BASE/tests/installer_boot_test.mjs" platform/build/bootloader/fm1-uboot-2.3.fwsc
# Saadet's web installer against the simulated FM-1 (background-tab throttling, resume), and its release site.
node web/test_install.mjs
"$PYTHON" tests/site_test.py
"$PYTHON" tests/release_support_test.py
"${CC:-cc}" -O1 -g -w -DAUDIO_FRAC_BITS=0 -fsanitize=address,undefined -fno-sanitize=shift,alignment -Ibuild/gen -Ifirmware/src tests/ui_ble_test.c -lm -pthread -o build/base-ui-ble-test
mkdir -p build/base-ui-ble
UI_PNG=build/base-ui-ble build/base-ui-ble-test
"${CC:-cc}" -O1 -g -w -DAUDIO_FRAC_BITS=0 -fsanitize=address,undefined -fno-sanitize=shift,alignment -Ibuild/gen -Ifirmware/src tests/base_stats_test.c -lm -pthread -o build/base-stats-test
build/base-stats-test
mkdir -p build/base-basic build/base-ble-midi build/base-ble-connect build/base-ble-devices build/base-midi-thru
platform/build/host/saadet_host platform/tests/basic.txt build/base-basic
platform/build/host/saadet_host platform/tests/ble-midi.txt build/base-ble-midi
platform/build/host/saadet_host platform/tests/ble-connect.txt build/base-ble-connect
platform/build/host/saadet_host platform/tests/ble-devices.txt build/base-ble-devices
platform/build/host/saadet_host platform/tests/midi-thru.txt build/base-midi-thru
printf '%s\n' 'ALL BASE PORT TESTS PASSED'
