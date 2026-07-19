# tools/ — developer & QA helpers

Developer/QA helper scripts for working on the Sony HB-F1XV emulator: build-asset gates,
deterministic probe/fixture generators, dump-to-media converters, A/B analyzers, evidence-capture
harnesses, and openMSX reference-parity drivers. **None of these are needed to build or run the
emulator** — the published one-command build bootstrap lives in `setup/` (`setup/build.ps1`,
`setup/build.sh`). All scripts assume the **repository root** as the working directory (they refer
to `bios/`, `build/`, `tests/parity/`, `debug/`, and each other by repo-root-relative paths).

## gates/ — asset & repository gates

- `validate-assets.ps1` — asserts the seven required Sony HB-F1XV BIOS ROMs exist in `bios/`
  and at least one `.rom` in `roms/`; `-Json` for machine-readable output.
- `checksum-assets.ps1` — reproducible checksums (SHA256 by default) of `bios/` + `roms/`
  assets; `-OutFile docs/asset-checksums.txt`.
- `audit-case-sensitivity.py` — verifies true on-disk casing of every quoted `#include` and the
  runtime asset-literal inventory, so the repo is safe on a case-sensitive filesystem.

## openmsx/ — openMSX A/B parity harnesses

Reference-parity drivers against openMSX (WSL `/usr/bin/openmsx` or the local Windows install);
most delegate the trace legs to `trace-parity.ps1` and diff via `analyze/trace-diff.py`.

- `ab-smoke.ps1` — capability smoke check: verifies openMSX exposes a usable headless
  single-step/debug interface and writes `docs/openmsx-ab-smoke.md`.
- `blanking-ab.ps1` — R#1 BL (display-enable) transition A/B over a frame-indexed input driver.
- `boot-parity.ps1` — FDC/WD2793 boot + register-sequence trace parity (BIOS boot and FDC probe).
- `capture-frames.ps1` — per-frame `screenshot -raw` capture from openMSX with frame-indexed
  keymatrix input; the openMSX leg feeding `analyze/sprite-oracle.py`.
- `cartridge-id-ab.ps1` — cartridge mapper auto-identification agreement check (no type forcing).
- `cartridge-parity.ps1` — cartridge-slot mapper trace parity with a synthetic Generic8kB image.
- `cpu-parity.ps1` — per-instruction Z80 CPU checkpoint parity (extends `trace-parity.ps1`).
- `disk-boot-parity.ps1` — full-boot / auto-disk-boot-trigger investigation trace.
- `fdc-read-timing-ab.ps1` — WD2793 Type-II Read Sector first-DRQ rotational-latency A/B.
- `fm-audio-ab.ps1` — audio A/B for the ultrasonic-silence property (openMSX
  `record start -audioonly` reference capture, analyzed by `analyze/audio-ab.py`).
- `halnote-parity.ps1` — Halnote / MSX-JE firmware-mapper parity harness.
- `halt-refresh-parity.ps1` — Z80 HALT R-register refresh-timing evidence (single-HALT program).
- `io-parity.ps1` — S1985 "MSX-ENGINE" I/O-bus probe trace parity.
- `memory-parity.ps1` — RAM/ROM probe + BIOS-boot trace parity on the genuine Sony_HB-F1XV
  machine.
- `peripheral-io-parity.ps1` — Kanji-font ROM + printer + cassette I/O parity.
- `rensha-turbo-parity.ps1` — speed controller + hardware PAUSE + Ren-Sha Turbo autofire
  evidence.
- `scc-parity.ps1` — KonamiSCC banking + SCC register-window parity (synthetic image).
- `split-screen-ab.ps1` — synthetic split-screen (line-interrupt) frame A/B, compared
  region-structurally by `analyze/split-screen-ab.py`.
- `sprite-command-parity.ps1` — sprite + VDP-command-engine A/B over the generated probes.
- `trace-parity.ps1` — the shared per-instruction opcode-trace parity harness (our
  `--parity-trace` vs an openMSX chained single-step Tcl script).
- `vdp-irq-parity.ps1` — VDP-IRQ register-write /INT re-evaluation parity.
- `vdp-parity.ps1` — V9958 register / VRAM / status probe trace parity.
- `vdp-render-parity.ps1` — VDP rendering-path derived-value A/B over the render probes.
- `ym2413-parity.ps1` — YM2413 (OPLL) register-write parity (two-port protocol).
- `zexall-parity.ps1` — bounded-prefix ZEXALL/ZEXDOC per-instruction trace parity.

## gen/ — deterministic probe / fixture / asset generators

Everything here is a pure function of constants and inputs (no wall clock), so generated
fixtures are byte-reproducible.

- `app-icon.py` — embeds the owner-provided taskbar PNG into `src/frontend/app_icon_data.h`
  (64×64 RGBA for `SDL_SetWindowIcon`).
- `basic-to-disk.py` — places ASCII MSX-BASIC programs / named payloads onto a formatted-blank
  720 KB FAT12 image (deterministic injector; `--read` re-extracts files for round-trip checks).
- `boot-disk.py` — deterministic 720 KB 2DD disk-image fixture (`tests/parity/m16_boot.dsk`,
  byte-for-byte port of `DiskImage::synthesize()`).
- `cartridge-probe.py` — synthetic Generic8kB cartridge image + Z80 driver probe
  (`tests/parity/`).
- `fdc-probe.py` — WD2793 restore / read-sector Z80 probe program.
- `format-blank-disk.ps1` — writes a TRUE formatted-blank 720 KB (2DD) FAT12 disk image, the
  write target for a game disk save.
- `halnote-probe.py` — synthetic 1 MB Halnote-mapper image with per-bank marker bytes.
- `peripheral-io-probe.py` — one combined Kanji / printer / cassette Z80 I/O probe.
- `scc-probe.py` — synthetic KonamiSCC cartridge image + SCC register probe.
- `split-screen-probe.py` — the openMSX-side Z80 program for the split-screen A/B.
- `sprite-command-probe.py` — flat-RAM Z80 probes for sprite modes + the VDP command engine.
- `typed-basic-program.py` — emits, from one (row,col,shift) keystroke sequence, BOTH engines'
  input forms (our `HBF1XV-INPUT-SCRIPT v1` + an openMSX keymatrix Tcl driver).
- `vdp-render-probe.py` — four flat-RAM Z80 VDP render-mode probes (palette sweep and friends).
- `vdp-scroll-cart.py` — family of deterministic 16 KB INIT-autostart cartridge ROMs, one per
  V9958 scroll scenario (also exports the tiny Z80 assembler other generators reuse).
- `video-cart.py` — screen-mode + sprite scenario cartridge assembler (imports
  `vdp-scroll-cart.py`'s assembler, never duplicates it).
- `ym2413-probe.py` — YM2413 two-port register-write probe covering the named address families.

## convert/ — dump-format converters

- `audio-dump-to-wav.py` — decoded PSG audio dump (`HBF1XV-AUDIO-DUMP v1`) → real playable
  PCM WAV.
- `frame-to-ascii.py` — renders display rows of a frame dump as ASCII, marking non-border
  pixels so content scanlines are countable.
- `frame-to-png.py` — decoded frame dump (`HBF1XV-FRAME-DUMP v1`, from `--dump-frame`) →
  truecolor PNG.
- `frame-zoom-crop.py` — crops a row band from a frame dump and upscales it (nearest) into a
  PNG with an optional scanline grid.
- `mem-to-audio.py` — raw memory buffer or dump region → INERT raw-PCM WAV (no PSG/OPLL
  synthesis; a reversible container around the bytes).
- `mem-to-png.py` — raw memory buffer or a `--region DRAM|SRAM|VRAM` dump slice → INERT
  grayscale PNG (pixel value == byte value; no VDP decoding).
- `omr-to-input-script.py` — converts an openMSX `<replay>` keyboard-matrix event log into our
  deterministic `HBF1XV-INPUT-SCRIPT v1` format.

## analyze/ — A/B comparators & report analyzers

- `audio-ab.py` — high-pitch-burst metric analyzer for WAV captures (block ZCR + AC-RMS
  burst detector).
- `envelope-frequency.py` — measures the PSG hardware-envelope modulation frequency of a WAV
  (short-window RMS envelope → FFT).
- `save-corruption-diff.py` — diffs a captured FDC write-trace sector against the FM-PAC
  `.sram` reference (delta distribution + corrupt regions).
- `scroll-ab-diff.py` — image differ: our decoded PNG vs an openMSX raw screenshot, aligned
  crop, side-by-side composite + similarity metric.
- `split-screen-ab.py` — region-structural split-frame comparator (palette-independent shift
  chain / region-break analysis).
- `sprite-attribute-census.py` — reads `--snapshot` dumps, locates the player sprite in the
  live SAT, and emits per-frame presence + oracle-box CSV.
- `sprite-oracle.py` — per-frame sprite-presence / flicker oracle over frame-PNG sequences
  (presence counts, absent runs, transitions).
- `trace-diff.py` — deterministic per-instruction A-vs-B architectural trace diff used by all
  the openMSX parity harnesses.
- `typed-audio-ab.py` — spectral / RMS audio A/B comparator (dominant-peak cents, band cosine,
  RMS-envelope similarity).
- `video-ab.py` — geometry-aware video A/B differ covering the four openMSX raw-screenshot
  geometry classes (text240 / 256×192 / 256×212 / 512-wide).
- `voice-ab.py` — digitized-voice A/B analyzer (per-block hf-RMS recovery ratio).
- `zexall-report.py` — parses `sony_msx_headless --cpm-run` captured ZEXALL/ZEXDOC output into
  a per-group PASS/FAIL table.

## capture/ — evidence-capture & scenario harnesses

- `av-scenarios-run.ps1` — manifest-driven dual-emulator A/B capture/compare harness (video →
  PNG diff, audio → WAV diff) over `scenarios.json`.
- `cartridge-evidence.ps1` — the recorded cartridge gameplay-evidence capture recipe
  (deterministic cold boot + scripted input → frame dump → PNG).
- `disk-scenario.sh` — types an identical key sequence at the Disk-BASIC `Ok` prompt on both
  engines, A/B-diffs the settled screen, optionally re-reads the written file from both `.dsk`s.
- `frames.ps1` — deterministic per-frame capture wrapper: N consecutive gameplay frames as
  decoded dumps + PNGs (one prefix-identical cold-boot run per frame index).
- `play-evidence.ps1` — the recorded gameplay-evidence capture recipe (deterministic cold boot
  + scripted input, four evidence frames).
- `playtest.ps1` — playtest frame-capture wrapper around `--debug-session`, `--input-script`,
  and `--dump-frame`, converting every dump to PNG.
- `scenarios.json` — the scenario manifest consumed by `av-scenarios-run.ps1` /
  `sprite-scenarios-run.ps1`.
- `screen-mode-sprite-run.ps1` — the 13-scenario screen-mode + sprite A/B runner over
  `gen/video-cart.py` cartridges with per-scenario geometry.
- `scroll-scenarios-run.ps1` — V9958 horizontal-scroll A/B harness driver over the
  `gen/vdp-scroll-cart.py` scenario cartridges; writes a markdown A/B table.
- `split-screen-sweep.sh` — replays a title input-script to a list of frame targets, dumping +
  converting each frame to PNG.
- `sprite-scenarios-run.ps1` — variant of `av-scenarios-run.ps1` with a parameterized headless
  exe path (for running against a secondary build).
- `sprite-scroll-scenarios-run.ps1` — variant of `scroll-scenarios-run.ps1` with parameterized
  exe/cart paths.
- `typed-audio-ab-run.ps1` — typed-at-`Ok` audio A/B runner (PSG + FM/OPLL): identical typed
  BASIC on both engines, fixed steady-state capture windows, spectral compare.

## input-scripts/ — deterministic input-event data

Data files in the `HBF1XV-INPUT-SCRIPT v1` format (`T=<tstate> KEY=<name> DOWN|UP`), consumed
via `--input-script` by the capture recipes above.

- `blanking-transition.script` — timed keypresses driving the display-blanking transition
  scenario (`openmsx/blanking-ab.ps1` mirrors it frame-indexed).
- `cartridge-evidence.script` — timed keypresses for `capture/cartridge-evidence.ps1`.
- `play-evidence.script` — timed keypresses for `capture/play-evidence.ps1`.
- `split-screen-title.script` — timed keypresses advancing a title into its split-screen scene.
- `sprite-scroll-title.script` — a timed keypress advancing a title past its title screen for
  the sprite/scroll capture.

## Naming convention

Directory = category (`gates`, `openmsx`, `gen`, `convert`, `analyze`, `capture`,
`input-scripts`); filename = what the script does. Filenames carry **no milestone numbers and no
game titles** — historical context (which milestone introduced a tool, which title a scenario
was validated on) lives in the reports under `docs/`, not in the filesystem.

## Renamed in v1.7.0

The flat, milestone-numbered layout was reorganised into the categories above. Historical
reports under `docs/` and `agent-protocol/` still cite the old names; this table maps them.

| Old name | New path |
| --- | --- |
| `validate-assets.ps1` | `gates/validate-assets.ps1` |
| `checksum-assets.ps1` | `gates/checksum-assets.ps1` |
| `audit-case-sensitivity.py` | `gates/audit-case-sensitivity.py` |
| `openmsx-ab-smoke.ps1` | `openmsx/ab-smoke.ps1` |
| `openmsx-cpu-parity.ps1` | `openmsx/cpu-parity.ps1` |
| `openmsx-fdc-read-timing-ab.ps1` | `openmsx/fdc-read-timing-ab.ps1` |
| `openmsx-io-parity.ps1` | `openmsx/io-parity.ps1` |
| `openmsx-m16-boot-parity.ps1` | `openmsx/boot-parity.ps1` |
| `openmsx-m19-cartridge-parity.ps1` | `openmsx/cartridge-parity.ps1` |
| `openmsx-m20-halnote-parity.ps1` | `openmsx/halnote-parity.ps1` |
| `openmsx-m21-vdp-render-parity.ps1` | `openmsx/vdp-render-parity.ps1` |
| `openmsx-m22-sprite-cmd-parity.ps1` | `openmsx/sprite-command-parity.ps1` |
| `openmsx-m23-halt-r-parity.ps1` | `openmsx/halt-refresh-parity.ps1` |
| `openmsx-m24-zexall-parity.ps1` | `openmsx/zexall-parity.ps1` |
| `openmsx-m25-rensha-parity.ps1` | `openmsx/rensha-turbo-parity.ps1` |
| `openmsx-m28-c5-boot-parity.ps1` | `openmsx/disk-boot-parity.ps1` |
| `openmsx-m29-scc-parity.ps1` | `openmsx/scc-parity.ps1` |
| `openmsx-m30-identification-ab.ps1` | `openmsx/cartridge-id-ab.ps1` |
| `openmsx-m32-split-ab.ps1` | `openmsx/split-screen-ab.ps1` |
| (renamed for privacy) | `openmsx/fm-audio-ab.ps1` |
| `openmsx-m34-mg-bl-ab.ps1` | `openmsx/blanking-ab.ps1` |
| `openmsx-mem-parity.ps1` | `openmsx/memory-parity.ps1` |
| `openmsx-peripheral-io-parity.ps1` | `openmsx/peripheral-io-parity.ps1` |
| `openmsx-trace-parity.ps1` | `openmsx/trace-parity.ps1` |
| `openmsx-vdp-irq-parity.ps1` | `openmsx/vdp-irq-parity.ps1` |
| `openmsx-vdp-parity.ps1` | `openmsx/vdp-parity.ps1` |
| `openmsx-ym2413-parity.ps1` | `openmsx/ym2413-parity.ps1` |
| `m51-openmsx-frames.ps1` | `openmsx/capture-frames.ps1` |
| `gen-m16-boot-disk.py` | `gen/boot-disk.py` |
| `gen-m16-fdc-probe.py` | `gen/fdc-probe.py` |
| `gen-m17-ym2413-probe.py` | `gen/ym2413-probe.py` |
| `gen-m18-peripheral-io-probe.py` | `gen/peripheral-io-probe.py` |
| `gen-m19-cartridge-probe.py` | `gen/cartridge-probe.py` |
| `gen-m20-halnote-probe.py` | `gen/halnote-probe.py` |
| `gen-m21-vdp-render-probe.py` | `gen/vdp-render-probe.py` |
| `gen-m22-sprite-cmd-probe.py` | `gen/sprite-command-probe.py` |
| `gen-m29-scc-probe.py` | `gen/scc-probe.py` |
| `gen-m32-split-probe.py` | `gen/split-screen-probe.py` |
| `gen-m38-vdp-scroll-cart.py` | `gen/vdp-scroll-cart.py` |
| `gen-m41-video-cart.py` | `gen/video-cart.py` |
| `gen-typed-ab.py` | `gen/typed-basic-program.py` |
| `msx-basic-to-disk.py` | `gen/basic-to-disk.py` |
| `format-blank-disk.ps1` | `gen/format-blank-disk.ps1` |
| `make-app-icon.py` | `gen/app-icon.py` |
| `frame-to-png.py` | `convert/frame-to-png.py` |
| `frame-row-ascii.py` | `convert/frame-to-ascii.py` |
| `frame-zoom-crop.py` | `convert/frame-zoom-crop.py` |
| `mem-to-png.py` | `convert/mem-to-png.py` |
| `mem-to-audio.py` | `convert/mem-to-audio.py` |
| `audio-dump-to-wav.py` | `convert/audio-dump-to-wav.py` |
| `omr-to-input-script.py` | `convert/omr-to-input-script.py` |
| `trace-diff.py` | `analyze/trace-diff.py` |
| `m32-split-ab-compare.py` | `analyze/split-screen-ab.py` |
| `m34-audio-ab-analyze.py` | `analyze/audio-ab.py` |
| `m38-ab-diff.py` | `analyze/scroll-ab-diff.py` |
| `m39-voice-ab-analyze.py` | `analyze/voice-ab.py` |
| `m41-audio-ab-analyze.py` | `analyze/typed-audio-ab.py` |
| `m41-video-ab.py` | `analyze/video-ab.py` |
| `m47-save-corruption-diff.py` | `analyze/save-corruption-diff.py` |
| `m51-sat-census.py` | `analyze/sprite-attribute-census.py` |
| `m51-sprite-oracle.py` | `analyze/sprite-oracle.py` |
| `measure-envelope-freq.py` | `analyze/envelope-frequency.py` |
| `zexall-report.py` | `analyze/zexall-report.py` |
| (renamed for privacy) | `capture/play-evidence.ps1` |
| (renamed for privacy) | `capture/cartridge-evidence.ps1` |
| `playtest-capture.ps1` | `capture/playtest.ps1` |
| `m51-capture-frames.ps1` | `capture/frames.ps1` |
| `m38-run.ps1` | `capture/scroll-scenarios-run.ps1` |
| `m41-run.ps1` | `capture/av-scenarios-run.ps1` |
| `m41-typed-audio-ab.ps1` | `capture/typed-audio-ab-run.ps1` |
| `m51-ab-run.ps1` | `capture/sprite-scenarios-run.ps1` |
| `m51-eg3-run.ps1` | `capture/screen-mode-sprite-run.ps1` |
| `m51-m38-run.ps1` | `capture/sprite-scroll-scenarios-run.ps1` |
| (renamed for privacy) | `capture/split-screen-sweep.sh` |
| `m41-disk-scenario.sh` | `capture/disk-scenario.sh` |
| `m41-scenarios.json` | `capture/scenarios.json` |
| (renamed for privacy) | `input-scripts/play-evidence.script` |
| (renamed for privacy) | `input-scripts/cartridge-evidence.script` |
| (renamed for privacy) | `input-scripts/blanking-transition.script` |
| (renamed for privacy) | `input-scripts/sprite-scroll-title.script` |
| (renamed for privacy) | `input-scripts/split-screen-title.script` |
| `bootstrap-build.ps1` | *(deleted — superseded by the published `setup/build.ps1`)* |
