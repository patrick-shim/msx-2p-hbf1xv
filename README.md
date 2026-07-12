# sony-msx-hbf1xv

Production-oriented **Sony HB-F1XV (MSX2+, 1988)** emulator in C++: a cycle-aware,
deterministic core (Z80A @ 3.58 MHz, Yamaha V9958 VDP with 128 KB VRAM, 64 KB RAM, PSG,
Konami SCC, YM2413 FM / MSX-MUSIC, RTC, WD2793-family FDC with a 720 KB 3.5" floppy, and the
full slot/mapper fabric) plus an optional SDL3 desktop frontend.

Current release: **v1.1.0**.

## Architecture

![Sony HB-F1XV MSX2+ emulator architecture — Z80A CPU and interrupt controller, Yamaha V9958 VDP and frame buffer, PSG / Konami SCC / YM2413 (OPLL) audio, WD2793 floppy controller, input devices, and the memory-mapper / slot fabric](assets/msx2p_system_architecture.png)

A system-level overview of the emulated machine: the Z80A CPU and interrupt controller, the
Yamaha V9958 VDP and frame buffer, the PSG / Konami SCC / YM2413 (OPLL) audio path, the WD2793
floppy controller, input devices, and the memory-mapper / slot fabric. (Illustrative overview;
the sections below and the source are the authoritative spec.)

## What works today

- Real Sony BIOS cold boot to the MSX2+ logo and BASIC, with a launch summary of the loaded
  configuration and the in-window hotkeys.
- MSX-DOS / Disk BASIC boot from `.dsk` images, multi-disk hot-swap (F11), and `--disk-writable`
  game saves persisted back to the host `.dsk`.
- Cartridge loading with automatic mapper identification (software-database SHA-1 match, then a
  bank-write heuristic), plus an FM-PAC peripheral cartridge: its OPLL mixed into the audio, the
  `CALL FMPAC` backup-manager screen, and 8 KB battery SRAM saved in the openMSX-compatible
  format (existing saves migrated losslessly).
- All V9958 screen and graphic modes (text, GRAPHIC 1–7, YJK/YAE), sprites, and the command
  engine with per-line raster rendering.
- Live audio: PSG (YM2149), Konami SCC, and built-in MSX-MUSIC (YM2413) FM.
- WD2793 FDC with index-pulse-relative read-sector rotational latency; `--fast-disk` for
  near-instant loads.
- Keyboard / joystick, Ren-Sha Turbo, the hardware PAUSE button, and the Speed Controller.
- An SDL3 window that resizes and scales (`--scale`, `--filter`, `--fullscreen`, Alt+Enter),
  a `--capture`-gated F10 live capture hotkey, and opt-in `--ram` sizing (64/128/256/512 KB).
- Passes the ZEXALL / ZEXDOC Z80 instruction exercisers.

## Build and test

There is one build tree, `build/`. Bootstrap it:

```powershell
powershell -ExecutionPolicy Bypass -File tools/bootstrap-build.ps1 -RunTests
```

This builds SDL3 once from the vendored SDL3 source into `build/_sdl3_install` (only if it is
missing), configures `build/` with `-DSONY_MSX_ENABLE_SDL3=ON` (the superset: both
executables plus all tests), builds Debug, and runs `ctest`.

Manual equivalent:

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON "-DCMAKE_PREFIX_PATH=build/_sdl3_install"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- Headless-only fallback (no `SDL3Config.cmake` available): reconfigure the same tree with
  `-DSONY_MSX_ENABLE_SDL3=OFF`.
- Fast subset: `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` excludes the
  ~30-minute ZEXALL / ZEXDOC sweep.
- Executables land in `build/Debug/`: `sony_msx_headless.exe`, `sony_msx_sdl3.exe`.

Requirements: CMake, a C++20-capable MSVC toolchain (Visual Studio 2022+ with the "Desktop
development with C++" workload), and PowerShell. No separate SDL3 install is needed — it is
built from the bundled source.

## Run

Run from the repository root (asset paths and the default software-database path are
resolved relative to the current directory).

**SDL3 frontend** (real window, throttled real-time loop, live audio/input):

```powershell
build\Debug\sony_msx_sdl3.exe                                  # plain BIOS boot to BASIC
build\Debug\sony_msx_sdl3.exe --cart1 roms\aleste.rom          # cartridge, mapper auto-identified
build\Debug\sony_msx_sdl3.exe --disk disks\msxdos22.dsk        # MSX-DOS boot floppy
```

Flags:
`--bios-dir <path>` (default `bios`), `--disk <path>` (repeatable — an ordered list, the
first disk inserted at boot, F11 cycles drive A through the rest at runtime), `--cart1 <path>`,
`--cart1-type <name>|auto`, `--cart2 <path>`, `--cart2-type <name>|auto`, `--softwaredb <path>`,
`--max-frames <N>`, `--hidden-window`, `--border` / `--no-border` (the framed openMSX-matching
canvas vs the default bare edge-to-edge Sony-original presentation), `--fast-disk` (opt-in FDC
turbo for near-instant disk loads; Alt+D toggles it live), `--disk-writable` (persist disk writes
back to the host `.dsk`), `--dump-state <name>`, `--trace-cpu <name>`, `--event-log <name>`,
`--input-script <path>`, `--snapshot <dir>`, `--fmpac-sram <path>` / `--no-fmpac-sram`
(override / opt out of the FM-PAC battery-SRAM auto-persistence, which otherwise saves to
`<cart-rom-path>.sram`), `--speed <0..7>` (initial Speed Controller level — a CPU slow-down
duty cycle, not a turbo; 0 = full speed, the default; F6/F7 still step it at runtime),
`--scale <1..8>` (initial window size `320N x 240N`, default `3` = 960×720; the window is
resizable and the picture stays aspect-correct letterboxed at any size),
`--filter <nearest|linear>` (texture scaling filter, default `linear`), `--fullscreen`
(Alt+Enter toggles at runtime), `--capture <on|off>` (default `off`; gates the F10 live
capture hotkey so a mis-struck F10 is inert during play — F11 disk-swap and F12 snapshot are
unaffected), `--stream-light`,
`--ram <64|128|256|512>` (main-RAM size in KB; default `64` = the stock HB-F1XV spec,
byte-identical to omitting it — `128`/`256`/`512` are opt-in **non-stock** "fully-populated
S1985" mods for larger games, `512` KB being the internal ceiling of the S1985 5-bit mapper
read-back; beyond 512 KB needs an external RAM-expansion cartridge),
`--help`. A `--cartN-type` of `auto` (or omitted) triggers
auto-identification; an explicit type is honored byte-for-byte.

**Headless** (`sony_msx_headless.exe`) is mode-driven; the main mode is:

```powershell
build\Debug\sony_msx_headless.exe --debug-session bios 0 --disk disks\msxdos22.dsk `
    --frames 1000 --dump-frame boot.frame --dump-state state.txt --event-log run.log
```

`--debug-session <bios_dir> <max_steps>` accepts `--disk`, `--cart1/--cart1-type`,
`--cart2/--cart2-type`, `--softwaredb`, `--debug-root`, `--dump-state`, `--trace-cpu`,
`--event-log`, `--input-script`, `--frames <N>`, `--dump-frame <name>`, `--disk-writable`,
`--fast-disk`, `--swap-disk-frame <N>`, `--fmpac-sram <path>`, `--snapshot <dir>` / `--snapshot-frame <N>`,
and `--stream-light`. The plain (no-subcommand) boot mode additionally accepts
`--speed <0..7>` and `--ram <64|128|256|512>` (default `64`; the same stock/non-stock
policy as the SDL3 frontend). Other single-purpose modes each print their own usage.

## Repository layout

- `src/` — emulator source (`core`, `devices`, `peripherals`, `machine`, `frontend`).
- `bios/`, `roms/`, `disks/` — local, legally-sourced development assets (see below).
- `build/` — the CMake build tree (gitignored; recreate any time with
  `tools/bootstrap-build.ps1`).

## Assets (BIOS / ROM / disk policy)

`bios/` (the seven Sony HB-F1XV system ROMs), `roms/` (cartridge images), and `disks/`
(MSX-DOS system disks plus `disks/games/` floppy sets) are local, legally-sourced development
assets. **They remain third-party intellectual property; this project asserts no
redistribution rights and makes no provenance claim.** The repository is hosted publicly with
`bios/` included as the owner's informed, accepted-risk decision (`roms/` and `disks/`
binaries are untracked). Validate the required assets with:

```powershell
./tools/validate-assets.ps1
```

## License

The emulator source is provided for personal, non-commercial reference and educational study
(see the notice at the top of each source file). Proprietary BIOS / ROM / disk assets are the
property of their respective rights holders and are **not** licensed by this project.
