#!/bin/sh
# Build the ZX Voice replica firmware for both boards.
#   build/zero2023/zxvoice-zero2023.uf2   the 2023 RP2040-Zero card
#   build/revb/zxvoice-revb.uf2           the Rev B card
# Toolchain and SDK live in ~/pico (ARM GNU Toolchain 13.3, pico-sdk 2.1.1).
set -e
cd "$(dirname "$0")"
export PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico/pico-sdk}"
TOOLCHAIN_BIN="$(ls -d "$HOME"/pico/arm-gnu-toolchain-*/bin 2>/dev/null | head -1)"
[ -n "$TOOLCHAIN_BIN" ] && export PATH="$TOOLCHAIN_BIN:$PATH"
[ -f pico_sdk_import.cmake ] || cp "$PICO_SDK_PATH/external/pico_sdk_import.cmake" .
for b in ${BOARDS:-zero2023 revb}; do
	case $b in zero2023) def=BOARD_ZERO2023;; revb) def=BOARD_REVB;; *) echo "unknown board $b"; exit 1;; esac
	mkdir -p build/$b
	( cd build/$b && cmake -DCMAKE_BUILD_TYPE=Release -DZXV_BOARD=$def ../.. >/dev/null && \
	  make -j8 2>&1 | grep -E "error|warning|Built target zxvoice" || true )
	ls -la build/$b/*.uf2
done
