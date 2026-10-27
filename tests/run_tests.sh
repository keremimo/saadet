#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Saadet host tests. Run ./build.sh and npm ci --prefix web first.
set -eu
cd "$(dirname "$0")/.."
OUT=build/host
LOG=$OUT/log
mkdir -p "$LOG" build/fm6_demo
CC=${CC:-cc}
compile() { "$CC" -O2 -DAUDIO_FRAC_BITS=0 -w -Ibuild/gen -Ifirmware/src "tests/$1.c" -lm -pthread -o "$OUT/$1" >"$LOG/$1.compile.log" 2>&1 || { cat "$LOG/$1.compile.log"; exit 1; }; }
run() { name=$1; shift; echo "Checking $name"; "$@" >"$LOG/$name.log" 2>&1 || { cat "$LOG/$name.log"; exit 1; }; }
# These inherited fixtures express audio magnitudes in Q15. Exercise their
# original numeric contracts separately from the shipping Q23 path below.
for name in synth_test prophet_knob_test prophet_test prophet_fast_test j106_test fm6_test fm6_ams_test fm6_prepare_test cz_window_test synth_fpu_test mod_route_test mod_spread_test control_prepare_test dual_core_test midi_uart_test midi_timing_test input_test storage_test power_test audio_boot_test usb_audio_test usb_audio_driver_test theme_test text_ref_test settings_test settings_usb_test preferences_compat_test main_scratch_test core1_stop_test core1_fallback_test libc_word_test ui_test ui_erase_test midi_input_test; do
 compile "$name"
 if [ "$name" = fm6_test ]; then run "$name" "$OUT/$name" build/fm6_demo; else run "$name" "$OUT/$name"; fi
done
"$CC" -O2 -DMELODEE_LOADER tests/main_scratch_test.c -o "$OUT/loader_scratch_test"
run loader_scratch "$OUT/loader_scratch_test"
# The shipping updater/package checks run in tests/run_base_tests.sh.
run usb-descriptors python3 tests/usb_audio_desc_test.py
run audio-ram python3 tests/audio_ram_test.py
run installer python3 tests/install_test.py
run recovery python3 tests/recovery_test.py
run editor-protocol node web/test_synth.mjs
run editor-dom node web/test_dom.mjs
# Bounds checks are separate from sample/cost tests so instrumentation cannot skew their timings.
"$CC" -O1 -g -DAUDIO_FRAC_BITS=0 -w -fsanitize=address -fno-omit-frame-pointer -Ibuild/gen -Ifirmware/src tests/synth_test.c -lm -o "$OUT/synth_asan"
run synth-address "$OUT/synth_asan"
# Shipping precision: complete synth/persistence integration, numerical
# contracts, concurrent worker parity, low-level audio and RAM layout.
for name in synth_test synth_fpu_test dual_core_test audio_precision_test j106_test; do
 "$CC" -O2 -w -Ibuild/gen -Ifirmware/src "tests/$name.c" -lm -pthread -o "$OUT/native-$name"
 run "native-$name" "$OUT/native-$name"
done
# The host fixture strips target attributes (including 8-byte alignment), and
# upstream phase code uses signed shifts. Keep bounds and overflow checking on.
"$CC" -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift,alignment -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Ibuild/gen -Ifirmware/src tests/audio_precision_test.c -lm -pthread -o "$OUT/precision_asan"
run precision-address-overflow "$OUT/precision_asan"
"$CC" -O1 -g -w -DAUDIO_FRAC_BITS=0 -fsanitize=address,undefined -fno-sanitize=shift,alignment -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Ibuild/gen -Ifirmware/src tests/j106_test.c -lm -pthread -o "$OUT/j106_asan"
run j106-address-overflow "$OUT/j106_asan"
printf '%s\n' 'ALL HOST TESTS PASSED'
