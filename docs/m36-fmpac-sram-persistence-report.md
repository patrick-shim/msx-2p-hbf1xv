# M36 — SDL3 FM-PAC SRAM Persistence (completes the "FM-PAC S-RAM saves work" clause)

**Date**: 2026-07-10
**Status**: DONE — round-trip integration-tested; 208/208; zero cpu/core. Part of the v1.0.37 closure.

> Authored by the MSX Developer Agent (opus); persisted by the coordinator.

## Gap
The machine core + headless `src/main.cpp` already persisted FM-PAC battery SRAM (`--fmpac-sram <path>`, load-on-insert + flush-on-exit, integration-tested across two machine instances). The **SDL3 frontend did not** — so an in-game FM-PAC save made in `sony_msx_sdl3.exe` was in-memory-only and lost on exit. The repo owner flagged this after confirming the in-game save works.

## Fix (authentic = automatic by default; a real FM-PAC battery always persists)
`src/frontend/sdl3_{app,cli,main}.*`, reusing the existing machine seam (`set_fmpac_sram_path`/`flush_fmpac_sram` — no new machine API):
- **Auto-default**: when a loaded cartridge resolves to `FmPac`, bind the SRAM path to `<cart-ROM-path>.sram` (e.g. `roms/fmpac.rom` → `roms/fmpac.rom.sram`, next to the cart in untracked `roms/`) — set BEFORE `load_cartridge()` so the machine's load-on-insert restores it (`hbf1xv_machine.cpp:977-982`). On `Sdl3App::shutdown()` (where `--disk-writable` also flushes), `flush_fmpac_sram()` writes it back; clean no-op when no FM-PAC / no path.
- **Override**: `--fmpac-sram <path>`. **Opt-out**: `--no-fmpac-sram`. Precedence: opt-out > override > auto-default.

## Evidence
- New `tests/integration/frontend/sdl3_app_fmpac_sram_integration_test.cpp` — PASS: default path derived from the cart; **round-trip** (write pattern via unlock+bus → `shutdown()` writes the file → a fresh `Sdl3App` restores it byte-for-byte); explicit override wins; `--no-fmpac-sram` binds no path (flush no-op); no-FM-PAC clean no-op; absent `.sram` starts zeroed.
- Full suite `ctest -E hbf1xv_m24_zexall_system_test` → **208/208**; the non-FM-PAC path byte-for-byte additive (multi-disk / CLI-session regressions green). `git status --porcelain src/devices/cpu src/core` empty (ZEXALL withheld). SDL3 exe rebuilt.

## Usage
`sony_msx_sdl3.exe --cart1 roms/fmpac.rom` → make an in-game save → quit → relaunch: the save persists via `roms/fmpac.rom.sram`. No flag needed.
