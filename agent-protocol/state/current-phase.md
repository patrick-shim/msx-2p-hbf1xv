# Current Phase

- Objective: The pre-authorized M21-M23 run is COMPLETE (v1.0.21/22/23). Now driving a NEW, similarly-scoped continuation per the human directive (2026-07-08) "let's advance to all M25, milestone by milestone, fully developed, and fully qa" — M24 (ZEXDOC/ZEXALL full parity sweep, C3) then M25 (Sony speed-controller + hardware PAUSE + Ren-Sha Turbo, C8), each with its own planner package, developer implementation, dedicated system integration test, QA sign-off, and separate tag, proceeding through the release-decision/tag step without an extra pause on a clean QA PASS (mirroring the M21-M23 cadence) — UNLESS QA does not reach a clean PASS, in which case: STOP and consult the human. The human ALSO stated (2026-07-08) "I confirm that there should be zero license-sensitive future work" (recorded as a standing project memory: never reproduce openMSX's own large data tables/arrays under `src/`, even framed as independent re-derivation — applies to any future C1/D4 VDP-timing-depth milestone) and flagged a forward request (NOT yet active — to be raised explicitly after M25 completes): debug/-folder test-artifact generation (e.g. rendered frames as PNGs) before M26 (SDL3 frontend) is tackled.
- Active Phase: M24 CLOSED (v1.0.24). M25 (Sony speed-controller + hardware PAUSE + Ren-Sha
  Turbo, backlog C8) not yet kicked off — next up per the standing M24-M25 continuation.
- Phase Owner: MSX Master Agent (coordinator)
- Phase Status: M24 closed by coordinator release decision (2026-07-08, DEC-0022/REQ-M24-005).
  This milestone's own standing directive named a Conditional-Pass stop condition that fired for
  the first time this run: QA (`docs/m24-qa-signoff.md`, RESP-M24-003) independently reproduced
  the full 134/134 ZEXALL/ZEXDOC headline claim from a genuinely clean rebuild (a THIRD from-
  scratch reproduction, byte-for-byte identical to the developer's and coordinator's own),
  confirmed license isolation/device isolation/the adversarial self-check/the combinatorial-total
  arithmetic/openMSX A/B PARITY all independently, and found no CPU-core defect — but returned a
  **Conditional Pass**, not a clean PASS, gated on one hardening fix: `tests/system/
  hbf1xv_m24_zexall_system_test.cpp` checked marker COUNT but never hard-asserted `error_markers ==
  0` for either suite. Per the standing directive, the coordinator stopped and consulted the human,
  who chose "Fix, re-confirm, then tag." The developer added the two hard assertions
  (`Zexall_ZeroErrorMarkers`/`Zexdoc_ZeroErrorMarkers`, purely additive, test-code-only) and
  re-ran the full slow sweep to completion a FOURTH independent time this cycle (1638.22s),
  landing on the identical 5,764,169,474-instruction/67-0-marker result yet again with the new
  hard gates genuinely exercised; the coordinator independently re-verified via direct file read
  plus its own fast-subset re-run. QA's DEC-0021 ruling (disk-boot-A/B sub-claim stays BLOCKED-as-
  recorded; `disks/` reserved for a future dedicated C5 investigation) was ratified as final.
  `references/zexall/` (GPL v2, YAZE-AG) committed as part of the closure commit, ending 5
  milestones (M19-M23) of sitting staged-but-uncommitted. Fourteen milestones M11-M24 now tagged
  (v1.0.11..v1.0.24).
- Current full suite: 124/124 (`ctest`, 121 prior + 3 new: `machine_cpm_bdos_harness_unit_test`,
  `machine_hbf1xv_m24_cpm_run_integration_test`, `hbf1xv_m24_zexall_system_test`), zero regression
  through M23. NOTE: `hbf1xv_m24_zexall_system_test` genuinely takes ~24-27 minutes to run (both
  real ZEXALL/ZEXDOC suites to completion) — registered with the CTest LABEL `m24_slow_full_sweep`
  so the routine evidence-gate cadence can exclude it explicitly via `ctest -LE m24_slow_full_sweep`
  (123/123 in ~3.5s) while the default, unfiltered `ctest` invocation still includes it for real.

## M21-M24 run summary

- **M21 (VDP rendering depth, v1.0.21)**: First pixel-rendering output path. `VdpFrameRenderer`/`FrameBuffer` — deterministic, pull-model, frozen-register-snapshot renderer, RGB555 pixels, zero new clock consumer. Every Target-Spec mode byte-exact (GRAPHIC7 GGGRRRBB byte layout; MULTICOLOR's real 256-wide canvas). YJK/YJK+YAE decode byte-exact, incl. an independently-verified floor-vs-truncation finding. G6/G7 planar interleave's CPU-port + display-path pieces closed (command-engine piece carried to M22). `ctest` 106/106. Closes D1/D5/D6; D7 IN-PROGRESS (M21 partial). QA PASS (`docs/m21-qa-signoff.md`).
- **M22 (sprites + VDP command engine, v1.0.22)**: `SpriteEngine` (D2) and `VdpCommandEngine` (D3), both owned inside `V9958Vdp`. Sprite Mode 1/2 byte-exact per `SpriteChecker.cc/.hh` (EC/CC/IC bits; color-0/TP transparency — a fact-sheet-vs-source discrepancy resolved in favor of source). Full R#32-46 register file, all 13 commands via a hybrid execution model (10 atomic; LMCM/LMMC/HMMC event-driven, mirroring the M16 FDC DRQ/INTRQ precedent). D7 closes IN FULL via 5 new coordinate-address functions (confirmed to bypass R#2 entirely — a genuinely new finding). `ctest` 117/117. Closes D2/D3; D7 DONE in full. QA PASS (`docs/m22-qa-signoff.md`) via genuinely independent, adversarial scrutiny — QA hand-verified the raw A/B divergence numbers itself and corrected the developer's causal narrative.
- **M23 (exact cycle timing, v1.0.23)**: Deliberately conservative scope, given this touches the highest-risk surface in the project (CPU-visible timing, adjacent to the zero-tolerance M9/M12 CPU-timing oracles). Closes C2 (Z80 HALT-R) in full: `Z80aCpu::step()`'s halted branch now calls `increment_refresh_register()`, and the already-existing machine-level formula automatically bills the correct 5T. Exactly one golden test deliberately updated (t3 4→5, elapsed_cycles 22→23). C1/D4 advance to IN-PROGRESS (M23 partial) with a real, tested, non-gating VDP access-timing foundation — the full slot tables and any actual CPU-execution gating explicitly declined this cycle, since the M21/M22 system tests already contain back-to-back `OUT (#98),A` writes that real gating could silently corrupt. `ctest` 121/121; all 10 named oracle suites plus all 6 M21/M22 device files confirmed genuinely untouched. QA PASS (`docs/m23-qa-signoff.md`) via exceptional scrutiny — QA independently re-ran the C2 A/B harness and designed its own exploratory probes to refine the developer's divergence hypothesis into a more precise, still-honest explanation.
- **M24 (ZEXDOC/ZEXALL full parity sweep, v1.0.24)**: CPU-validation milestone — a genuine, generic `CpmBdosHarness` (zero zexall-specific knowledge) ran the real, GPL-licensed ZEXALL/ZEXDOC Z80 exerciser suite against the already-QA-verified Z80A CPU core to genuine CP/M warm-boot completion. **134/134 group verdicts PASS**, independently reproduced FOUR separate times from clean rebuilds (developer, coordinator, QA, post-fix confirmation), every time byte-for-byte identical (5,764,169,474 instructions/suite, 67/0 marker split). Adversarial self-check PASSED; openMSX A/B bounded-prefix PARITY independently re-run twice (developer, QA). `ctest` 124/124; zero changes to `src/devices/`/`src/peripherals/`/`src/core/`. QA's first-ever Conditional Pass this run (a real, if narrow, regression-harness gap — see Phase Status above) was fixed and reconfirmed per the human's explicit choice before tagging. Closes C3 in full.

## Standing watch-items (carried forward, none blocking)

1. `references/zexall/` (legally-sourced ZEXALL/ZEXDOC suite) is now COMMITTED as part of M24's closure (DEC-0022) — resolved, no longer a watch-item.
2. C5 (FDC full-boot residual) still needs the real auto-disk-boot trigger investigated. The
   human added real MSX-DOS system-disk images at `disks/msxdos22.dsk` (737,280 bytes),
   `disks/msxdos23.dsk` (737,280 bytes), and an unpacked `disks/msxdos24/` tree (`COMMAND2.COM`,
   `MSXDOS.SYS`, `MSXDOS2.SYS`, `AUTOEXEC.BAT`, `HELP/`, `UTILS/`, version 2.32 per `MSXDOS2.SYS`'s
   embedded string) — all three sized/shaped consistent with the HB-F1XV's documented 720 KB 3.5"
   floppy spec (DEC-0021). QA formally ruled (M24 closure, DEC-0022) that redoing C3/M24's
   disk-boot-A/B sub-claim against this asset would require solving C5's own still-unsolved
   auto-boot-trigger problem plus new keystroke-automation scope — out of scope for a QA-step
   fix, so it stays BLOCKED-as-recorded. `disks/` is reserved for a FUTURE, dedicated C5 milestone.
3. Any test asserting real ROM/asset content must call `machine.set_asset_root(SONY_MSX_BIOS_DIR)` before `cold_boot()` with the matching `tests/CMakeLists.txt` compile-definition wiring, or it silently 0xFF-fills under `ctest` (DEC-0016).
4. M20's disclosed sub-mapper-shadow A/B divergence (openMSX 19.1 runtime-side) could be re-checked against a newer openMSX build if available — not blocking.
5. The interlace/EO hedge (D6, M21) is a documented SIMPLIFICATION of openMSX's own `getEvenOddMask()`, not a bit-for-bit port.
6. M21's A/B evidence could not live-compare computed pixel/color values (no introspection point in openMSX) — an environment limitation.
7. ASX (S#8/S#9, M22) persistence is narrower in scope than `vdp_command_engine.h`'s own header comment states (only SRCH/LMCM persist it, not LINE/LMMM/HMMM) — a documentation-only correction recommended for a future cycle.
8. Future A/B probes for the VDP command engine should explicitly include S#7/S#8/S#9 in their comparison gate or explicitly disclose the exclusion.
9. NEW from M23: the C2 A/B divergence's refined, authoritative causal framing is "emulated time accumulated during the unthrottled pre-measurement boot window, per openMSX's own bulk `advanceHalt` scheduling" — not the implementation report's original, less-precise "wall-clock time between Tcl round-trips" framing.
10. NEW from M23: C1/D4's precisely-named 5-item remainder (full per-mode/per-scanline slot tables + raster-position reconciliation with `run_frame()`'s whole-frame-atomic jump; CPU-priority-steals-command-engine-slot interaction; command engine's real per-pixel/per-line VDP-cycle cost; exact sub-frame IRQ raster position; any actual CPU-execution gating) is genuinely large, exacting, license-sensitive future work (~340 numeric table entries to independently re-derive) — a dedicated future milestone, not a quick add-on.
11. NEW from M23: any future milestone attempting real CPU-execution gating on VDP access timing must first explicitly re-validate the M21/M22 system-test fixtures (`tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62`, `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78`) against real wait/drop behavior — a demonstrated, not hypothetical, regression risk.
12. NEW from M24: `git diff <tag> -- <path>` cannot show content for a wholly untracked (never `git add`ed) file, regardless of what it contains — confirmed via `git cat-file -e <tag>:<path>` returning "exists on disk, but not in `<tag>`". Verifying whether a new, not-yet-committed file contains an expected change requires a direct file read, not a tag-diff, or it can produce a false "not applied" conclusion.
13. NEW from M24: `hbf1xv_m24_zexall_system_test` now hard-asserts `error_markers == 0` for both suites (post-DEC-0022 hardening fix) — the project's single most valuable CPU-regression oracle is now a proper zero-tolerance gate, not observe-and-report. Any future milestone adding a similarly expensive, high-value regression test should default to a hard-gate design from the start, per QA's M24 finding.

## Indicative follow-on order (per `agent-protocol/state/deferred-backlog.md`)

M25 Sony speed-controller/hardware PAUSE (C8) · M26 SDL3 frontend (C9) · a future VDP-timing-depth milestone (C1/D4 remainder) · a future dedicated C5 real-disk-boot-trigger investigation (now unblocked on assets via `disks/`, DEC-0021). M25 not yet kicked off — awaiting human directive.

- Updated At: 2026-07-08T09:10:00+09:00 (M24 CLOSED, tagged v1.0.24, DEC-0022; QA's Conditional
  Pass fixed and reconfirmed per the human's explicit choice; next up: M25 kickoff)
