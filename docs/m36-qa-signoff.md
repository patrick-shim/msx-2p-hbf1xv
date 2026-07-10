# M36 QA Sign-off — Committed WIP Checkpoint (HEAD `af44cb3`)

> Authored by the MSX QA Agent (opus), 2026-07-10; persisted by the coordinator (QA was under a
> no-report-file session constraint and returned the text). No code was changed by QA; the working
> tree was clean after verification.

- **Milestone ID:** M36 (Phase 2 device-level DEC-0050 + Phase 3 debug tooling DEC-0051/0052), committed WIP checkpoint `af44cb3`
- **Scope note:** Verdict covers the COMPLETED, committed M36 slices only. Bug B (YS II building-interior crash) is an explicitly documented, tracked OPEN item and is NOT treated as a QA failure (see Residual Risk R1).
- **Method:** Clean rebuild of the single canonical `build/` via `tools/bootstrap-build.ps1` (SDL3=ON). Non-destructive adversarial mutation per DEC-0049 (backup → mutate → rebuild one target → restore → verify byte-identical). No `git checkout/restore/stash` on working-tree files. Final working-tree diff empty; CPU/core diff vs `ba30b36` empty.

## 1. Regression Scope
Committed slices verified (zero `src/devices/cpu`/`src/core` edits): Phase 2 — FM-PAC peripheral cartridge (`cartridge_fmpac_rom.*`); disk-save write-back (`disk_image.*`, opt-in `--disk-writable`, `tools/format-blank-disk.ps1`); speculative internal `sram_` removal (bare dump `[SRAM] size=0`); DSKCHG read-and-clear one-shot (`sony_fdc.cpp` 0x3FFD → `take_disk_changed()`); R-M35-1. Phase 3 — `debug_snapshot.*` (F12 + `--snapshot`, 13 additive const getters); F10 live stream-capture.

## 2. Regression Matrix Status
Build: clean rebuild exit 0, zero compile errors. **`ctest -E hbf1xv_m24_zexall_system_test` → 206/206 passed, 0 failed** (63.8 s; ZEXALL correctly withheld — CPU/core diff empty). Consolidated M36 focus re-run → **15/15 passed** (FM-PAC unit+integration w/ real ROM; disk-save unit+integration; DSKCHG one-shot; R-M35-1; `sram_`-removal ×3; snapshot ×4 + determinism system test FNV digest 23764175; stream-capture system test).

Source-level openMSX parity independently re-read (not relayed): DSKCHG read-and-clear vs `DiskChanger.cc:95-100` / `PhilipsFDC.cc:37` (mutating readMem) vs `:90` (const peekMem) — the port mirrors it exactly (`mem_read(0x3FFD)` mutating; `debug_snapshot.cpp:371` peek const). FM-PAC register map vs `MSXFmPac.cc` — faithful (enable `&0x11`, bit4 force-reset re-locks, bank `&0x03`, magic AND-gate 0x4D/0x69, page-1-only open-bus, memory-mapped OPLL). Additive/default-off behaviorally proven (byte-identical run counters + `--dump-state` with/without `--snapshot`; golden serializer untouched).

## 3. Adversarial-Mutation Outcomes (non-vacuous, DEC-0049 non-destructive)
- Revert DSKCHG `take_disk_changed()`→`disk_changed()` → `devices_fdc_sony_fdc_unit_test` FAILs (`Dskchg_OneShot_*`), restored byte-identical, re-passes.
- FM-PAC unlock AND→OR in `check_sram_enable` → `devices_cartridge_fmpac_rom_unit_test` FAILs (`Unlock_WrongSecondByte_Relocks` +2), restored byte-identical, re-passes.
Both guards genuine; tree clean after (only gitignored `debug/m36_ysii_repro/` untracked).

## 4. Residual-Risk Register (none blocker-level for a WIP checkpoint)
- **R1 — Bug B (YS II building-interior crash): OPEN, carried forward. High (release-gating), NOT a checkpoint failure.** Per DEC-0052 the DSKCHG fix corrected a genuine latent media-change bug + strengthened R-M35-1 but was NOT the freeze cause. Documented root cause: a nested VBLANK-interrupt storm from an upstream divergence; V9958 interrupt path audited-correct vs openMSX; disk reads byte-perfect. Out of scope for this sign-off.
- **R2 — Documentation overclaim (Medium).** Phase 2 report §9 header ("Bug B FIX … resolves the §5 escalation") frames the DSKCHG fix as the Bug B cure — superseded by DEC-0052. The fix is correct/valuable on its own merits; must be annotated before any release messaging.
- **R3 — No runtime openMSX A/B smoke for M36 behavior changes (Medium, release-gate).** AC-d8 (FM-PAC memory A/B) + AC-b6 (Bug B first-divergence) not run (WSL openMSX availability unverified); no explicit waiver in decisions.md. Mitigation: source-level parity independently verified (§2); unit/integration tests lock the semantics. Runtime smoke is release-candidate hardening.
- **R4 — FM-PAC SRAM persistence format divergence (Low).** Emulation-visible window identical to openMSX (0x1FFE); only the `.sram` on-disk file format differs (raw 8192 vs openMSX's headered store). Relevant only for save-file interchange.
- **R5 — FM-PAC OPLL audio not yet mixed into machine output (Low, disclosed follow-on).** `opll()` is the seam; does not affect SRAM game-save behavior.
- **R6 — `roms/fmpac.rom` is a non-canonical BIOS variant (Low, disclosed).** 16,384 bytes, SHA1 `2dc4517e…`; validated functionally (PAC2OPLL detect + unlock/bank), never by hash-match.

## 5. Conditions gating a future production-candidate/release tag
1. (R1/R2) Do NOT tag any release as "YS II playable" / "Bug B fixed"; annotate the Phase 2 §9 report so the DSKCHG change reads as a latent media-change bug fix (+ R-M35-1), not the Bug B cure.
2. (R3) Before a release tag: capture a runtime openMSX A/B smoke (FM-PAC AC-d8 + DSKCHG AC-b6) into `docs/openmsx-ab-smoke.md`, OR record an explicit A/B waiver in `decisions.md`.
3. (R4, optional) Reconcile FM-PAC SRAM `.sav` format if openMSX interchange is ever desired.

## 6. Sign-off Decision — **CONDITIONAL PASS** (committed portion)
The committed M36 slices are QA-approved for the checkpoint: clean-rebuild 206/206 (ZEXALL withheld; CPU/core diff empty), 15/15 focus targets, openMSX source-parity independently verified, additive/default-off proven byte-identical, both adversarial mutations non-vacuous with a fully restored clean tree. **Conditional** solely because release-tagging is gated on §5 (Bug B open + must not be advertised as resolved; runtime A/B smoke or a formal waiver before a production tag) — conditions concerning release messaging and A/B hardening, not the correctness of the committed code, which passes.

Carried forward: Bug B (OPEN, DEC-0052) — not a QA failure for this sign-off.
