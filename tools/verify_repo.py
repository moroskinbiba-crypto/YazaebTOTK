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
    'tools/setup_deps.sh',
]:
    required(p)

for p in root.rglob('*'):
    if p.is_file() and p.name.startswith('_'):
        errors.append(f"bad leading-underscore filename: {p.relative_to(root)}")

workflow = (root / '.github/workflows/build.yml').read_text(encoding='utf-8')
for needle in [
    'workflow_dispatch:',
    'branches: [ main, master ]',
    'include-hidden-files: true',
    'test -s artifact/sd/switch/.overlays/TOTK-Explorer-v3.ovl',
    'test -f data/points.csv',
]:
    if needle not in workflow:
        errors.append(f"workflow missing: {needle}")

makefile = (root / 'Makefile').read_text(encoding='utf-8')
if 'DATA :=' in makefile:
    errors.append('Makefile should not define DATA; CSV is packaged by CI, not compiled')
if '$(TOPDIR)/libs/libdmntcht.a -lnx' not in makefile:
    errors.append('Makefile does not use explicit libdmntcht.a path')
if makefile.count('$(MAKE) --no-print-directory -C $@ -f $(TOPDIR)/Makefile all') != 1:
    errors.append('unexpected recursive make structure')

source = (root / 'source/memory.cpp').read_text(encoding='utf-8')
if 'MAX_CANDIDATES = 4096' not in source:
    errors.append('candidate cap was not reduced to 4096')
if 'std::vector<Candidate> moving' in source:
    errors.append('duplicate moving candidate vector still present')
if 'addReservoirCandidate' not in source:
    errors.append('reservoir sampling helper missing')

ui = (root / 'source/ui.cpp').read_text(encoding='utf-8')
if 'setValue(' not in ui:
    errors.append('UI does not update live ListItem values')

version = (root / 'include/explorer.hpp').read_text(encoding='utf-8')
if 'VERSION = "3.1.0"' not in version:
    errors.append('version mismatch in explorer.hpp')

if errors:
    print('VERIFY FAILED')
    for e in errors:
        print(' -', e)
    sys.exit(1)

print('VERIFY OK')
print(' - repository paths: OK')
print(' - workflow packaging: OK')
print(' - Makefile recursion: OK')
print(' - candidate memory cap: OK')
print(' - live UI update hooks: OK')
print(' - version: 3.1.0')
