# Tools Directory

Helper tools written in Python and PowerShell for build, test, data prep, and agent-assisted workflows.

## Purpose

- Keep repeatable automation in one place.
- Reduce ad-hoc command drift across developers and agents.
- Provide deterministic, scriptable project operations.

## Conventions

- Python scripts: `*.py`
- PowerShell scripts: `*.ps1`
- Prefer lowercase kebab-case names, for example `prepare-rom-index.ps1`.
- Scripts should be idempotent where practical.

## Expected Script Categories

- Build helpers (configure/build/test wrappers).
- Asset validation helpers (`bios/` and `roms/` path checks).
- Report or protocol helpers (optional).

## Included Script

- `validate-assets.ps1`: verifies required Sony HB-F1XV BIOS files and checks that at least one `.rom` exists in `roms/`.
- `checksum-assets.ps1`: generates reproducible asset checksums (`bios/*.rom`, `roms/*.rom`) for QA and protocol evidence.
- `openmsx-ab-smoke.ps1`: verifies openMSX availability on WSL and writes A/B smoke evidence to `docs/openmsx-ab-smoke.md`.
- `openmsx-trace-parity.ps1` / `trace-diff.py`: M10-S4 opcode-trace parity harness and deterministic diff.
- `mem-to-png.py`: M10-S5 deterministic memory-buffer -> grayscale PNG visualizer (INERT raw byte view; no VDP rendering).
- `mem-to-audio.py`: M10-S5 deterministic memory-buffer -> PCM WAV serializer (INERT raw byte view; no PSG/OPLL synthesis).

Usage:

```powershell
./tools/validate-assets.ps1
./tools/validate-assets.ps1 -Json
./tools/checksum-assets.ps1
./tools/checksum-assets.ps1 -Json
./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
./tools/openmsx-ab-smoke.ps1
```

## M10-S5 memory converters (`mem-to-png.py`, `mem-to-audio.py`)

Deterministic converters that turn a dumped MSX memory buffer into an inspectable
artifact. Both are INERT raw-buffer visualizers/serializers: they treat source bytes
directly (one byte -> one grayscale pixel; bytes -> raw PCM samples). Neither performs
real V9958 VDP rendering nor PSG/YM2413 audio synthesis -- those remain separate device
milestones.

Input contract (either converter):

- Raw binary buffer (default): a file of raw bytes -- a region extracted from a dump, or
  any committed memory image (e.g. `tests/parity/z80_parity_checkpoint.bin`).
- M10-S3 region dump (`--region NAME`): parses the full-state dump text emitted by
  `src/machine/debug_dump.cpp` (`write_state_dump`, tag `HBF1XV-STATE-DUMP v1`) and
  extracts a named region (`DRAM`/`SRAM`/`VRAM`). The folded canonical hex (`*` repeat
  marker, 16 bytes/line, 8-hex-digit offset) is expanded losslessly back to bytes.

Output contract:

- `mem-to-png.py` -> 8-bit grayscale PNG (color type 0), `--width` px/row (default 256),
  height `ceil(len/width)`, last row padded with `--pad` (default `00`).
- `mem-to-audio.py` -> canonical PCM WAV (44-byte header + data): mono, 8-bit unsigned,
  8000 Hz by default; `--bits 16`, `--rate`, `--channels` configurable.

Determinism: identical input -> byte-identical output on any platform. The PNG `IDAT` is
emitted as DEFLATE *stored* (uncompressed) blocks (no zlib compression-level/version
dependence); no `tIME`/`tEXt`/`pHYs` chunks. The WAV header is hand-assembled with fixed
fields and carries no timestamp/`LIST`/`fact` chunk. Both use stdlib only (`struct`,
`zlib` for CRC/Adler only, `hashlib`, `argparse`).

Self-checks (hermetic, re-runnable; assert a fixed golden SHA-256 + format invariants +
two-encode reproducibility + S3 fold round-trip):

```powershell
python tools/mem-to-png.py --self-check
python tools/mem-to-audio.py --self-check

# Convert a raw memory buffer:
python tools/mem-to-png.py tests/parity/z80_parity_checkpoint.bin -o out.png --width 16
python tools/mem-to-audio.py tests/parity/z80_parity_checkpoint.bin -o out.wav

# Convert a region out of an M10-S3 full-state dump:
python tools/mem-to-png.py debug/traces/state.txt --region VRAM -o vram.png
python tools/mem-to-audio.py debug/traces/state.txt --region DRAM -o dram.wav
```

Exit codes:

- `0`: required BIOS set present and at least one ROM available.
- `1`: one or more required BIOS files are missing, or no ROM files were found.

## Agent Usage Policy

- Agents should prefer scripts in this directory when an equivalent exists.
- Any script-assisted result should report the exact script path used.
- Avoid embedding large one-off shell logic in agent responses when a script can own it.
