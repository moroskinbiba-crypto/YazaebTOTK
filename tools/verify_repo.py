#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
errors = []

def required(path: str):
    if not (root / path).is_file():
        errors.append(f"missing file: {path}")

for p in [
    '.github/workflows/build.yml',
    'Makefile',
    'data/points.csv',
    'include/explorer.hpp',
    'source/main.cpp',
    'source/memory.cpp',
    'source/map.cpp',
    'source/ui.cpp',
    'source/log.cpp',
    'tools/setup_deps.sh',
]:
    required(p)

workflow = (root / '.github/workflows/build.yml').read_text(encoding='utf-8')
for needle in [
    'workflow_dispatch:',
    'include-hidden-files: true',
    'make clean',
    'make -j2',
    'TOTK-Explorer-v3.nro',
    'TOTK-Explorer-v3.ovl',
]:
    if needle not in workflow:
        errors.append(f"workflow missing: {needle}")

makefile = (root / 'Makefile').read_text(encoding='utf-8')
for needle in [
    'libs/libultrahand/ultrahand.mk',
    'export NROFLAGS += --nacp=$(TOPDIR)/$(TARGET).nacp',
    "printf 'ULTR' >> $@",
    '$(OUTPUT).ovl: $(OUTPUT).nro',
]:
    if needle not in makefile:
        errors.append(f"Makefile missing: {needle}")

source = (root / 'source/memory.cpp').read_text(encoding='utf-8')
for needle in [
    'MAX_CANDIDATES = 65536',
    'EXACT_RETRY_TICKS = 30',
    'clearDiscoveryState()',
    'dmntchtInitialize()',
    'dmntchtExit()',
    'Result ensureMemory()',
]:
    if needle not in source:
        errors.append(f'memory.cpp missing: {needle}')

if 'addReservoirCandidate' in source or 'rngState' in source:
    errors.append('memory.cpp still contains unreliable reservoir scanner state')

main = (root / 'source/main.cpp').read_text(encoding='utf-8')
if 'initMemory()' in main:
    errors.append('main.cpp must not initialize dmnt directly before GUI startup')
if 'shutdownMemory()' not in main:
    errors.append('main.cpp missing shutdownMemory cleanup')
if '--hud' not in main:
    errors.append('main.cpp missing --hud mode dispatch')
if 'TotkExplorerHudOverlay' not in main:
    errors.append('main.cpp missing persistent HUD overlay class')

ui = (root / 'source/ui.cpp').read_text(encoding='utf-8')
for forbidden in [
    '::backgroundColor', '::FullMode', '::deactivateOriginalFooter',
    'tsl::gfx::Color', 'drawRoundRect(', 'hidScanInput(', 'CONTROLLER_P1_AUTO',
]:
    if forbidden in ui:
        errors.append(f'ui.cpp uses incompatible persistent-HUD API: {forbidden}')
if 'ex::ensureMemory();' not in ui:
    errors.append('ui.cpp missing dmnt reconnect on GUI resume')
if 'tsl::setNextOverlay(path, "--hud")' not in ui:
    errors.append('ui.cpp missing persistent HUD transition')
if 'tsl::hlp::requestForeground(false)' not in ui:
    errors.append('ui.cpp missing game foreground handoff')
if 'L + R + Minus' not in ui:
    errors.append('ui.cpp missing persistent HUD close chord')

version = (root / 'include/explorer.hpp').read_text(encoding='utf-8')
if 'VERSION = "3.5.0"' not in version:
    errors.append('version mismatch: expected 3.5.0')

points = [
    line for line in (root / 'data/points.csv').read_text(encoding='utf-8').splitlines()
    if line.strip() and not line.lstrip().startswith('#')
]
if len(points) != 152:
    errors.append(f'points.csv expected 152 data rows, got {len(points)}')
for idx, line in enumerate(points, 1):
    if len(line.split(',')) != 6:
        errors.append(f'points.csv row {idx}: expected 6 columns')
    else:
        layer = line.split(',')[5]
        if layer not in {'Surface', 'Sky', 'Depths'}:
            errors.append(f'points.csv row {idx}: invalid layer {layer!r}')

setup = (root / 'tools/setup_deps.sh').read_text(encoding='utf-8')
expected_pin = '1b7a64a4d73489c870f3fb9caa9927e9a2347478'
if expected_pin not in setup:
    errors.append('setup_deps.sh does not use the audited Tetris libultrahand pin')

if errors:
    print('VERIFY FAILED')
    for e in errors:
        print(' -', e)
    sys.exit(1)

print('VERIFY OK')
print(' - project layout: OK')
print(' - current 3.5.0 architecture: OK')
print(' - NACP/ULTR build hooks: OK')
print(' - lazy dmnt:cht lifecycle: OK')
print(' - points.csv: 152 valid rows')
print(' - libultrahand pin: Tetris reference commit')
