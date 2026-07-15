# M36 Phase 2 — Implementation Report

**Date**: 2026-07-10
**Milestone**: M36 Phase 2 (YS II live-playtest bug fixes + DEC-0050 disk-write item)
**Plan of record**: `docs/m36-phase2-planner-package.md` (DEC-0050-aligned; supersedes the machine-level-SRAM framing of `docs/m36-planner-package.md`)
**Status**: 5 of 6 slices COMPLETE + QA-ready; **S-b (Bug B, the MANDATORY item) REPRODUCED-partway then ESCALATED** (navigation stall — awaiting a human YS II save-disk/route). Uncommitted (coordinator owns closure).

> This report was authored by the MSX Developer Agent (opus) and is persisted here by the
> coordinator as the required milestone artifact, followed by the coordinator's independent
> verification addendum (§8). It is the handoff for msx-qa.

## 1. Milestone Target
DEC-0050-aligned: FM-PAC peripheral cartridge (save path #2) + disk-save persistence (save path #1) + msx-playtest enabler + internal-`sram_` reconciliation + R-M35-1, plus repro-first diagnosis of Bug B (black-screen-on-building-entry).

## 2. Code Changes (per slice)

**S-a — msx-playtest enabler (DONE).** Prepended YAML frontmatter (`name/description/tools/model: opus`) to `.claude/agents/msx-playtest.md`, mirroring `msx-qa.md`. Body unchanged. (Registration is now live — the agent is spawnable.)

**S-b — Bug B (REPRO-FIRST → ESCALATED, see §5).** No fix implemented. Added a deterministic headless disk-swap repro enabler (`--swap-disk-frame <N>`) reusing the M35 multi-disk cache — required to reproduce past YS II's two-disk boundary.

**S-c — Disk-save persistence (DONE).**
- `src/devices/fdc/disk_image.{h,cpp}`: optional `host_path_` + `dirty_` + `flush()`. `flush()` is write-only output (never re-reads host into emulation), a pure function of final `data_`; `write_lba` sets `dirty_`. Default (no host path) is byte-for-byte unchanged.
- `src/main.cpp`: `--disk-writable` (opt-in host write-back, flush at exit) + `--swap-disk-frame <N>` + caches the full repeatable `--disk` list. `src/frontend/sdl3_{app,cli,main}.{h,cpp}`: `disk_writable` config, host-path bind on attach, flush-before-swap-discard + cache update, flush on shutdown.
- `tools/format-blank-disk.ps1`: deterministic 737,280-byte truly-empty FAT12 720KB blank (media 0xF9) — distinct from `synthesize()`/`gen-m16-boot-disk.py`, which fill sectors with test garbage that would corrupt an empty filesystem.

**S-d — FM-PAC peripheral cartridge (DONE).** New `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` (`CartridgeFmPacRom`) grounded verbatim-behavior in `references/openmsx-21.0/src/sound/MSXFmPac.cc`: page-1 window (open-bus on pages 0/2/3), 4-bank ROM `rom[bank*0x4000+rel]` (bank masked to available banks), 8KB `BatteryBackedSram`, 0x5FFE/0x5FFF magic unlock (both bytes, re-checked each write), 0x7FF6 enable (bit0 I/O, bit4 force-reset), 0x7FF7 bank, 0x7FF4/0x7FF5 memory-mapped OPLL (forwarded to an owned `Ym2413Opll`; audio-mix wiring is a documented additive follow-on per scope §1.2). New enum `CartridgeMapperType::FmPac` + name string `"FMPAC"` (honestly disclosed: FM-PAC is an openMSX MSXDevice `REGISTER_MSXDEVICE(MSXFmPac,"FM-PAC")`, NOT a RomInfo RomType — `grep FMPAC RomInfo.cc` empty). Slot dispatch arm; `"PAC2OPLL"@0x18` signature auto-detection in `cartridge_identifier.{h,cpp}`; machine `fmpac(slot)` accessor + `set_fmpac_sram_path`/`flush_fmpac_sram` (`.sram` load-on-insert / save, mirroring the M20 Halnote pattern); headless `--fmpac-sram <path>`.

**S-e — internal `sram_` removed (DONE).** Removed `kSramBytes`, `sram_`, `sram_size()`, `sram()`, `sram_.clear()`. `serialize_state_dump()` now emits the inserted FM-PAC's 8KB SRAM when present, else `[SRAM] size=0` (bare machine = "NO S-RAM AVAILABLE", per DEC-0050). Added `BatteryBackedSram::data()`. Updated all three consumers: `hbf1xv_memory_regions_unit_test.cpp`, `hbf1xv_memory_regions_integration_test.cpp`, `hbf1xv_debug_dump_unit_test.cpp`. `halnote_.sram()` (16KB) and `s1985_engine` `sram_` (16B) untouched (verified by grep).

**S-f — R-M35-1 (DONE).** Public `Sdl3App::swap_to_next_disk()` + `current_disk_index()`; new multi_disk case asserts index advances 0→1→wrap + media-change signal.

## 3. Unit Test Results (captured)
- `devices_cartridge_fmpac_rom_unit_test` — Passed (unlock/relock, both-bytes-AND, bank select + mask + wrap, enable bit4 force-reset + magic-gate, SRAM R/W gated by unlock but not by enable bit0, OPLL memory-mapped ports, open-bus pages, is_valid_image_size).
- `devices_disk_image_writeback_unit_test` — Passed (flush round-trip, write-protect gate, no-path no-op, determinism: two runs byte-identical).
- `machine_hbf1xv_memory_regions_unit_test`, `machine_hbf1xv_debug_dump_unit_test` — Passed (updated for `sram_` removal; bare dump = `[SRAM] size=0`).

## 4. Integration Test Results (captured)
- `machine_hbf1xv_m36_fmpac_cartridge_integration_test` — Passed using the REAL `roms/fmpac.rom`: signature auto-ID → FmPac; `PAC2OPLL@0x4018` read through the bus (#A8 slot-1/page-1 select); magic unlock + SRAM R/W through the bus; `.sram` persist across two machine instances; absent `.sram` → zero default; state dump reflects the cart's 8KB SRAM; coexistence — built-in MSX-MUSIC #7C/#7D unaffected, FM-PAC OPLL is a separate chip.
- `machine_hbf1xv_m36_disk_save_persist_integration_test` — Passed (write→flush→reload→byte-identical; two-run determinism; default no-host-path in-memory-only, no file created).
- `machine_hbf1xv_memory_regions_integration_test` — Passed (updated).
- `frontend_sdl3_app_multi_disk_integration_test` — Passed; adversarial mutation confirmed (hard-coding `current_disk_index_ = 0` fails `DiskSwapAdvances_IndexBecomesOne`; built, ran, reverted).
- Full suite: **199/199 passed, 0 failed** (`ctest --test-dir build -C Debug`). Build exit 0. `tools/validate-assets.ps1` green; `docs/asset-checksums.txt` refreshed.

## 5. Bug B diagnosis + ESCALATION (the mandatory C-build item)
Repro-first, vision-read (every PNG viewed):
1. YS II disk1 boots → long auto-playing intro cinematic.
2. At frame ~11000 it renders "INSERT DATADISK IN DRIVE A-RET." — YS II is a two-disk game; the world (towns/buildings) is on disk2. RETURN with disk1 still inserted does not advance (correct).
3. Built `--swap-disk-frame` to swap to disk2 deterministically; after swap + RETURN, the game world runs: overworld field, HUD (M.P/EXP/GOLD/PLAYER/ENEMY), an NPC, dialog — all render correctly. State dump: CPU healthy (halted=0; PC varies 99B5/A385/9C6B; IFF1/IFF2=1). Bare-machine dump correctly shows `[SRAM] size=0`.
4. The starting-area building is a sealed cave with an NPC blocking the doorway — not enterable. Reaching an enterable interior needs YS II map knowledge / a specific route not deterministically derivable.

Discriminating evidence so far: normal field/dialog/NPC rendering is healthy (argues against a systemic VDP-blank on ordinary transitions); the M34/DEC-0045 R#1-BL render gate is present and correct (`vdp_frame_renderer.cpp:436`). The specific building-interior black-screen root cause (FDC interior-load read-fail from disk2 vs a VDP command-engine/blank on the interior redraw vs mapper mis-route) cannot be discriminated without reaching an actual interior.

ESCALATION (per the hard rule): (a) autonomous navigation to an enterable interior STALLED (sealed/guarded starting cave + no map knowledge); (b) the most likely fix surface is the VDP render path (escalation-required). No Bug B fix implemented. Unblock needed: a human YS II save-disk positioned at a building entrance (chosen by the human), OR a deterministic route to one.

## 6. Known Issues / Honest Disposition
- C-build (MANDATORY): NOT achieved — ESCALATED (navigation stall). Substantial groundwork delivered (deterministic headless disk-swap; game-world reached; field rendering proven healthy).
- C-save: BOTH paths implemented and PROVEN to persist deterministically at byte level via integration tests using the real `roms/fmpac.rom` and the real disk-write pipeline. The in-game (playtest PNG) save demonstration is blocked by the same navigation stall. Disk save alone satisfies the AND/OR C-save clause at the mechanism level.
- openMSX A/B: NOT run this session (WSL openMSX availability unverified). AC-d8 (FM-PAC memory A/B) and AC-b6 (Bug B first-divergence) pending, not fabricated. Disk write-back has no CPU-trace A/B by nature (round-trip verified instead, AC-c7).
- `roms/fmpac.rom` provenance (AC-d7, honest): SHA1 `2dc4517ebd5a061f9b5aa6b449cc4d4a2073540c`, SHA256 `e91f36f9664c650b2273adb515d3a544469413f3a8e7555b7ff31dafd2d29dcc`, 16384 bytes — a signature-valid but NON-CANONICAL FM-PAC BIOS variant (matches NEITHER openMSX `fmpac.xml` dump). Validated functionally (PAC2OPLL detect + unlock/bank behavior), never by hash-match.
- ZEXALL: withheld-justification holds — `git status` shows zero `src/devices/cpu/` and `src/core/` edits. (The full `ctest` run did include `m24_slow_full_sweep`, which passed — a bonus confirmation of zero CPU regression.)

## 7. QA Handoff
- Verify from the single canonical `build/` after a clean rebuild: `cmake --build build --config Debug` (exit 0), `ctest --test-dir build -C Debug --output-on-failure` (199/199).
- New/updated targets: `devices_cartridge_fmpac_rom_unit_test`, `devices_disk_image_writeback_unit_test`, `machine_hbf1xv_m36_fmpac_cartridge_integration_test`, `machine_hbf1xv_m36_disk_save_persist_integration_test`, `frontend_sdl3_app_multi_disk_integration_test` (+ mutation check), the three S-e-updated tests.
- Release recommendation: S-a/S-c/S-d/S-e/S-f are QA-ready. S-b (C-build) is the release gate and is ESCALATED, not done — do not tag v1.0.37 as "YS II fully playable" until Bug B is unblocked (human save-disk) and fixed/verified.

## 8. Coordinator Independent Verification Addendum (2026-07-10)
The coordinator independently verified (not merely relayed) the developer's claims:
- **Scope discipline**: `git status --short -- src/devices/cpu src/core` is EMPTY — zero CPU/core edits confirmed. All 30 modified + 7 new source/test files map to the six slices.
- **Tests re-run**: the 8 M36-touching tests re-run green from the existing `build/` (`ctest -R "m36|fmpac|disk_image_writeback|multi_disk|memory_regions|debug_dump"` → 8/8 Passed), including the FM-PAC integration test against the real `roms/fmpac.rom` and the disk-save round-trip.
- **Frames vision-read**: the coordinator opened `ys2_skip_f11000.png` (the real "INSERT DATADISK IN DRIVE A-RET." prompt), `ys2_data_f14000.png` (game world — field + YS II HUD + the "OVAL STONES…CAVE" dialog, rendering healthy), and `ys2_enter_f13400.png` (Adol + NPCs at a guarded stone structure). The game world was genuinely reached; the stall is a legitimate game-navigation limit, not an emulator fault on normal screens.
- **Disposition**: the escalation is HONEST and correct. Milestone remains IN PROGRESS pending the human-provided YS II save-disk to reproduce/fix Bug B and to demonstrate the in-game save-persist. The five completed slices are QA-ready but QA sign-off is deferred to M36 closure (single sign-off covering Phase-1 tooling too).

## 9. DSKCHG read-and-clear one-shot fix (2026-07-10) — a real latent media-change bug, NOT the Bug B cure

**SUPERSEDED-FRAMING CORRECTION (DEC-0052 + M36 QA sign-off R2):** this section was originally titled "Bug B FIX … resolves the §5 escalation" — that framing is WRONG and is corrected here. The DSKCHG read-and-clear change below is a genuine, correct, openMSX-grounded fix for a real latent MEDIA-CHANGE bug (the DSKCHG one-shot at 0x3FFD stayed asserted forever after any F11 disk swap) and it strengthened R-M35-1 — but it did **NOT** resolve the YS II building-interior black screen. A post-fix replay reproduced the identical freeze; the real cause was later root-caused (via the F10 stream-capture tool) to a nested VBLANK-interrupt storm from an upstream control-flow divergence (our V9958 interrupt path is audited-correct vs openMSX; disk reads are byte-perfect). **Bug B remains OPEN** — do not cite this fix as its resolution.

The DSKCHG diagnosis below was grounded in two live F12 snapshots — `debug/snapshot/f002495_…/` (just before entry) and `debug/snapshot/f002601_…/` (the black screen, ~106 frames later) — from which the coordinator diagnosed the root cause (this is grounded evidence, not the §5 hypothesis). The fix is a device-level FDC change — ZERO `src/devices/cpu/` and `src/core/` edits (ZEXALL withheld; `git status --porcelain src/devices/cpu src/core` empty).

**Root cause (from the two snapshots).** Both bundles are byte-identical in VDP and FDC, and the CPU is at the identical `PC=399B` with `HALT=1, IFF1=0` (only R + T-states advancing) → the game executed `DI; HALT` and is permanently frozen (a maskable IRQ is pending but masked; the machine has no NMI source). Slots `A8=FF` → all four pages are slot 3-0 RAM, so `PC=399B` is game code in RAM. FDC state shows `COMMAND=D0` (Force Interrupt / abort) and `DISK_CHANGED=1` still latched. Mechanism: `SonyFdc::mem_read` case `0x3FFD` exposes DSKCHG on bit 2, reading the non-clearing `DiskDrive::disk_changed()`. The F11 hot-swap (`sdl3_app.cpp` → `DiskDrive::set_disk_changed(true)`) sets the latch, and nothing cleared it except machine `reset()`. So after ANY disk swap DSKCHG stayed asserted forever; YS II's building-interior loader re-checks the medium after the swap, sees a perpetually-"changed" disk, retries/aborts (Force Interrupt) and drops into `DI; HALT`. Universal media-change bug, not YS II-specific.

**openMSX grounding.** Real hardware + openMSX implement DSKCHG as a read-and-clear one-shot:
- `references/openmsx-21.0/src/fdc/DiskChanger.cc:95-100` — `diskChanged()` returns the flag then sets `diskChangedFlag = false` (reading CLEARS the latch).
- `references/openmsx-21.0/src/fdc/PhilipsFDC.cc:37` — the register read (`readMem` 0x3FFD) calls the MUTATING `multiplexer.diskChanged()` (clears); `:90` — the debugger peek (`peekMem` 0x3FFD) calls the const `peekDiskChanged()` (does NOT clear).

**The fix (universal, model-hardware, never game-keyed).**
- `src/devices/fdc/disk_drive.h` — added a MUTATING `take_disk_changed()` that returns the current latch and sets `disk_changed_ = false`. The existing const `disk_changed()` is unchanged (non-clearing) for peek/snapshot.
- `src/devices/fdc/sony_fdc.cpp` (case `0x3FFD`) — now uses `drive_.take_disk_changed()` (mirrors PhilipsFDC.cc:37 readMem). One register read reports "changed" exactly once, then reverts to "not changed" → the freeze can no longer occur.
- `src/devices/fdc/sony_fdc.h` — decode-table comment updated to note read-clears the one-shot.

**System-wide review (feedback_system_wide_review_first).** The debug/snapshot path stays on the const, non-clearing getter: `src/machine/debug_snapshot.cpp:371 fdc_section` uses `drive.disk_changed()` (peek); the memory-region dumps read raw arrays (DRAM/VRAM/SRAM `.data()`) and the FDC section uses const `peek_*` getters — NONE route through `SonyFdc::mem_read`, so capturing a snapshot/state-dump never consumes the DSKCHG one-shot (Phase-3 determinism preserved). This mirrors the PhilipsFDC readMem (mutating) / peekMem (const) split, which the codebase already implements at the getter level. No separate SonyFdc peek path exists or is reachable by a debug dump, so none was added. Single-disk games are unaffected: with no swap `disk_changed_` stays false, so DSKCHG always reads "not changed" exactly as before.

**Test evidence (captured this session).**
- `devices_fdc_sony_fdc_unit_test` — Passed (0.03 s). Added the direct mechanism proof: simulate a swap (`set_disk_changed`), read 0x7FFD once → bit2==0 (changed) and the latch clears, read again → bit2==1 (reverts) — `Dskchg_OneShot_*`; plus the const `disk_changed()` peek is stable and does NOT clear (`Dskchg_ConstPeek_*`); plus a single-disk no-swap regression (`Dskchg_NoSwap_*`, always reads not-changed).
- `frontend_sdl3_app_multi_disk_integration_test` — Passed (5.31 s). R-M35-1 strengthened: after an F11 swap the const peek is stable, then `take_disk_changed()` (the exact accessor `SonyFdc::mem_read(0x7FFD)` calls) reads changed-then-clears end-to-end (`DiskSwapAdvances_Dskchg*`).
- Regression green: `devices_fdc_wd2793_type1..4`, `machine_hbf1xv_m16_fdc_integration_test`, `machine_hbf1xv_m16_boot_checkpoint_integration_test`, `hbf1xv_m28_c5_disk_boot_investigation_system_test`, and the Phase-3 `machine_debug_snapshot_devices_unit_test` + `hbf1xv_m36_snapshot_determinism_system_test` (#194) all Passed — proving the snapshot path still does not perturb.
- Full suite: **205/205 passed, 0 failed** (`ctest --test-dir build -C Debug`, ZEXALL `-E zexall` withheld). Build exit 0, SDL3=ON. `tools/validate-assets.ps1` green; `docs/asset-checksums.txt` refreshed.

**Honest disposition.** The fix is proven at the mechanism level (a single 0x7FFD read now clears DSKCHG, so the `DI;HALT` retry/abort trigger is removed) and by the end-to-end swap→read-and-clear integration path. A fully headless deterministic replay of the exact YS II building-entry freeze needs the actual game reaching an interior (the §5 navigation stall), so — per the M35 precedent — the authoritative end-to-end confirmation is the human replaying YS II to the building. Left UNCOMMITTED (coordinator owns closure).
