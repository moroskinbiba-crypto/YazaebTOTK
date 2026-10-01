#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p libs include/switch

if [[ ! -f libs/libtesla/include/tesla.hpp ]]; then
  rm -rf libs/libtesla
  git clone --depth 1 https://github.com/WerWolv/libtesla.git libs/libtesla
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 https://github.com/Insektaure/Shiny-Stash-Live-Map.git "$TMP/dmnt"
install -m 0644 "$TMP/dmnt/lib/libdmntcht.a" libs/libdmntcht.a
install -m 0644 "$TMP/dmnt/include/switch/dmntcht.h" include/switch/dmntcht.h

echo "Dependencies ready."
