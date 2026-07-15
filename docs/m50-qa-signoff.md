# M50 QA Sign-off — Externalized strict-XML configuration (production hardening)

**Recommendation: PASS** (2026-07-14). DEC-0077. Owner makes the release decision after this sign-off.

Scope: move the emulator's DEFAULTS/KNOBS out of hardcoded C++ into a single strict
`sony_msx_hbf1xv.xml` (config FOUND → parse+validate+use, precedence CLI > XML > built-in default;
NOT found → one WARNING + built-in defaults). Four slices delivered, UNCOMMITTED at QA time (11
tracked-modified files + 7 new untracked files + the 7 gitignored guard tests). Pure configuration
plumbing — NO new device/timing behavior; every settable value was A/B-validated in its origin
milestone (RAM M42, FM-PAC/fast-disk M46, video/persistence M37/M39). NOTE: no
`docs/m50-implementation-report.md` exists — QA verified the delivered code directly by reading
every new/changed file and re-running all gates.

---

## 1. Regression Scope

- **Determinism gate (AC-1/AC-7):** the no-config path must be byte-identical to pre-M50 on every
  headless / `--debug-session` / `*-parity` path and every SDL3 `--hidden-window` run.
- **Accuracy protection (AC-6, §3 HARD-EXCLUDE):** zero diff over the silicon-timing set.
- **Shipped == defaults round-trip (AC-2/AC-S4).**
- **Precedence CLI > XML > default per knob (AC-3/AC-4/AC-S2).**
- **Graceful malformed-robustness (AC-5, never throws/crashes).**
- **Parser reuse / software_db behavior-preserving refactor (AC-8/AC-S1-4).**
- **Full deterministic suite + asset/publishability gates.**

## 2. Regression Matrix Status (all real output)

| # | Invariant | Result |
| --- | --- | --- |
| 1 | **§3 empty timing diff** — `git diff` over `src/core`, `src/devices/cpu`, `vdp_access_timing.h`, `v9958_vdp.*`, `sprite_engine.*`, `src/devices/fdc/wd2793.*`, `s1985_engine.*` | **0 lines**; no untracked leak into any timing dir (only leaks are the M50 files in `src/machine`+`src/frontend`). Re-attested 0 after all QA activity. **PASS** |
| 1 | **Byte-identical no-config fallback** | Headless NEVER auto-loads (code + no test passes `--config`); SDL3 gates on `!hidden_window`; default-constructed `EmulatorConfig` = built-in defaults so `resolve_*` reproduces the pre-M50 convenience defaults. All 235 pre-M50 tests GREEN + UNCHANGED. **PASS** |
| 2 | **Shipped == defaults + zero warnings** | `machine_hbf1xv_shipped_config_roundtrip_integration_test` **Passed**. Guard proven genuine (see §3 below). **PASS** |
| 3 | **Precedence CLI > XML > default per knob** | `frontend_config_runtime_unit_test` + `frontend_machine_config_resolution_unit_test` **Passed** — non-tautological (RAM three-way ladder, per-knob CLI-flips-XML, `--stock` preset over XML). **PASS** |
| 4 | **Graceful malformed handling** | `machine_emulator_config_unit_test` **Passed** — per-key WARNING naming key + default fallback (bad bool/enum/range, unknown key/attr) with co-present valid key still applied; structural garbage/wrong-root/empty → one whole-file WARNING + all defaults; never throws. **PASS** |
| 5 | **Full ctest** | `cmake --build build --config Debug --clean-first` clean (both exes, exit 0); `ctest -C Debug -LE m24_slow_full_sweep` → **242/242, 0 failed, exit 0** (52.6 s). The 7 new M50 tests all Passed. **PASS** |
| 6 | **ZEXALL/ZEXDOC** | NOT run — correct gate. Empty `src/devices/cpu` + `src/core` diff (per #1), so per `feedback_slow_test_cadence.md` the fast subset is the correct gate; the excluded `m24_slow_full_sweep` label holds exactly 1 test (`hbf1xv_m24_zexall_system_test`). **PASS (correctly skipped)** |
| 7 | **Publishability + assets** | `git check-ignore sony_msx_hbf1xv.xml` → exit 1 (**NOT ignored**, publishable); `tools/validate-assets.ps1` → **True** (7 BIOS present matching config defaults + 1 ROM). **PASS** |
| 8 | **Parser reuse / refactor** | `software_db.cpp` refactor = pure identifier rename (`TagScanner`→`XmlTagScanner`) + delegation to extracted `xml_tokenizer.*` with `capture_attributes=false` (byte-for-byte tokenization); `machine_software_db_unit_test` + `machine_xml_tokenizer_unit_test` **Passed**; no new third-party dependency. **PASS** |

**Test count reconciliation (confirms AC-1):** pre-M50 baseline 235 (excl. ZEXALL, A-1) + 7 new M50
tests = **242** ran; +1 excluded ZEXALL = 243 registered. Pre-M50 tests are additive-unchanged.

### Adversarial verification (non-destructive, DEC-0049 compliant)
Backed up `sony_msx_hbf1xv.xml` via `cp` (never `git checkout`), mutated `<ram kb="512">`→`"256"`, ran
ONLY the round-trip test → **FAILED (exit 1)** with `DRIFT: ram_kb shipped != compiled default` +
missing-token + load_from_file cases; restored from backup, re-ran → **Passed (exit 0)**; confirmed
`git status` byte-identical to the intended M50 state and no `.qabak` leak. The round-trip guard is
GENUINE, not tautological — it catches both a drifted default and a mistyped shipped value.

## 3. Failures and Risk Ranking

No failures. Residual risks (all LOW):

- **R-A (LOW, informational):** the entire `tests/` tree is gitignored repo-wide (pre-existing
  `.gitignore:203:/tests/`, NOT touched by M50), so the 7 new anti-drift/precedence guard tests —
  like all prior milestone tests — will not be published when the owner commits M50. This is the
  established repo convention (tests are local); the guards still run in every local/QA build. Not
  an M50 defect; flagged only so the owner is aware the round-trip anti-drift guard does not travel
  with the public remote.
- **R-B (LOW):** `<machine><slots>` is validate-and-echo only (S1 accepts + ignores the subtree; no
  live remapping) — matches planner §4.5 (arbitrary slot remapping explicitly OUT of M50). No risk;
  documented scope.
- **R-C (LOW):** VRAM is validated-to-128 and never resizes the VDP (no runtime seam) — matches
  planner §4.4. The knob is documentation + honest-ceiling only.
- **R-D (LOW):** 22 pre-existing C4834 `[[nodiscard]]` build warnings in EXISTING test files
  (cartridge_slot, wd2793_type1-4, battery_backed_sram, m19/m29 cartridge) — none in M50 code; the
  three new M50 source units compiled warning-clean. Pre-existing, non-blocking.

## 4. Required Fixes

None blocking. Optional follow-ups (owner discretion, non-blocking): (a) decide whether the M50
guard tests warrant an exception to the repo-wide `tests/` ignore so the anti-drift protection is
published; (b) sweep the pre-existing C4834 test warnings in a future hygiene pass.

## 5. Sign-off Decision

**PASS** → owner makes the release decision. All milestone acceptance criteria (AC-1..AC-8) and
per-slice criteria (AC-S1..AC-S4) verified against real command output. Byte-identical no-config
determinism proven (empty §3 diff + 235 pre-M50 tests unchanged); shipped-config round-trip guard
proven genuine by adversarial mutation; precedence and graceful-fallback proven non-tautologically;
242/242 suite GREEN; ZEXALL correctly skipped (zero cpu/core diff); config publishable; assets valid.
openMSX A/B not required (pure config plumbing, empty timing diff) — recorded here so its absence is
not read as an oversight.
