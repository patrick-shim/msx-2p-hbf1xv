# M36 Bug B FIX — V9958 IE0/IE1 register-write /INT re-evaluation (YS II interior-load crash)

**Date**: 2026-07-10
**Milestone**: M36 (mandatory item: "YS II building interiors load")
**Status**: FIXED — unit-tested + regression-clean (207/207) + openMSX A/B-validated + **LIVE human-validated** (the building interior renders; data saving works). Uncommitted at authoring; committed at M36 closure.

> Authored by the MSX Developer Agent (opus); persisted by the coordinator, with the coordinator's
> independent verification (§7). The definitive end-to-end signal is the human's live YS II re-run:
> the interior loads instead of freezing, and a save persists.

## 1. The bug (proven from the live F10 watch-log)
The crash is a nested VBLANK-interrupt storm. The live watch-log `debug/traces/stream_f001882_..._watch.log` shows YS II's VBlank ISR repeatedly writes **R#1 = 0x40 (IE0 OFF)** (pc=A461, ~398×) into the crash. On real hardware, clearing IE0 **immediately de-asserts /INT**, so the ISR's trailing `EI` (at 0xA373) cannot re-fire. Our VDP drove /INT purely from the `irq_vertical_` latch (set at VBlank, cleared only by an S#0 read); **`change_register` case 1 (the R#1 write) never re-evaluated /INT** — so after the ISR cleared IE0 our /INT stayed asserted (F still latched) → the `EI` re-entered the ISR forever → stack overflow → the black screen. (The earlier audit checked the S#0-read path and wrongly concluded "VDP correct"; it missed the register-write path.)

## 2. The fix — grounded in openMSX `references/openmsx-21.0/src/video/VDP.cc:1170-1198`
In `V9958Vdp::change_register` (`src/devices/video/v9958_vdp.cpp`): capture the OLD register value before the commit, compute `change = old ^ new`, then:
- **case 1 (R#1 / IE0, bit5):** if `(change & 0x20)` — IE0 set → `if (status_reg0_ & 0x80) irq_vertical_ = true;` (re-assert only if F is pending, openMSX's documented Andonis/Zanac case, VDP.cc:1189-1194); IE0 cleared → `irq_vertical_ = false;` (VDP.cc:1196). Then `update_irq();`.
- **case 0 (R#0 / IE1, bit4):** if `(change & 0x10)` — IE1 cleared → `irq_horizontal_ = false;` (VDP.cc:1182); the SET edge is left to the existing per-step line-match poll (`Hbf1xvMachine::poll_line_interrupt → V9958Vdp::on_line_match`, our analogue of openMSX's `scheduleHScan`). Then `update_irq();`.
- `on_vsync()` and the S#0-read path are UNCHANGED; the fix only ADDS the register-write re-evaluation, and only on the IE0/IE1 change edge (writes that leave the enable bit unchanged are behavior-neutral). Universal — not YS-II-specific.

## 3. Evidence
- **New unit test** `tests/unit/devices/video_v9958_ie0_register_write_irq_unit_test.cpp` — PASS. Covers IE0-clear (R#1=0x40) de-asserts /INT while F pending; IE0-reenable re-asserts (F pending); IE0-reenable with no F pending does not assert; an R#1 write leaving IE0 set doesn't disturb a held /INT; IE1-clear de-asserts the line /INT; the normal VBlank path holds until S#0 read. **Non-vacuous**: with the two `if (change & 0x2?)` blocks neutered, the test fails exactly on `Ie0Clear_FStillPending_IntDeasserts` + `Ie1Clear_IntDeasserts`; restored → clean pass.
- **Full suite**: `ctest -E hbf1xv_m24_zexall_system_test` → **207/207**, ZEXALL withheld. Re-confirmed regressions: `machine_hbf1xv_interrupt_ack_integration_test`, `devices_v9958_status_irq_unit_test` (M14 seam: exactly-one-accept, /INT released on S#0 read, no re-trigger), `machine_hbf1xv_m26_vsync_boundary_integration_test` (normal VBlank path unchanged), `machine_hbf1xv_m32_line_interrupt_integration_test`, `machine_hbf1xv_m34_bl_latch_integration_test` — all green.
- **openMSX A/B (WSL openMSX 19.1, genuine Sony_HB-F1XV machine):**
  - Register/VRAM path parity (`tools/openmsx-vdp-parity.ps1`): ARCHITECTURAL PARITY (empty diff) — the edit doesn't perturb the register-write/VRAM contract (`docs/m14-parity-trace-diff.md`).
  - Targeted VDP-IRQ A/B (openMSX `VDP.IRQvertical` probe; hold F, toggle R#1 IE0): `S0=9F asserted=1 ie0_clear=0 ie0_set=1 ie0_clear2=0` — the **1→0→1→0** pattern is identical to our fix + the unit test. openMSX confirms the register-write IE0 re-evaluation is correct hardware behavior.
- **LIVE human validation**: the human re-ran YS II into the building interior — it RENDERS (no freeze); data saving also confirmed working.
- Zero `src/devices/cpu/` / `src/core/` edits (`git status --porcelain` empty; ZEXALL withheld).

## 4. Files
`src/devices/video/v9958_vdp.cpp` (the fix); `tests/unit/devices/video_v9958_ie0_register_write_irq_unit_test.cpp` + `tests/CMakeLists.txt` (new test); `docs/m14-parity-trace-diff.md` + `docs/asset-checksums.txt` (regenerated).

## 5. Disposition
This supersedes the DEC-0053 conclusion that "our V9958 interrupt path is correct" — that was incomplete (it verified the S#0-read path but not the register-write path). The actual cause was the missing IE0/IE1 register-write /INT re-evaluation, now fixed, A/B-validated, and live-confirmed. **M36 Bug B is CLOSED.**

## 7. Coordinator Independent Verification (2026-07-10)
- `git status --porcelain src/devices/cpu src/core` = EMPTY.
- Read the actual `change_register` diff — matches openMSX VDP.cc:1170-1198 exactly (old-value capture, IE0 set/reset per F, IE1 clear, `update_irq()`), `on_vsync`/S#0-read untouched.
- Re-ran the new IE0 test + the interrupt-seam regressions (v9958_ie0, m26_vsync, m32_line_interrupt, m34_bl_latch) → 6/6 Passed; the rebuilt `sony_msx_sdl3.exe` carries the fix.
- The human live-confirmed the building loads + saves persist — the authoritative end-to-end signal.
