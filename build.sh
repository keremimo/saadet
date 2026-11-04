#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Build Saadet on macOS (see BUILDING.md).
#   ./build.sh [--no-ble] [--release X.Y[.Z]]   fm1-base with Bluetooth MIDI (default): platform/build/saadet.fwsc;
#                                               --no-ble: the USB-only platform/build/saadet-usb.fwsc
#   ./build.sh --host                           the fm1-base host simulator (no toolchain or SDK)
#   ./build.sh --board smk37 --model pro|elite|mke   the SMK-37: build/saadet-smk37[-elite|-mke].fwsc (docs/SMK37_PORT.md)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
TARGET=1
for a in "$@"; do
    case "$a" in
        --legacy) echo "build.sh: the legacy platform is retired; use the fm1-base build"; exit 2 ;;
        --host) TARGET=0 ;;
    esac
done
# Pillow loads Raqm's FriBidi at run time; macOS strips DYLD_* from the environment of /bin/sh, so it is set here
[ "$(uname -s)" = Darwin ] && export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-/opt/homebrew/lib:/usr/local/lib:/usr/lib}"

"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
"$PY" -c 'from PIL import features; assert features.check("raqm")' 2>/dev/null ||
    { echo "build.sh: $PY's Pillow has no Raqm (the UI font needs OpenType features: brew install fribidi)"; exit 1; }
if [ $TARGET = 1 ]; then
    [ -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ] || { echo "build.sh: no toolchain in $JIELI_TOOLCHAIN (run tools/get_toolchain.sh)"; exit 1; }
    [ -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build.sh: no JieLi AC79 SDK in $AC79_SDK"; exit 1; }
    if [ "$(uname -s)" != Linux ] || [ "$(uname -m)" != x86_64 ]; then
        docker info >/dev/null 2>&1 || { echo "build.sh: Docker is not running"; exit 1; }
    fi
fi
exec "$PY" tools/build_base.py "$@"
