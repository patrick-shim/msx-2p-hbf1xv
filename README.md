# sony-msx-hbf1xv

Production-oriented **Sony HB-F1XV (MSX2+, 1988)** emulator in C++: a cycle-aware,
deterministic core (Z80A @ 3.58 MHz, Yamaha V9958 VDP with 128 KB VRAM, 64 KB RAM, PSG,
Konami SCC, YM2413 FM / MSX-MUSIC, RTC, WD2793-family FDC with a 720 KB 3.5" floppy, and the
full slot/mapper fabric) plus an optional SDL3 desktop frontend.

Current release: **v1.2.2** — SDL3 quality-of-life: a **master volume** control (`--volume <0..100>`,
a `<volume>` knob in the XML config, and live Alt+D / Alt+U steps — applied strictly after the
machine mix, byte-identical at full volume), **disk-save write-back ON by default** (a real MSX
writes its floppies; `--no-disk-writable` or the new Alt+S live toggle keep disks read-only), and
the fast-disk hotkey moved to **Alt+F**. On top of v1.2.1's critical **V9958 sprite-visibility
fix**: rows redrawn by the VDP command engine (blits) are now sprite-paced before they are sealed
into the frame, so sprites no longer vanish or flicker on games that rebuild their scrolling
terrain with command blits (Aleste 2's plane, Firebird's player and flying enemies, Laydock 2 — a
v1.1.6 per-line-sprite-rendering regression, root-caused by bisect and verified frame-for-frame
against openMSX at a 0-flicker match).
On top of v1.2.0's optional **strict-XML external configuration** (`sony_msx_hbf1xv.xml`: every launch
knob in an annotated file at the repo root, resolved **CLI > XML > built-in default**, hardware-timing
constants deliberately not configurable), v1.1.8's **MSX-logo Windows app icon**, v1.1.7's optional
**phosphor-persistence** flicker-softener (`--persistence`, Alt+B/Alt+M, default off), v1.1.6
**per-line-live V9958 sprite rendering** (split-screen HUD titles like Space Manbow / Laydock 2 —
backlog D9), the v1.1.5 command-engine access-slot contention model, and the v1.1.4 Z80A/V9958/PSG
Sony-hardware timing parity; the FM-PAC peripheral firmware (`roms/fmpac.rom`) is bundled.

## Architecture

![Sony HB-F1XV MSX2+ emulator architecture — Z80A CPU and interrupt controller, Yamaha V9958 VDP and frame buffer, PSG / Konami SCC / YM2413 (OPLL) audio, WD2793 floppy controller, input devices, and the memory-mapper / slot fabric](assets/msx2p_system_architecture.png)

A system-level overview of the emulated machine: the Z80A CPU and interrupt controller, the
Yamaha V9958 VDP and frame buffer, the PSG / Konami SCC / YM2413 (OPLL) audio path, the WD2793
floppy controller, input devices, and the memory-mapper / slot fabric. (Illustrative overview;
the sections below and the source are the authoritative spec.)

## What works today

- Real Sony BIOS cold boot to the MSX2+ logo and BASIC, with a detailed launch summary of the
  loaded configuration (RAM, VRAM, slots, FM-PAC/SRAM status, disk, video) and the in-window hotkeys.
- Convenience-first launch defaults (v1.1.2): **512 KB RAM, fast-disk ON, and the FM-PAC
  peripheral auto-loaded into slot 2** so its battery SRAM saves are always available (a game
  cartridge in slot 1 coexists) — with `--stock` to restore the authentic bare HB-F1XV in one flag.
- MSX-DOS / Disk BASIC boot from `.dsk` images, multi-disk hot-swap (F11), and disk-save
  persistence back to the host `.dsk` — **ON by default** as of v1.2.2 (a real MSX writes its
  floppies); `--no-disk-writable` keeps disks read-only, and Alt+S toggles it live.
- Cartridge loading with automatic mapper identification (software-database SHA-1 match, then a
  bank-write heuristic), plus an FM-PAC peripheral cartridge: its OPLL mixed into the audio, the
  `CALL FMPAC` backup-manager screen, and 8 KB battery SRAM saved in the openMSX-compatible
  format (existing saves migrated losslessly).
- All V9958 screen and graphic modes (text, GRAPHIC 1–7, YJK/YAE), sprites, and the command
  engine with per-line raster rendering and hardware-timed command duration (the `CE` busy-wait
  window paces software that polls it, so command-driven cut-scenes run at the correct speed).
- Live audio: PSG (YM2149), Konami SCC, and built-in MSX-MUSIC (YM2413) FM, with a
  **master volume** control (`--volume <0..100>`, default 100; Alt+D / Alt+U step it -/+10% live)
  applied strictly after the machine mix — SDL3-presentation only, byte-identical at full volume.
- WD2793 FDC with index-pulse-relative rotational latency on read and write, and a cycle-accurate
  write byte-stream: the sector data-position is decoupled from CPU write timing (a missed slot
  substitutes `0x00`+Lost-Data and advances, never drops), so in-game disk saves are byte-exact
  under any write cadence and land on the latched track/side; `--fast-disk` for near-instant loads.
- Keyboard / joystick, Ren-Sha Turbo, the hardware PAUSE button, and the Speed Controller.
- An SDL3 window that resizes and scales (`--scale`, `--filter`, `--fullscreen`, Alt+Enter),
  a `--capture`-gated F10 live capture hotkey, and opt-in `--ram` sizing (64/128/256/512 KB).
- An **in-window menu bar** (Dear ImGui) — File / Machine / Video / Audio / Disk / Help —
  exposing the existing runtime controls (pause, speed, ren-sha, fullscreen, scale, filter,
  persistence, volume, mute, fast-disk, disk-writable, swap disk, exit) with live checkmarks and
  the hotkey labels; genuinely new-capability items (open cartridge/disk, eject, reset, new blank
  disk) are shown grayed for now. The menu is mouse-operated and appears only in an interactive
  window — never under `--hidden-window` / headless, so it never affects determinism or tests.
- Passes the ZEXALL / ZEXDOC Z80 instruction exercisers.
- A standalone **`msx-disk` disk utility** (`diskutils\msx-disk.exe`): create / hex-read / format
  720 KB MSX-DOS FAT12 `.dsk` images byte-exact to the machine's own layout (see
  [Disk utility](#disk-utility-msx-disk) below).

## Build and test

One codebase, two toolchains — **Windows (MSVC) and macOS (AppleClang)** build from the same
`CMakeLists.txt`, selected automatically at configure time. There is no fork and no
per-platform build file. There is also one build tree, `build/`, on both.

The one difference that matters: Visual Studio is a **multi-config** generator (pick the config
at build/test time with `--config` / `-C`), while Ninja is **single-config** (pick it at
*configure* time with `CMAKE_BUILD_TYPE`, then pass no `--config` / `-C` at all).

### Windows

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

- Executables land in `build/Debug/`: `sony_msx_headless.exe`, `sony_msx_sdl3.exe`.
- Fast subset: `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` excludes the
  ~30-minute ZEXALL / ZEXDOC sweep.

Requirements: CMake, a C++20-capable MSVC toolchain (Visual Studio 2022+ with the "Desktop
development with C++" workload), and PowerShell. No separate SDL3 install is needed — it is
built from the bundled source.

### macOS

```bash
tools/bootstrap-build.sh --run-tests
```

Manual equivalent — note **no `--config` / `-C Debug`** (Ninja is single-config):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

- Executables land in **`build/`, not `build/Debug/`**, with no `.exe` suffix:
  `sony_msx_headless`, `sony_msx_sdl3`, `msx-disk`. This is the most common trip hazard.
- Fast subset: `ctest --test-dir build -LE m24_slow_full_sweep`.

Requirements: the Xcode Command Line Tools (`xcode-select --install`) for AppleClang, plus
`brew install ninja cmake sdl3`. Unlike the Windows path, macOS links **Homebrew's** SDL3 —
`find_package(SDL3 CONFIG REQUIRED)` locates it with no `CMAKE_PREFIX_PATH` argument — this is
the tested macOS route. To build SDL3 from the vendored source instead (on a machine with no
Homebrew SDL3), `tools/bootstrap-build.sh --vendored-sdl3` mirrors what the Windows bootstrap
does, though that fallback branch has not yet been exercised on macOS. `brew install powershell`
is optional and only needed to run the `tools/*.ps1` asset gates; the test suite itself is pure
C++ and needs no PowerShell.

### Both platforms

- Headless-only fallback (no `SDL3Config.cmake` available): reconfigure the same tree with
  `-DSONY_MSX_ENABLE_SDL3=OFF`. This **changes the test count** — 16 tests are SDL3-gated, so
  the fast subset reports **237** instead of **253**. That is the fallback configuration, not a
  regression.
- Some tests report `SKIP` and still pass when an optional game asset is absent; the count stays
  green.

## Disk utility: msx-disk

The build also produces a standalone host-side disk tool, post-build-copied to
`diskutils\msx-disk.exe` (`diskutils/msx-disk`, no suffix, on macOS; source in `src/diskutils/`,
fully build-isolated from the emulator —
neither links the other). It creates, inspects, and formats 720 KB 3.5" DD MSX-DOS FAT12
`.dsk` images (80 tracks x 2 sides x 9 sectors x 512 bytes) byte-exact to the layout the
HB-F1XV's WD2793 / Sony Disk ROM expects:

```powershell
diskutils\msx-disk.exe --create mydisk.dsk           # new fully-formatted blank 720 KB image
diskutils\msx-disk.exe --read mydisk.dsk --sector 0  # hex dump (whole disk, --sector <N>, or --range <A-B> in hex)
diskutils\msx-disk.exe --format mydisk.dsk           # re-format in place
```

On macOS, the same three commands as `./diskutils/msx-disk --create mydisk.dsk` (etc.). The tool
is deterministic across platforms: `--create` produces a byte-identical image on Windows and
macOS (same SHA256).

Safety and determinism: `--create` refuses to overwrite an existing file and `--format`
refuses a file that is not exactly 737,280 bytes — both return exit code 3 unless `--force`
is given (exit codes: 0 success, 1 usage, 2 I/O, 3 safety refusal). Output contains no
timestamps or volume serial, so identical invocations produce byte-identical images. Created
disks are empty data/files media — mounted and recognized by Disk BASIC / MSX-DOS, but not
bootable (no proprietary DOS system files are written; copy `MSXDOS.SYS`/`COMMAND.COM` from
your own MSX-DOS disk to make one bootable).

## Run

Run from the repository root (asset paths and the default software-database path are
resolved relative to the current directory).

**SDL3 frontend** (real window, throttled real-time loop, live audio/input):

```powershell
build\Debug\sony_msx_sdl3.exe                                       # plain BIOS boot to BASIC
build\Debug\sony_msx_sdl3.exe --slot1 "games\roms\Aleste 2\aleste2.rom"   # cartridge in slot 1 (FM-PAC auto-loads into slot 2)
build\Debug\sony_msx_sdl3.exe --disk disks\msxdos23.dsk             # MSX-DOS boot floppy
build\Debug\sony_msx_sdl3.exe --disk games\disks\ys2\ys2-d1.dsk --disk games\disks\ys2\ys2-d2.dsk --disk-writable   # multi-disk game (F11 swaps; saves persist)
```

The same lines on **macOS** — the binary sits in `build/` with no `.exe`, and paths use `/`:

```bash
./build/sony_msx_sdl3                                          # plain BIOS boot to BASIC
./build/sony_msx_sdl3 --slot1 "games/roms/Aleste 2/aleste2.rom" # cartridge in slot 1
./build/sony_msx_sdl3 --disk disks/msxdos23.dsk                 # MSX-DOS boot floppy
./build/sony_msx_sdl3 --disk games/disks/ys2/ys2-d1.dsk --disk games/disks/ys2/ys2-d2.dsk --disk-writable
```

(The game library is organized one folder per title, and several titles contain spaces — quote
those paths.)

Flags:
`--bios-dir <path>` (default `bios`), `--disk <path>` (repeatable — an ordered list, the
first disk inserted at boot, F11 cycles drive A through the rest at runtime), `--slot1 <path>`,
`--slot1-type <name>|auto`, `--slot2 <path>`, `--slot2-type <name>|auto` (the two cartridge slots;
the old `--cart1`/`--cart2`[`-type`] names remain accepted as silent aliases), `--softwaredb <path>`,
`--max-frames <N>`, `--hidden-window`, `--border` / `--no-border` (the framed openMSX-matching
canvas vs the default bare edge-to-edge Sony-original presentation), `--fast-disk` / `--no-fast-disk`
(FDC turbo for near-instant disk loads — **ON by default as of v1.1.2**; `--no-fast-disk` restores
accurate rotational timing; **Alt+F** toggles it live), `--disk-writable` / `--no-disk-writable`
(persist disk writes back to the host `.dsk` — **ON by default as of v1.2.2**; `--no-disk-writable`
keeps disks read-only; Alt+S toggles it live), `--volume <0..100>` (master volume percent, default
`100` = full; attenuation only; Alt+D / Alt+U step it -/+10% live; SDL3 presentation only),
`--dump-state <name>`, `--trace-cpu <name>`, `--event-log <name>`,
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
`--ram <64|128|256|512>` (main-RAM size in KB; **default `512` as of v1.1.2** — a "fully-populated
S1985" mod for larger games, `512` KB being the internal ceiling of the S1985 5-bit mapper
read-back; `--ram 64` restores the stock HB-F1XV spec; beyond 512 KB needs an external
RAM-expansion cartridge), `--no-fmpac` (skip the default FM-PAC auto-load), `--stock` (one-shot
authentic bare machine: 64 KB + accurate disk timing + no FM-PAC; explicit per-option flags still
win, e.g. `--stock --ram 512`),
`--help`. A `--slotN-type` (or the `--cartN-type` alias) of `auto` (or omitted) triggers
auto-identification; an explicit type is honored byte-for-byte.

By default (v1.1.2) the machine boots ready-to-play: **512 KB RAM, fast-disk ON, and the FM-PAC
peripheral auto-loaded into slot 2** (from `roms/fmpac.rom`, SRAM persisted to
`roms/fmpac.rom.sram`) so its battery saves are always available; a game cartridge in slot 1
coexists with it. `--no-fmpac` skips the auto-load, an explicit `--slot2 <rom>` overrides it, a
missing `roms/fmpac.rom` is skipped gracefully, and `--stock` reverts all three defaults to the
authentic bare HB-F1XV.

### In-window menu bar

An interactive SDL3 launch shows a **Dear ImGui menu bar** across the top of the window
(**mouse-operated**; the menu is chrome only and never appears under `--hidden-window` or in the
headless build). Its layout:

- **File** — Open Cartridge ▸ Slot 1 / Slot 2 *(grayed)*, Open Disk *(grayed)*, Swap Disk
  (`F11`, enabled with more than one `--disk`), Eject *(grayed)*, Exit.
- **Machine** — Pause (`PAUSE`), Reset *(grayed)*, Speed 0–7 (`F6`/`F7`), Ren-Sha Turbo 0–100%
  (`F8`/`F9`), RAM (info only — restart to change).
- **Video** — Fullscreen (`Alt+Enter`), Scale 1×–8×, Filter Linear/Nearest, Border *(startup only
  — grayed)*, Persistence ±10% (`Alt+B` / `Shift+Alt+B`), Persistence Mode avg/peak (`Alt+M`).
- **Audio** — Volume ±10% (`Alt+D` / `Alt+U`), Mute.
- **Disk** — Fast Disk (`Alt+F`), Disk Writable (`Alt+S`), New Blank Disk *(grayed)*.
- **Help** — Hotkeys (the full in-window hotkey list, single-sourced with the launch banner),
  About.

Checkmarks reflect live state and the hotkey labels are the exact in-window hotkeys. The grayed
items are genuinely new capabilities being enabled incrementally; every wired item is identical to
its keyboard hotkey. Keyboard navigation is deliberately off, so the `Alt+`letter host hotkeys keep
working while the menu is visible. The menu bar is a **Windows/macOS interactive-only** feature.

**Headless** (`sony_msx_headless.exe`) is mode-driven; the main mode is:

```powershell
build\Debug\sony_msx_headless.exe --debug-session bios 0 --disk disks\msxdos23.dsk `
    --frames 1000 --dump-frame boot.frame --dump-state state.txt --event-log run.log
```

`--debug-session <bios_dir> <max_steps>` accepts `--disk`, `--slot1/--slot1-type`,
`--slot2/--slot2-type` (aliases `--cart1/--cart2[-type]`), `--softwaredb`, `--debug-root`,
`--dump-state`, `--trace-cpu`, `--event-log`, `--input-script`, `--frames <N>`,
`--dump-frame <name>`, `--disk-writable`, `--fast-disk` / `--no-fast-disk`, `--no-fmpac`,
`--stock`, `--swap-disk-frame <N>`, `--fmpac-sram <path>`, `--snapshot <dir>` / `--snapshot-frame <N>`,
and `--stream-light`. It shares the SDL3 frontend's v1.1.2 convenience defaults (512 KB, fast-disk,
FM-PAC into slot 2; `--stock` reverts them). Other single-purpose modes (e.g. the openMSX-parity
identification path) keep their own stock defaults and each print their own usage.

## Configuration file (optional)

Every default and knob can be externalized to a strict XML config file, so the machine is
configurable without recompiling. The annotated reference [`sony_msx_hbf1xv.xml`](sony_msx_hbf1xv.xml)
at the repository root lists **every** externalized field (RAM/VRAM, fast-disk, FM-PAC auto-load,
video scale/filter, persistence, border, disk-writable, master volume, speed, fullscreen, capture,
BIOS directory + the seven ROM filenames, FM-PAC ROM/SRAM paths, and the software-database path) set
to its exact built-in default, each commented with its type and allowed range/enum.

- **Optional.** The emulator always runs standalone with zero config; if no config file is found it
  prints one warning line and continues on the built-in defaults.
- **Precedence:** command-line flag **>** config file **>** built-in default. An explicit CLI flag
  always wins; the config file overrides the compiled default; an omitted knob keeps its default.
- **Auto-load** happens only on an interactive SDL3 launch (a real window), searching
  `<exe-dir>/sony_msx_hbf1xv.xml` then `<cwd>/sony_msx_hbf1xv.xml`. The headless executable and the
  deterministic hidden-window/test paths never auto-load. `--config <path>` force-loads in any mode.
  The shipped reference lives at the repo root and is not auto-found from `build/Debug/` — copy it
  next to the executable (or into the working directory) and edit it to activate.
- **Strict but never fatal:** each value is type- and range/enum-checked; a bad value warns naming
  the offending key and falls back to that key's default (the rest of the file still applies), never
  crashing.
- **Hardware timing is not configurable.** The Z80A clock, interrupt-acknowledge timings, V9958
  access-slot/line cycles, WD2793 FDC timing, and the strict 128 KB VRAM are the silicon spec and
  stay fixed in code, so no config edit can degrade emulation accuracy.

## Repository layout

- `src/` — emulator source (`core`, `devices`, `peripherals`, `machine`, `frontend`).
- `bios/`, `roms/`, `disks/`, `games/` — local, legally-sourced development assets (see below).
- `build/` — the CMake build tree (gitignored; recreate any time with
  `tools/bootstrap-build.ps1`).

## Assets (BIOS / ROM / disk policy)

`bios/` (the seven Sony HB-F1XV system ROMs), `roms/` (the FM-PAC peripheral ROM `fmpac.rom`
plus its battery-SRAM `fmpac.rom.sram`), `disks/` (MSX-DOS system disks), and `games/` (the
game library — `games/disks/<title>/` floppy sets and `games/roms/` cartridge images) are
local, legally-sourced development assets. **They remain third-party intellectual property;
this project asserts no redistribution rights and makes no provenance claim.** The repository
is hosted publicly with `bios/` included as the owner's informed, accepted-risk decision
(`roms/`, `disks/`, and `games/` binaries are untracked). Validate the required assets with:

```powershell
./tools/validate-assets.ps1
```

On macOS (PowerShell 7 via `brew install powershell`; drop the Windows-only
`-ExecutionPolicy Bypass`):

```bash
pwsh -File tools/validate-assets.ps1
```

## License

The emulator source is provided for personal, non-commercial reference and educational study
(see the notice at the top of each source file). Proprietary BIOS / ROM / disk assets are the
property of their respective rights holders and are **not** licensed by this project.
