#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LIBTESLA_REPO="https://github.com/masagrator/Status-Monitor-Deux.git"
LIBTESLA_COMMIT="23a97111808d74993ece25b8bab79402fa2d7880"
DMNT_REPO="https://github.com/Insektaure/Shiny-Stash-Live-Map.git"
DMNT_COMMIT="548896d0cc3b5fd531bd970f0708ecda13338492"

mkdir -p "$ROOT/libs" "$ROOT/include/switch"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

rm -rf "$ROOT/libs/libtesla"
git clone --filter=blob:none --no-checkout "$LIBTESLA_REPO" "$TMP/tesla"
git -C "$TMP/tesla" checkout --detach "$LIBTESLA_COMMIT"
cp -a "$TMP/tesla/lib/libtesla" "$ROOT/libs/libtesla"

rm -f "$ROOT/libs/libdmntcht.a" "$ROOT/include/switch/dmntcht.h"
git clone --filter=blob:none --no-checkout "$DMNT_REPO" "$TMP/dmnt"
git -C "$TMP/dmnt" checkout --detach "$DMNT_COMMIT"
install -m 0644 "$TMP/dmnt/lib/libdmntcht.a" "$ROOT/libs/libdmntcht.a"
install -m 0644 "$TMP/dmnt/include/switch/dmntcht.h" "$ROOT/include/switch/dmntcht.h"

test -s "$ROOT/libs/libdmntcht.a"
test -s "$ROOT/include/switch/dmntcht.h"
test -s "$ROOT/libs/libtesla/include/tesla.hpp"
test -s "$ROOT/libs/libtesla/include/ini_funcs.hpp"
test -s "$ROOT/libs/libtesla/include/stb_truetype.h"

echo "libtesla commit: $LIBTESLA_COMMIT"
echo "dmnt:cht source commit: $DMNT_COMMIT"
echo "Dependencies ready."
