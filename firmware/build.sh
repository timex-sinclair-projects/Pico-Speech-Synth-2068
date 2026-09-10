#!/bin/sh
# Build the ZX Voice replica firmware.  Produces build/zxvoice.uf2.
# Toolchain and SDK live in ~/pico (ARM GNU Toolchain 13.3, pico-sdk 2.1.1).
set -e
cd "$(dirname "$0")"
export PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico/pico-sdk}"
TOOLCHAIN_BIN="$(ls -d "$HOME"/pico/arm-gnu-toolchain-*/bin 2>/dev/null | head -1)"
[ -n "$TOOLCHAIN_BIN" ] && export PATH="$TOOLCHAIN_BIN:$PATH"
[ -f pico_sdk_import.cmake ] || cp "$PICO_SDK_PATH/external/pico_sdk_import.cmake" .
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. >/dev/null
make -j8 2>&1 | grep -E "error|warning|Built target zxvoice" || true
ls -la zxvoice.uf2
