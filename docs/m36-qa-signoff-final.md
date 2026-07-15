# M36 QA Sign-off — FINAL (v1.0.37 release decision)

> Authored by the MSX QA Agent (opus), 2026-07-10; persisted by the coordinator (QA under a
> no-report-file constraint). No code changed by QA; working tree left untouched.

- **Milestone:** M36 (Phase 1 playtest harness + Phase 2 DEC-0050 device work + Phase 3 debug tooling + **Bug B VDP IE0/IE1 /INT fix** + F10 stream-light).
- **Base:** git HEAD `791e848`; the two final additions (Bug B fix, stream-light) verified UNCOMMITTED on top.
- **Method:** true clean rebuild of the single canonical `build/` (SDL3=ON); non-destructive adversarial mutation per DEC-0049; grounding re-read from `references/openmsx-21.0/src/video/VDP.cc`; independent openMSX 19.1 runtime A/B on the genuine `Sony_HB-F1XV` machine.

## 1–2. Regression matrix
- Clean rebuild exit 0; **`ctest -E hbf1xv_m24_zexall_system_test` → 207/207 passed, 0 failed** (ZEXALL correctly withheld; `git status --porcelain src/devices/cpu src/core` EMPTY).
- Bug B unit test `devices_v9958_ie0_register_write_irq_unit_test` — Passed. Interrupt-seam regressions (interrupt_ack, v9958_status_irq, m26_vsync_boundary, m32_line_interrupt, m34_bl_latch) — all Passed (normal VBlank + line-int paths behavior-unchanged).
- Watch-mode (m36_stream_capture, m36_snapshot_determinism) — Passed; additive/default-off verified (observers installed only under a stream-light arm; null-guarded no-ops when unarmed).
- Prior committed slices (FM-PAC unit+integration w/ real ROM, disk-save, DSKCHG one-shot, memory-regions, debug-dump) — all Passed.
- Fix vs source: re-read `VDP.cc:1177-1198` — the fix matches **line-for-line** (IE0 clear→reset; IE0 set→set iff `statusReg0 & 0x80`; IE1 clear→reset; SET edge left to the line-match poll). Register/VRAM parity A/B (`docs/m14-parity-trace-diff.md`) — ARCHITECTURAL PARITY (empty diff).

## 3. Adversarial + A/B
- **Non-vacuity (DEC-0049 non-destructive):** backed up `v9958_vdp.cpp`, neutered both `if (change & 0x1?/0x2?)` blocks → the test FAILED exactly on `Ie0Clear_FStillPending_IntDeasserts` + `Ie1Clear_IntDeasserts`; restored byte-identical (sha1 match); re-passes. Guard genuine.
- **Independent openMSX runtime A/B (QA re-ran, not relayed):** genuine `Sony_HB-F1XV`, CPU parked with `iff=0`, F latched, IE0 toggled, `VDP.IRQvertical` probe → `IV_baseline=0, IV_ie0set_Fpending=1, IV_ie0clear=0, IV_ie0set2=1` (0→1→0→1). Independently confirms the exact behavior our fix implements + the unit test asserts.

## 4. Residual-risk register (none release-gating)
- **R-A** WD2793 read has no rotational latency (Medium, accuracy backlog, DEC-0053) — audited NOT the Bug B cause; delicate FDC-timing follow-on.
- **R-B** the VDP-IRQ probe lived only in report prose (Low) — QA re-verified live; recommend committing a `tools/` probe harness for reproducibility.
- **R-C/R-D/R-E** FM-PAC `.sram` format vs openMSX (interchange-only), FM-PAC OPLL audio not yet mixed, `roms/fmpac.rom` non-canonical variant — all Low, disclosed.
- **R1/R2 CLEARED:** Bug B FIXED, source-grounded, non-vacuous test, regression-clean, runtime-A/B-confirmed, LIVE human-validated; the Phase-2 §9 overclaim correction is in place.

## 5. R3 disposition (runtime FM-PAC/DSKCHG A/B smoke)
Source-level parity + unit/integration tests + the (independently re-run) VDP-IRQ runtime A/B + the live human end-to-end validation are **SUFFICIENT for the v1.0.37 tag.** The widest-blast-radius change (VDP IE0/IE1 /INT) carries a first-hand runtime openMSX A/B AND empty-diff register/VRAM parity. FM-PAC is additive (active only with `--cart`), source-parity-verified vs `MSXFmPac.cc`, real-ROM integration-tested. DSKCHG is source-parity-verified vs `DiskChanger.cc`/`PhilipsFDC.cc`, unit+integration tested, and exercised by the live save-persist run. The dedicated AC-d8/AC-b6 smokes are **release-candidate hardening, not blockers**; recommend the coordinator record a short A/B waiver in `decisions.md` (waiving only the residual dedicated smokes, not A/B review for the behavior-critical VDP change).

## 6. Sign-off Decision — **PASS** (v1.0.37 release)
M36 is release-ready. The mandatory completion goal is MET: YS II building interiors load (Bug B FIXED) and data saving persists, **LIVE human-validated by the repo owner on the real SDL3 build** — the authoritative end-to-end evidence, corroborated by clean-rebuild 207/207 (ZEXALL withheld; CPU/core diff empty), a non-vacuous unit test, an unchanged interrupt-seam regression set, exact source parity vs `VDP.cc:1177-1198`, an independently re-run openMSX VDP-IRQ A/B, and additive/default-off proven for the stream-light tooling. No blocker-level gaps remain. **Approve tagging v1.0.37**, with the R3 waiver recorded.
