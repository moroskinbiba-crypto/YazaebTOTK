#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LIBULTRAHAND_REPO="https://github.com/ppkantorski/libultrahand.git"
LIBULTRAHAND_COMMIT="856ddbddd796fc4a59ad2e0bf939c5963e6f9dd2"
DMNT_REPO="https://github.com/Insektaure/Shiny-Stash-Live-Map.git"
DMNT_COMMIT="548896d0cc3b5fd531bd970f0708ecda13338492"

mkdir -p "$ROOT/libs" "$ROOT/include/switch"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

rm -rf "$ROOT/libs/libultrahand"
git clone --filter=blob:none --no-checkout "$LIBULTRAHAND_REPO" "$TMP/ultrahand"
git -C "$TMP/ultrahand" checkout --detach "$LIBULTRAHAND_COMMIT"

mkdir -p "$ROOT/libs/libultrahand"
cp -a "$TMP/ultrahand/ultrahand.mk" "$ROOT/libs/libultrahand/"
cp -a "$TMP/ultrahand/common" "$ROOT/libs/libultrahand/"
cp -a "$TMP/ultrahand/libultra" "$ROOT/libs/libultrahand/"
cp -a "$TMP/ultrahand/libtesla" "$ROOT/libs/libultrahand/"

rm -f "$ROOT/libs/libdmntcht.a" "$ROOT/include/switch/dmntcht.h"
git clone --filter=blob:none --no-checkout "$DMNT_REPO" "$TMP/dmnt"
git -C "$TMP/dmnt" checkout --detach "$DMNT_COMMIT"
install -m 0644 "$TMP/dmnt/lib/libdmntcht.a" "$ROOT/libs/libdmntcht.a"
install -m 0644 "$TMP/dmnt/include/switch/dmntcht.h" "$ROOT/include/switch/dmntcht.h"

test -s "$ROOT/libs/libultrahand/ultrahand.mk"
test -s "$ROOT/libs/libultrahand/libtesla/include/tesla.hpp"
test -s "$ROOT/libs/libultrahand/libultra/include/ini_funcs.hpp"
test -d "$ROOT/libs/libultrahand/libultra/source"
test -d "$ROOT/libs/libultrahand/common"
test -s "$ROOT/libs/libdmntcht.a"
test -s "$ROOT/include/switch/dmntcht.h"

echo "libultrahand commit: $LIBULTRAHAND_COMMIT"
echo "dmnt:cht source commit: $DMNT_COMMIT"
echo "Dependencies ready."
