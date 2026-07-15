# M36 Phase 3 — Comprehensive Live Debug Snapshot — Implementation Report

**Date**: 2026-07-10
**Milestone**: M36 Phase 3 (debug snapshot; DEC-0051)
**Plan of record**: `docs/m36-phase3-planner-package.md`
**Status**: COMPLETE + QA-ready. Uncommitted (coordinator owns closure).

> Authored by the MSX Developer Agent (opus); persisted by the coordinator (the developer was under a
> no-report-file constraint and returned the text). Coordinator independent-verification addendum in §7.

## 1. Target
DEC-0051 comprehensive, live-triggerable debug snapshot (slices S1–S5): a NEW serializer separate from
the golden-locked state-dump capturing the EXACT state of EVERY machine component to
`debug/snapshot/<id>/`, versioned `HBF1XV-SNAPSHOT v1`, restore-ready typed fields, F12 hotkey (SDL3) +
`--snapshot` CLI (both executables), 13 additive const getters, deterministic content, `.gitignore` line —
all additive, read-only w.r.t. emulation, zero `src/devices/cpu/` or `src/core/` edits.

## 2. Code changes
**New serializer (core of the feature):** `src/machine/debug_snapshot.{h,cpp}` — per-component section
functions (cpu/dram/mapper/vram/vdp/audio/rtc/fdc/s1985/slots/sysctrl/clock/pause_rensha/cartridge/
halnote/peripherals), `Snapshot`/`SnapshotFile` types, `kSnapshotFormatTag`, deterministic `frame_file()`
id, FNV-1a `checksum()`. Reuses `debug_dump::serialize_region`+`debug_format` verbatim; no wall-clock/RNG.
Registered in `CMakeLists.txt`.

**Machine (`src/machine/hbf1xv_machine.{h,cpp}`):** additive `mapper_io()`, `snapshot_id()`
(deterministic `f<frame>_c<cycle>`), `serialize_snapshot()`, `write_snapshot()` → `<debug_root>/snapshot/<id>/`.
Manifest built last (indexes + FNV-checksums each component file). Golden
`serialize_cpu`/`serialize_region`/`serialize_state_dump()` UNTOUCHED.

**13 additive const getters (§2.4), all `const`/read-only, zero logic change:** CPU (no getter needed —
WZ/Q/interrupt/ei/pending already public); V9958Vdp latches/IRQ (data_latch, status_reg0, eo_field,
irq_vertical/horizontal/level, blink_countdown, …); VdpCommandEngine FSM (status/sx/sy/dx/dy/nx/ny/asx/
arg/cmd/transfer_* as numeric codes); PSG `generator_snapshot()` (counters/LFSR/residual); YM2413
address_latch/last_write*; FM-PAC r1ffe/r1fff (unlock latch); RP5C01 decoded time; WD2793 full FSM
(plain member reads, never `sync()`); DiskDrive motor_latched/off_pending/deadline; S1985 addr/pattern/
color regs; MB670836 pause frame_index; CartridgeMapperDevice defaulted virtual `rom_window()` overridden
in the 7 window mappers (generic bank dump, no RTTI).

**Wiring (additive, default-off):** headless `--snapshot <dir>` + `--snapshot-frame <N>`; SDL3
`snapshot_dir` config + `on_snapshot_hotkey()` bound to **F12** (host hotkey like F11, never routed to the
input mapper; captured at end of `run_one_frame()` after `on_vsync_boundary()`) + a `request_snapshot()`/
`snapshot_requested()` test seam. F11 is the only pre-existing host hotkey → no F12 collision.
`.gitignore` gains `/debug/snapshot/`.

## 3. Test results
- New unit tests (`tests/unit/machine/debug_snapshot_{core,vdp,audio,devices}_unit_test.cpp`,
  `tests/unit/frontend/sdl3_cli_snapshot_unit_test.cpp`) — all PASS: CPU typed-field exactness incl. WZ/Q;
  mapper segs+readback; VDP all 12 sub-sections live-matched; audio PSG counters + YM2413 + FM-PAC.OPLL
  present only after FmPac insert; FDC/drive/disk/RTC/pause/Ren-Sha/Konami-banks; **golden guard** (the
  state-dump `[CPU]` block byte-unchanged and contains no `WZ=`); serialize-twice byte-identical +
  non-perturbing; default-off no-op (no dir when flag absent).
- **Determinism system test** `tests/system/hbf1xv_m36_snapshot_determinism_system_test.cpp` — PASS. Two
  identical runs to frame 90 → byte-identical bundle; adversarial third run (one extra CPU step) → different
  bundle+digest (non-vacuous). Digest `FNV-1a32 = 23764175`. Manual cross-run SHA-256 (frame 150,
  `diff -rq` clean): `e9e5041aeff7fcf91755153bd10d5ad16b7d3dab40c5548964abba8b224d345b`.
- **SDL3 F12 integration** `tests/integration/frontend/sdl3_app_snapshot_integration_test.cpp` — PASS.
- **Full fast-subset**: build exit 0; `ctest -E hbf1xv_m24_zexall_system_test` → **205/205, 0 failed**;
  ZEXALL #127 correctly withheld (zero cpu/core edits). Golden `machine_hbf1xv_debug_dump_unit_test` green.
- Asset gates: `tools/validate-assets.ps1` → True (7 BIOS, 4 ROM).

**AC-7 payoff (real YS II content):** snapshot captured with `disk1+disk2`, `datadisk.script`,
`--swap-disk-frame 11000`, `--snapshot-frame 12600`. Discriminating fields exposed: `disk_index=1/2`;
`[VDP.MODE] GRAPHIC4`, `R01=60` (display-enable BL bit set), palette loaded, `IRQ_ACTIVE=1`;
`[FDC.DIAG] READ_CMDS_ACCEPTED=746 BYTES_TRANSFERRED=381952 COMPLETIONS_OK=746` (reads succeeding, no
errors, PHASE=Idle); slots/mapper populated. Cleanly separates VDP-blank vs FDC-read-fail vs slot/mapper
for Bug B — no dependence on the game's own save.

**Default-off proof (AC-3/AC-5):** without `--snapshot`, no snapshot dir, identical run counters
(`steps=1430835 final_pc=2cb4 elapsed_cycles=12545287`). `git status --porcelain src/devices/cpu src/core`
= empty.

**Restore-ready (AC-4):** versioned every file + manifest; complete (all §2.4 internals; buffers whole);
typed fixed-width tokens (enums as symbol+numeric); `size=<n>` per region; per-file size+CRC integrity
anchor. No reader built (restore is a named future milestone).

## 4. Known issues / disposition
- Example snapshots are gitignored (`/debug/snapshot/`). Discoverable example on disk:
  `debug/snapshot/f000400_c0000000023895941/` (BIOS→BASIC, deterministic). If tracked evidence is wanted,
  add a `!` exception (M26/M27 precedent).
- The developer added the `r1ffe()/r1fff()` const getters to the Phase-2 file
  `src/devices/cartridge/cartridge_fmpac_rom.h` (needed for the snapshot); otherwise it did not touch the
  uncommitted Phase-1/2 work. Scope-separate at closure.
- openMSX A/B N/A by design (a snapshot is this emulator's own introspection artifact; M27/M31 precedent).

## 5. QA handoff
- Determinism: `ctest -R hbf1xv_m36_snapshot_determinism_system_test` (digest 23764175); or two
  `--debug-session bios 5000000 --frames 150 --snapshot <A/B>` runs → `diff -rq` clean, SHA-256 `e9e5041a…`.
- No-op guard: `--frames 210` with vs without `--snapshot` → identical counters, no dir when absent.
- Discipline: `git status --porcelain src/devices/cpu src/core` empty; golden `[CPU]` block unchanged; all
  §2.4 getters const with no logic change.
- Full re-verify: clean rebuild of the single `build/`, then `ctest -E hbf1xv_m24_zexall_system_test`
  (205/205). ZEXALL withheld.

## 6. Files
New: `src/machine/debug_snapshot.{h,cpp}`; `tests/unit/machine/debug_snapshot_{core,vdp,audio,devices}_unit_test.cpp`;
`tests/unit/frontend/sdl3_cli_snapshot_unit_test.cpp`; `tests/system/hbf1xv_m36_snapshot_determinism_system_test.cpp`;
`tests/integration/frontend/sdl3_app_snapshot_integration_test.cpp`. Modified (additive): `hbf1xv_machine.{h,cpp}`,
`main.cpp`, `sdl3_{app,cli,main}.*`, the §2.4 device headers, `CMakeLists.txt`, `tests/CMakeLists.txt`, `.gitignore`.

## 7. Coordinator Independent Verification Addendum (2026-07-10)
- **Scope**: `git status --porcelain src/devices/cpu src/core` = EMPTY (zero cpu/core edits) — confirmed.
- **Tests re-run**: the 7 snapshot targets re-run green from the existing `build/` (core/vdp/audio/devices
  unit, cli, determinism system test, SDL3 F12 integration) → 7/7 Passed.
- **Snapshot content vision-read**: the coordinator opened the BIOS→BASIC example bundle
  (`debug/snapshot/f000400_c0000000023895941/`) — manifest (versioned, 9 files, per-file CRC), cpu.txt
  (full Z80 incl. WZ/Q/IFF/IM/EI_DELAY/pending/T-states), vdp.txt (R00–R31 + cmd regs + S0–S9 + latch +
  VRAM ptr + 16 palette + mode + IRQ + raster + sprite + command engine), fdc.txt (WD2793 regs + read
  diagnostics + full FSM + drive + disk). Genuinely comprehensive and sane.
- **Disposition**: Phase 3 is complete, verified, and QA-ready. The snapshot tool is the enabler for the
  Bug B root-cause (capture the black-screen VDP/FDC/slot state directly). QA sign-off remains deferred to
  the single M36-closure sign-off covering Phase 1 + Phase 2 + Phase 3.
