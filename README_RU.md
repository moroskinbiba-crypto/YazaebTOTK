# TOTK Explorer v3.1.0 — reviewed build

Tesla overlay for The Legend of Zelda: Tears of the Kingdom.

Target build:
- Version: 1.4.3
- Title ID: 0100F2C0115B6000
- Build ID: 277178B7DBA1B6D4

## Changes in this reviewed build

- GitHub Actions validates `data/points.csv` before packaging.
- Artifact upload uses `include-hidden-files: true`, so `switch/.overlays/*.ovl` is retained.
- Workflow supports both `main` and `master` and exposes `workflow_dispatch`.
- Repository paths use real dot-prefixed GitHub names; no leading-underscore aliases are used.
- libtesla is fetched from tag `v1.3.3`.
- `libdmntcht.a` is verified before compile.
- Candidate storage is capped at 4096 entries and uses reservoir sampling instead of stopping at the first 20,000 matches.
- The duplicate moving-candidate vector was removed.
- Scan UI, coordinates, map and diagnostics update their `ListItem` values during `update()`.
- Build and package steps verify that the final `.ovl` exists and is non-empty.

## What is intentionally not claimed

The coordinate scanner is heuristic. It looks for float triples in the TOTK heap and filters them using player movement and vertical movement. It is not a guaranteed game-structure parser. A successful scan must be validated on the target Switch build.

The point database is intentionally empty apart from comments. The map/nearby engine reads verified points from `sd:/switch/totk_explorer/points.csv`.

## Build

Use the GitHub Actions workflow `TOTK Explorer Build`, or a devkitPro/devkitA64 environment with:

```sh
bash tools/setup_deps.sh
make clean
make -j2
```

The result is `TOTK-Explorer-v3.ovl`.

## Installation

Copy the `switch` directory from the build artifact to the root of the microSD card:

```text
sd:/switch/.overlays/TOTK-Explorer-v3.ovl
sd:/switch/totk_explorer/points.csv
```
