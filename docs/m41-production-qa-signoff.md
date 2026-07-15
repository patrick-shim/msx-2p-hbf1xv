# M41 — Final Production-QA Sign-off (Sony HB-F1XV)

**QA owner:** msx-qa · **Date:** 2026-07-11 · **HEAD:** `6f6fb39` (main) · **Decision: CONDITIONAL PASS → conditions C1–C3 DISCHARGED 2026-07-11 (coordinator)**

> Authoritative record for DEC-0059. This file is local (agent-protocol / docs / debug / tools /
> tests are gitignored per DEC-0044); only the src/feature commits are on the export remote.

## 1. Regression Scope

Release-confidence gate for the M41 system-wide openMSX A/B parity sweep (8 subsystems: keyboard,
joystick, PSG, FM/OPLL, BASIC execution, screen modes, sprites, disk I/O) plus the three M41 source
changes it produced. Verification was performed independently from the single canonical `build/`
tree (DEC-0041), not taken on the developer's word: clean project rebuild, fast + slow ctest re-run,
asset gate, commit-scope audit, independent re-diff of the post-fix YJK captures, an adversarial
non-tautology mutation of the YJK fix, independent re-extraction of the disk on-disk bytes, and
reference cross-checks (fMSX + fact-sheet) of the load-bearing YJK direction claim.

## 2. Independent Gate Results

| Gate | Result | Evidence |
|---|---|---|
| Clean rebuild, SDL3=ON, both exes | **PASS** | `tools/bootstrap-build.ps1` exit 0; both `sony_msx_headless.exe` + `sony_msx_sdl3.exe` present |
| Fast ctest (`-LE m24_slow_full_sweep`) | **PASS — 215/215, 0 failed** | 78 s; includes SDL3-gated + new YJK/PSG-envelope/JOY tests |
| ZEXALL/ZEXDOC full sweep (`-L m24_slow_full_sweep`) | **PASS** | `Test #129 Passed 2069.15 s` (34.5 min), 0 failed, `ZEXALL_EXIT=0`, hard-asserts `error_markers==0` both exercisers. Log: `debug/m41/zexall/m41-zexall-sweep.log` |
| Asset gate | **PASS** | `tools/validate-assets.ps1` → True; 7 BIOS + 5 ROMs |
| 3 M41 commits on `main`, src-scoped, de-AI | **CONFIRMED** | `3976bc4` (JOY: input_script.*+main.cpp), `6044c19` (YJK: devices/video/vdp_frame_renderer.*), `6f6fb39` (PSGENV: devices/audio/psg_ym2149.*). **None touch src/core or src/devices/cpu** → per-edit ZEXALL rule correctly did not fire. No Co-Authored-By/AI-attribution trailer |

The ZEXALL sweep is the production-candidate CPU checkpoint; running it here satisfies the
slow-test-cadence rule even though M41 made no cpu/core edits. Its green, zero-marker result is
consistent with the git-proven absence of cpu/core changes.

## 3. Consolidated Final Parity Matrix

**True fully-verified MATCH: 30** · Partial (our-side correct, oracle incomplete): 1 (H3) ·
KNOWN-DELTA: 4 · BLOCKED: 3 · **Total dispositioned: 38.**

| Subsystem | MATCH | KNOWN-DELTA | BLOCKED | Notes / independent verification |
|---|---|---|---|---|
| Screen modes (M0–M12) | 8 exact + **2 YJK post-fix (m10,m12)** + baseline_cart_g2 = **11** | — | — | Re-ran the diff tool on the committed post-fix captures: m10_yjkyae, m12_yjk, probe_yae_yjk, probe_yae_pal all `pct_mismatch=0.000`; G7/G4 controls unchanged 0.000 |
| Sprites (SP1–SP3) | 3 | — | — | Static-settled scenes MATCH |
| Keyboard (K*) | 2 (k1,k2) | — | — | Covers RCTRL `:`/`*` (`73ce71c`), quotes, `/` |
| BASIC exec (B1–B4) | 4 | — | — | float / string / PRINT USING / FOR all MATCH |
| PSG (P1–P5) | 6 (incl. **p3_envelope resolved post-fix**) | — | — | Pitch bit-exact (0.00 cents); p5 upper-harmonic KD-PSG-RESAMP sub-threshold, dispositioned MATCH |
| FM/OPLL (F1–F4) | — | **4** (f1,f2,f4 KD-FM-SYNTH; f3 KD-FM-RHYTHM) | — | Pitch bit-exact, timbre cosine 0.996–0.998; attack/rhythm NOT sample-exact = license-blocked |
| Disk I/O (H1–H5) | **4** (H1,H2,H4,H5) | — | — | H1/H4/H5 on-disk bytes **independently re-extracted, SHA-equal** ours==omx; H3 = our-side correct + openMSX on-disk BLOCKED (see C1) |
| Joystick (J1–J3) | — | — | **3** | openMSX keyjoystick port unscriptable headless; decode A/B-MATCH via STICK(0) control |

**Honesty review — every non-MATCH is legitimately irreducible, no masked defect:**

- **KD-FM-SYNTH / KD-FM-RHYTHM (4):** the standing license-sensitive-scope ban. `YM2413NukeYktTables.ii`
  appears in `src/` ONLY as a non-opening attestation (`ym2413_synth.h:78-79`), never transcribed;
  attack/rhythm approximations are disclosed in-source. Pitch/timbre bit-exact; sample-exactness
  correctly **not claimed**. Permanent, correct.
- **BLOCKED joystick (3):** genuine openMSX harness limitation (keyjoystick reads host SDL events,
  not the MSX matrix). **Not masked** — the STICK/STRIG *decode* is proven A/B-equal via the STICK(0)
  cursor-key control; our side fully validated (8 dirs + triggers + port-2, correct MSX encoding).
  Mirrors the M25 hardware-PAUSE BLOCKED precedent.

**Non-tautology confirmation (adversarial mutation, DEC-0049-compliant):** backed up
`vdp_frame_renderer.cpp`, mutated `kYjkDisplayLead 4→0`, rebuilt only the YJK target → YJK unit test
FAILED with 11 case failures; restored from backup, `git status` byte-identical, rebuilt → test
passed again. The YJK regression guard is genuinely non-tautological. The PSG-envelope test is
first-principles (one step per EP*16 CPU cycles; full ramp = 512*EP; EP-linear scaling), not a
number-flip.

## 4. Findings (evidence/documentation, NOT code defects) — all discharged

No emulator defects and no blocker-level regressions were found. The three sign-off conditions were
documentation/ledger corrections, all discharged by the coordinator on 2026-07-11:

- **C1 (H3) — DISCHARGED.** The report claimed `omx BLK.BIN = 2205b7b8 EQUAL`, but the committed
  `debug/m41/s4/H3/omx.dsk` is byte-identical to the bootable-blank starting image — openMSX wrote
  nothing (its BSAVE keystroke was dropped, independently re-verified: omx root dir empty, ours
  carries the BLK.BIN entry). H3 re-dispositioned honestly as **our-side correct + openMSX on-disk
  BLOCKED (keystroke-drop)**; the "EQUAL" line retracted in `debug/m41/ab/m41-ab-report.md`.
  Disk-WRITE parity remains proven by H1/H4/H5's SHA-equal round-trips.
- **C2 (ledger) — DISCHARGED.** Added `DEC-0059` (authorizes M41 + the additive JOY-verb src touch),
  an M41 block in `state/milestones.md`, and refreshed `state/current-phase.md` to M41. Noted the
  ledger was stale since M37 (v1.0.39 shipped between without its own milestone blocks — known
  documentation debt).
- **C3 (report) — DISCHARGED.** Folded a "DEF-M41-YJKOFFSET — RESOLVED" section into
  `debug/m41/ab/m41-ab-report.md` and flipped the S2 m10/m12 rows MISMATCH → MATCH (mirroring the
  PSGENV RESOLVED section).
- **Residual/accepted — YJK direction vs openMSX 21.0 source (LOW).** Our +4 fix matches the openMSX
  19.1 runtime oracle, fMSX 6.0 (`Common.h:732-737`/`778-783` draw the first 4 pixels as backdrop),
  and V9958 fact-sheet §5 J-in-bytes-3/4 packing — but disagrees with openMSX 21.0 source (which
  omits the latency). The two-reference-disagreement guardrail was followed (disagreement disclosed
  in-commit and in-code; the interpretation corroborated by two independent non-openMSX sources
  wins). Residual risk low and documented.

## 5. Sign-off Decision: **CONDITIONAL PASS → conditions discharged (effectively PASS)**

The emulator is technically release-ready: all four hard gates green; the three M41 commits are
correctly src-scoped with no cpu/core touch and no AI trailer; the two behavior fixes (YJK, PSGENV)
are independently verified with non-tautological regression guards; every KNOWN-DELTA and BLOCKED is
legitimately irreducible with no masked defect. The Conditional was driven solely by the three
non-code corrections above (C1–C3), all discharged 2026-07-11. None was an emulator defect or a
blocker-level regression. Discharging them required no gate re-run (documentation-only, no cpu/core
impact).

## 6. Release-Decision Recommendation

- **Version/tag → v1.0.40 (NOT folded into v1.0.39).** `v1.0.39` is already tagged and pushed
  (`origin/main` at `53e4eaf`), immutable. HEAD `6f6fb39` is **5 commits ahead**: the 3 M41 fixes
  (`3976bc4` JOY, `6044c19` YJK, `6f6fb39` PSGENV) + 2 ride-along features (`6ef652b` --fast-disk,
  covered by `devices_fdc_wd2793_fastdisk_unit_test`; `73ce71c` Right-Ctrl `:/*`, A/B-validated by
  keyboard scenario k2). New user-facing content (YJK visual correctness, PSG envelope audio
  correctness, --fast-disk, Right-Ctrl `:/*`, JOY test verb) → release as **v1.0.40**.
- **The actual tag + push are owner-run (DEC-0047).** Recommend the owner tag `v1.0.40` at `6f6fb39`
  and push; the auto-mode classifier blocks the assistant from the push.

## Evidence paths

- ZEXALL durable log: `debug/m41/zexall/m41-zexall-sweep.log`
- A/B report (with the C1/C3 corrections): `debug/m41/ab/m41-ab-report.md`
- Post-fix YJK captures re-diffed: `debug/m41/ab/s2fix/`
- Disk images re-extracted: `debug/m41/s4/H1..H5/{ours,omx}.dsk`
- Fixes: `src/devices/video/vdp_frame_renderer.cpp` (kYjkDisplayLead=4), `src/devices/audio/psg_ym2149.cpp` (envelope `count += generator_steps*2`)
- Ledger: `agent-protocol/channels/decisions.md` (DEC-0059), `agent-protocol/state/milestones.md` (M41), `agent-protocol/state/current-phase.md`
