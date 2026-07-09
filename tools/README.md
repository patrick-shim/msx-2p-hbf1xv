# tools/ — automation index

PowerShell (`*.ps1`) and Python (`*.py`) helpers used by agents and developers. Prefer
these over ad-hoc command chains; any script-assisted result should cite the exact script
path used. All scripts assume the repository root as working directory unless a parameter
says otherwise; every converter/generator is deterministic (identical input → byte-identical
output).

## Build / bootstrap

- `bootstrap-build.ps1` — **THE single way to build and test** (M33 single-build policy,
  DEC-0041). Builds SDL3 once from `references/sdl3/` into `build/_sdl3_install` if missing,
  configures the ONE canonical `build/` tree (`-DSONY_MSX_ENABLE_SDL3=ON` superset), builds,
  and with `-RunTests` runs ctest. Wiping `build/` is always safe — this restores it.
  `powershell -ExecutionPolicy Bypass -File tools/bootstrap-build.ps1 -RunTests [-Config Release]`

## Evidence gates (run at every milestone)

- `validate-assets.ps1` — asserts all seven required Sony HB-F1XV BIOS ROMs exist in `bios/`
  and ≥1 `.rom` in `roms/`. Exit 0 = OK. `./tools/validate-assets.ps1 [-Json]`
- `checksum-assets.ps1` — reproducible SHA256 checksums of `bios/*.rom` + `roms/*.rom`.
  `./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt [-Json]`

## openMSX A/B harnesses (WSL `/usr/bin/openmsx`; write `docs/*` evidence)

General:

- `openmsx-ab-smoke.ps1` — verifies openMSX availability on WSL and writes the smoke
  evidence document `docs/openmsx-ab-smoke.md`.

Per-milestone parity harnesses (each drives our emulator and openMSX over the same
deterministic probe and diffs the results into `docs/m<N>-parity-trace-diff.md` or a named
output doc):

- `openmsx-trace-parity.ps1` — M10 per-instruction opcode-trace parity (with `trace-diff.py`).
- `openmsx-io-parity.ps1` — M11 I/O-bus probe parity (`tests/parity/m11_bus_probe.bin`).
- `openmsx-cpu-parity.ps1` — M12 Z80 CPU checkpoint parity.
- `openmsx-mem-parity.ps1` — M13 memory/mapper probe parity.
- `openmsx-vdp-parity.ps1` — M14 VDP register/status probe parity.
- `openmsx-m16-boot-parity.ps1` — M16 FDC/disk-boot register-sequence parity.
- `openmsx-ym2413-parity.ps1` — M17 YM2413 (OPLL) register-write parity.
- `openmsx-peripheral-io-parity.ps1` — M18 Kanji-font/printer/cassette I/O parity.
- `openmsx-m19-cartridge-parity.ps1` — M19 cartridge-mapper probe parity.
- `openmsx-m20-halnote-parity.ps1` — M20 Halnote firmware-mapper parity.
- `openmsx-m21-vdp-render-parity.ps1` — M21 VDP rendering-path parity.
- `openmsx-m22-sprite-cmd-parity.ps1` — M22 sprite + VDP-command-engine parity.
- `openmsx-m23-halt-r-parity.ps1` — M23 HALT R-register timing parity.
- `openmsx-m24-zexall-parity.ps1` — M24 bounded-prefix ZEXALL/ZEXDOC trace parity.
- `openmsx-m25-rensha-parity.ps1` — M25 Ren-Sha Turbo autofire parity (live Sony_HB-F1XV).
- `openmsx-m28-c5-boot-parity.ps1` — M28/C5 disk-auto-boot investigation trace.
- `openmsx-m29-scc-parity.ps1` — M29 KonamiSCC banking + SCC register parity.
- `openmsx-m30-identification-ab.ps1` — M30 no-forcing mapper auto-identification agreement
  (writes `docs/m30-identification-ab.md`).
- `openmsx-m32-split-ab.ps1` — M32 split-screen (line-interrupt) frame A/B (with
  `gen-m32-split-probe.py` + `m32-split-ab-compare.py`).

## Probe / fixture generators (regenerate `tests/parity/` fixtures byte-identically)

- `gen-m16-boot-disk.py` — 720 KB 2DD synthetic boot disk (`m16_boot.dsk`).
- `gen-m16-fdc-probe.py` — WD2793 restore/read-sector Z80 probe.
- `gen-m17-ym2413-probe.py` — YM2413 two-port register-write probe.
- `gen-m18-peripheral-io-probe.py` — combined Kanji/printer/cassette probe.
- `gen-m19-cartridge-probe.py` — synthetic Generic8kB cartridge + probe.
- `gen-m20-halnote-probe.py` — synthetic 1 MB Halnote mapper image.
- `gen-m21-vdp-render-probe.py` — four VDP render-mode probes (palette/G7/YJK/planar).
- `gen-m22-sprite-cmd-probe.py` — sprite + command-engine probes.
- `gen-m29-scc-probe.py` — synthetic KonamiSCC cartridge + SCC probe.
- `gen-m32-split-probe.py` — openMSX-side split-screen Z80 program.

## Comparators / report parsers

- `trace-diff.py` — deterministic per-instruction A-vs-B trace diff (used by the parity
  harnesses).
- `m32-split-ab-compare.py` — region-structural split-frame comparator (our frame dump vs
  an openMSX raw screenshot; no byte-level color compare by design).
- `zexall-report.py` — parses `sony_msx_headless --cpm-run` captured output into the
  ZEXALL/ZEXDOC PASS/FAIL group report.

## Capture / convert (debug artifacts)

- `frame-to-png.py` — decoded frame dump (`HBF1XV-FRAME-DUMP v1`, from `--dump-frame` /
  `write_frame_dump()`) → real truecolor PNG. Deterministic encoder.
- `audio-dump-to-wav.py` — decoded PSG audio dump (`HBF1XV-AUDIO-DUMP v1`) → playable
  canonical PCM WAV.
- `mem-to-png.py` — raw memory buffer or a `--region DRAM|SRAM|VRAM` slice of a full-state
  dump → INERT grayscale PNG (raw byte view, no VDP rendering). Has `--self-check`.
- `mem-to-audio.py` — same inputs → INERT raw-PCM WAV (no PSG/OPLL synthesis). Has
  `--self-check`.

## Recorded evidence recipes

- `capture-metalgear-evidence.ps1` (+ data file `metalgear-evidence-input.script`) — the
  permanent, recorded recipe that regenerates the committed Metal Gear gameplay evidence
  `debug/frames/m31-rc-metalgear-f1100.png` (M32 closure supersession). Runs the capture
  twice and asserts byte-identical frame dumps before converting.

## Conventions

- Lowercase kebab-case names; scripts idempotent where practical.
- Generators/converters are stdlib-only Python and hand-assemble their output formats so
  results are byte-identical across platforms.
- Harness outputs under `docs/` are milestone evidence — do not regenerate them casually;
  historical `docs/m<N>-parity-trace-diff.md` files are frozen records.
