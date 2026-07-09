# sony-msx-hbf1xv

Production-oriented **Sony HB-F1XV (MSX2+, 1988)** emulator in C++: a cycle-aware,
deterministic core (Z80A @ 3.58 MHz, Yamaha V9958 VDP with 128 KB VRAM, 64 KB RAM, PSG,
Konami SCC, YM2413 FM/MSX-MUSIC, RTC, WD2793-family FDC with 720 KB 3.5" floppy, full
slot/mapper fabric) plus an optional SDL3 desktop frontend. The authoritative hardware
target and all operating rules live in [`CLAUDE.md`](CLAUDE.md).

## Current status (2026-07-09)

- **Milestones M0–M32 are closed**; git tags `v1.0.11`…`v1.0.33` mark the closure line
  (`v1.0.NN` = milestone `MNN` through M28; `v1.0.29` tagged the live-playtesting hardening
  arc DEC-0028..DEC-0034, so tags offset milestone numbers by one from M29 on: M29=v1.0.30 …
  M32=v1.0.33). `v1.0.32` was declared the production candidate; `v1.0.33` advanced that
  line with the M32 RC-playtest fixes (raster-accurate per-line rendering + line
  interrupts, FM mix calibration).
- **Authoritative live status:** [`agent-protocol/state/current-phase.md`](agent-protocol/state/current-phase.md)
  (active phase), [`state/milestones.md`](agent-protocol/state/milestones.md),
  [`state/deferred-backlog.md`](agent-protocol/state/deferred-backlog.md) (open backlog),
  and [`channels/decisions.md`](agent-protocol/channels/decisions.md) (DEC-NNNN history).
  Always trust those over any README snapshot.
- Working today, per the closed-milestone record under `docs/`: real Sony BIOS cold boot to
  the MSX2+ logo and BASIC; MSX-DOS / Disk BASIC boot from `.dsk` images; cartridge loading
  with universal mapper auto-identification (softwaredb SHA1 match, then a bank-write
  heuristic); sprites + V9958 command engine with per-line raster rendering; live PSG + SCC
  + YM2413 FM audio; keyboard/joystick, Ren-Sha Turbo, hardware PAUSE and Speed Controller;
  ZEXALL/ZEXDOC clean (durable log `docs/m31-rc-zexall-log.txt`).

## Build and test (the single way)

**Single-build policy (DEC-0041): there is exactly ONE build tree — `build/`.** Never create
per-agent or per-purpose trees. Bootstrap everything with:

```powershell
powershell -ExecutionPolicy Bypass -File tools/bootstrap-build.ps1 -RunTests
```

This builds SDL3 once from the vendored `references/sdl3/` source into `build/_sdl3_install`
(only if missing), configures the canonical `build/` tree with `-DSONY_MSX_ENABLE_SDL3=ON`
(the superset: both executables + all tests), builds Debug, and runs `ctest`.

Manual equivalent (see `CLAUDE.md` "Build & test flow"):

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON "-DCMAKE_PREFIX_PATH=build/_sdl3_install"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- Headless-only fallback (no `SDL3Config.cmake` available): reconfigure the SAME tree with
  `-DSONY_MSX_ENABLE_SDL3=OFF`.
- Fast subset: `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` — excludes the
  ~30-minute ZEXALL/ZEXDOC sweep, which runs only at release-candidate checkpoints or after
  direct `src/devices/cpu/` / `src/core/` edits.
- Executables land in `build/Debug/`: `sony_msx_headless.exe`, `sony_msx_sdl3.exe`.

## Run

Run from the repository root (asset paths and the default softwaredb path are CWD-relative).

**SDL3 frontend** (real window, throttled real-time loop, live audio/input):

```powershell
build\Debug\sony_msx_sdl3.exe                                  # plain BIOS boot to BASIC
build\Debug\sony_msx_sdl3.exe --cart1 roms\aleste.rom          # cartridge, mapper auto-identified
build\Debug\sony_msx_sdl3.exe --disk disks\msxdos22.dsk        # MSX-DOS boot floppy
```

Flags (all verified in `src/frontend/sdl3_main.cpp` / `src/frontend/sdl3_cli.cpp`):
`--bios-dir <path>` (default `bios`), `--disk <path>`, `--cart1 <path>`,
`--cart1-type <name>|auto`, `--cart2 <path>`, `--cart2-type <name>|auto`,
`--softwaredb <path>` (default `references/openmsx-21.0/share/softwaredb.xml`),
`--max-frames <N>`, `--hidden-window`, `--border`, `--dump-state <name>`,
`--trace-cpu <name>`, `--event-log <name>`, `--input-script <path>`, `--help`.
`--cartN-type` omitted (or `auto`) triggers auto-identification; an explicit type is
honored byte-for-byte.

**Headless** (`sony_msx_headless.exe`) is mode-driven; the main mode is:

```powershell
build\Debug\sony_msx_headless.exe --debug-session bios 0 --disk disks\msxdos22.dsk `
    --frames 1000 --dump-frame boot.frame --dump-state state.txt --event-log run.log
```

`--debug-session <bios_dir> <max_steps>` accepts `--disk`, `--cart1/--cart1-type`,
`--cart2/--cart2-type`, `--softwaredb`, `--debug-root`, `--dump-state`, `--trace-cpu`,
`--event-log`, `--input-script`, `--frames <N>` (drive N real frames; `<max_steps>` then
ignored), `--dump-frame <name>`. Other single-purpose headless modes (each prints usage):
`--cpm-run`, `--parity-trace`, `--bios-boot-trace`, `--frame-dump-demo`,
`--audio-dump-demo`, and the per-milestone parity probes (`--vdp-parity`,
`--vdp-render-parity`, `--sprite-cmd-parity`, `--ym2413-parity`, `--halnote-parity`).
Debug output lands under `debug/` (see [`debug/README.md`](debug/README.md)).

## Repository layout

- `src/` — emulator source; folder boundaries in [`src/CLAUDE.md`](src/CLAUDE.md)
  (`core`, `devices`, `peripherals`, `machine`, `frontend`).
- `tests/` — deterministic unit/integration/system tests + `tests/parity/` fixtures;
  conventions in [`tests/CLAUDE.md`](tests/CLAUDE.md).
- `tools/` — PowerShell/Python automation (build bootstrap, evidence gates, A/B harnesses,
  converters); index in [`tools/README.md`](tools/README.md).
- `docs/` — frozen milestone packages/reports/sign-offs + investigation reports; taxonomy in
  [`docs/README.md`](docs/README.md).
- `agent-protocol/` — multi-agent coordination: live state (`state/`), append-only channels
  (`channels/`), canonical baseline/guardrails text.
- `references/` — read-only grounding sources (openMSX 21.0, fMSX 6.0, SDL3, fact sheets,
  ZEXALL). Never copied into `src/` (license isolation).
- `bios/`, `roms/`, `disks/` — local, legally-sourced development assets (see below);
  `bios/` contents documented in [`bios/README.md`](bios/README.md).
- `debug/` — runtime debug output (traces/logs/frames/sounds); mostly gitignored, with
  committed evidence exceptions ([`debug/README.md`](debug/README.md)).
- `.claude/` — agents, commands, workflow, and settings for the orchestration.
- `build/` — the one canonical CMake tree (gitignored; recreate any time with
  `tools/bootstrap-build.ps1`).

## Assets (BIOS / ROM / disk policy)

`bios/` (the seven Sony HB-F1XV system ROMs), `roms/` (cartridge images), and `disks/`
(MSX-DOS system disks + `disks/games/` floppy sets, e.g. the two-disk YS II) are local,
legally-sourced development assets. **They remain third-party IP; this project asserts no
redistribution rights.** Per **DEC-0047** the repository owner has made an informed decision to
host this repo publicly with `bios/` included (`roms/`/`disks/` content is untracked, but their
binaries remain in pre-`b5efd29` history). That public-exposure risk is the owner's accepted
responsibility; contributors must still not claim redistribution rights or fabricate provenance.
Validate and checksum with:

```powershell
./tools/validate-assets.ps1
./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
```

## Reference A/B validation

openMSX (at `/usr/bin/openmsx` inside WSL) is the primary behavior reference; fMSX 6.0 is
the independent cross-reference. Behavior-affecting milestones carry A/B evidence produced
by the `tools/openmsx-*.ps1` harnesses and recorded as `docs/m<N>-parity-trace-diff.md`
(smoke: `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md`).

## Multi-agent workflow

The repository runs a Claude Code native orchestration — Human → Coordinator →
`msx-planner` / `msx-developer` / `msx-qa` specialists, planner-first, with QA sign-off
gating every milestone. Full rules in [`CLAUDE.md`](CLAUDE.md); protocol data in
[`agent-protocol/`](agent-protocol/README.md).
