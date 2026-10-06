#!/usr/bin/env python3
from pathlib import Path
import math
import re
import sys

root = Path(__file__).resolve().parents[1]
errors = []

def required(path: str):
    if not (root / path).is_file():
        errors.append(f"missing file: {path}")

for p in [
    ".github/workflows/build.yml",
    "Makefile",
    "data/points.csv",
    "include/explorer.hpp",
    "source/main.cpp",
    "source/memory.cpp",
    "source/map.cpp",
    "source/ui.cpp",
    "source/log.cpp",
    "tools/setup_deps.sh",
    "tools/logic_tests.py",
]:
    required(p)

workflow = (root / ".github/workflows/build.yml").read_text(encoding="utf-8")
for needle in [
    "workflow_dispatch:",
    "pull_request:",
    "include-hidden-files: true",
    "make clean",
    "make -j2",
    "TOTK-Explorer-v3.nro",
    "TOTK-Explorer-v3.ovl",
]:
    if needle not in workflow:
        errors.append(f"workflow missing: {needle}")

makefile = (root / "Makefile").read_text(encoding="utf-8")
for needle in [
    "libs/libultrahand/ultrahand.mk",
    "export NROFLAGS += --nacp=$(TOPDIR)/$(TARGET).nacp",
    "printf 'ULTR' >> $@",
    "$(OUTPUT).ovl: $(OUTPUT).nro",
    "APP_VERSION := 3.6.2",
]:
    if needle not in makefile:
        errors.append(f"Makefile missing: {needle}")

header = (root / "include/explorer.hpp").read_text(encoding="utf-8")
if 'VERSION = "3.6.2"' not in header:
    errors.append("version mismatch: expected 3.6.2")
if "struct Candidate" in header or "struct Profile" in header:
    errors.append("explorer.hpp still contains removed calibration structures")
for needle in [
    "enum class ScanStage { Idle, Resolving, Ready, Failed };",
    "Result ensureMemory();",
    "void startAutoScan();",
    "void resetScan();",
    "void refreshPlayer();",
]:
    if needle not in header:
        errors.append(f"explorer.hpp missing: {needle}")

memory = (root / "source/memory.cpp").read_text(encoding="utf-8")
for needle in [
    "EXACT_RETRY_TICKS = 30",
    "HEALTH_CHECK_TICKS = 120",
    "resetPlayerState()",
    "dmntchtInitialize()",
    "dmntchtExit()",
    "Result ensureMemory()",
    "resolveExactPlayerActor",
    "ACTOR_POSITION = 0x2B4",
]:
    if needle not in memory:
        errors.append(f"memory.cpp missing: {needle}")

for forbidden in [
    "SCAN_CHUNK",
    "MAX_CANDIDATES",
    "candidatesList",
    "profilePath()",
    "readProfileFile",
    "saveProfile",
    "loadProfile",
    "scanChunk",
    "filterMove",
    "filterJump",
    "reservoir",
    "rngState",
]:
    if forbidden in memory:
        errors.append(f"memory.cpp still contains removed unsafe calibration path: {forbidden}")

if "g.stage == ScanStage::Ready" not in memory or "refreshPlayer();" not in memory:
    errors.append("memory.cpp missing Ready hot path")
if "if (g.playerActor != 0 && refreshExactPlayer(false))" not in memory:
    errors.append("memory.cpp Start/Refresh path is not idempotent")
if "g.stage = ScanStage::Resolving;" not in memory:
    errors.append("memory.cpp missing resolver recovery state")
if "void resetProcessState(const char* message)" not in memory:
    errors.append("memory.cpp missing stale-process state reset helper")
if "resetProcessState(" not in memory:
    errors.append("memory.cpp stale-process reset is not used")
if "void releaseOwnedProcess()" not in memory:
    errors.append("memory.cpp missing owned dmnt process release helper")
if "releaseOwnedProcess();" not in memory:
    errors.append("memory.cpp owned dmnt release helper is not used")

main = (root / "source/main.cpp").read_text(encoding="utf-8")
if "initMemory()" in main:
    errors.append("main.cpp must not initialize dmnt directly")
if "shutdownMemory()" not in main:
    errors.append("main.cpp missing shutdownMemory cleanup")
if "TotkExplorerOverlay" not in main:
    errors.append("main.cpp missing overlay class")

ui = (root / "source/ui.cpp").read_text(encoding="utf-8")
for forbidden in [
    "::backgroundColor",
    "::FullMode",
    "::deactivateOriginalFooter",
    "tsl::gfx::Color",
    "drawRoundRect(",
    "hidScanInput(",
    "CONTROLLER_P1_AUTO",
    "setNextOverlay(",
    "--hud",
]:
    if forbidden in ui:
        errors.append(f"ui.cpp uses incompatible/obsolete API: {forbidden}")

for needle in [
    "ex::ensureMemory();",
    "tsl::changeTo<PersistentHudGui>()",
    "tsl::hlp::requestForeground(false)",
    "tsl::disableHiding = true",
    "L + R + Minus",
    "tsl::Overlay::get()->close()",
    "cachedPoints",
    "setLayerPos(0, 0)",
]:
    if needle not in ui:
        errors.append(f"ui.cpp missing: {needle}")

if "tsl::setNextOverlay(" in ui or '"--hud"' in ui:
    errors.append("Map HUD still uses a second overlay entrypoint")

points_lines = [
    line.strip()
    for line in (root / "data/points.csv").read_text(encoding="utf-8").splitlines()
    if line.strip() and not line.lstrip().startswith("#")
]
if len(points_lines) != 152:
    errors.append(f"points.csv expected 152 data rows, got {len(points_lines)}")

seen_names = set()
for idx, line in enumerate(points_lines, 1):
    parts = line.split(",")
    if len(parts) != 6:
        errors.append(f"points.csv row {idx}: expected 6 columns")
        continue
    kind, name, sx, sy, sz, layer = parts
    if not kind or not name:
        errors.append(f"points.csv row {idx}: empty type/name")
    if layer not in {"Surface", "Sky", "Depths"}:
        errors.append(f"points.csv row {idx}: invalid layer {layer!r}")
    try:
        coords = [float(sx), float(sy), float(sz)]
        if not all(math.isfinite(x) for x in coords):
            errors.append(f"points.csv row {idx}: non-finite coordinates")
    except ValueError:
        errors.append(f"points.csv row {idx}: non-numeric coordinates")
    key = (kind, name)
    if key in seen_names:
        errors.append(f"points.csv row {idx}: duplicate {kind}/{name}")
    seen_names.add(key)

# Sanity-check known anchors used by the runtime/package verifier.
anchors = {
    "Kyononis Shrine": ("-205", "451", "35", "Surface"),
    "Ukouh Shrine": ("275", "-913", "1474", "Sky"),
}
for name, expected in anchors.items():
    matches = [p.split(",") for p in points_lines if len(p.split(",")) == 6 and p.split(",")[1] == name]
    if len(matches) != 1 or tuple(matches[0][2:6]) != expected:
        errors.append(f"points.csv anchor mismatch: {name}")

setup = (root / "tools/setup_deps.sh").read_text(encoding="utf-8")
for expected in [
    "1b7a64a4d73489c870f3fb9caa9927e9a2347478",
    "548896d0cc3b5fd531bd970f0708ecda13338492",
]:
    if expected not in setup:
        errors.append(f"setup_deps.sh missing dependency pin: {expected}")

# Architecture/state-machine invariants.
if "ScanStage::Resolving" not in ui:
    errors.append("Calibration UI missing Resolving state")
if "ScanStage::WaitMove" in ui or "ScanStage::WaitJump" in ui:
    errors.append("Calibration UI still references removed movement stages")
if "captureMove" in ui or "captureJump" in ui:
    errors.append("Calibration UI still references removed capture handlers")
if "std::vector<ex::Point> cachedPoints" not in ui:
    errors.append("Persistent HUD cache missing")

if errors:
    print("VERIFY FAILED")
    for err in errors:
        print(" -", err)
    sys.exit(1)

print("VERIFY OK")
print(" - version 3.6.2")
print(" - dependency pins: OK")
print(" - exact Player-only runtime: OK")
print(" - persistent HUD lifecycle: OK")
print(" - calibration state machine: OK")
print(" - points.csv: 152 valid, unique rows")
print(" - package/build hooks: OK")
