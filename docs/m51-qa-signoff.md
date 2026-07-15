# M51 QA Sign-off — Sprite disappearance/flicker during scroll (DEC-0078)

- Milestone ID: M51 (`docs/m51-planner-package.md`; developer artifact `docs/m51-implementation-report.md` incl. §10 canonical attestation)
- QA Owner: MSX QA Agent (independent re-verification session, owner pre-authorized autonomous run)
- Date: 2026-07-15 (verification executed ~03:45-04:15 KST; this file written immediately after the last command)
- Change under test: commit `278af86` on `main` (local-only, NOT pushed; coordinator-ratified) —
  `src/devices/video/v9958_vdp.{h,cpp}` + `src/machine/hbf1xv_machine.cpp`, +228/−5
- Verdict: **PASS** — release remains owner-gated (NORMAL human-release gate, HIGH blast radius; see §5)

**Supersedes note:** a prior `docs/m51-qa-signoff.md` (file mtime 03:38, header self-dated 04:05 —
a forward-dated timestamp, recorded here as an evidence-hygiene defect of that document) existed
before this session. Per the never-trust-re-run charter and the dual-session incident history, NOTHING
was carried forward from it: every gate below was independently re-executed or re-read from raw
artifacts in THIS session. Where comparable, the prior document's numbers were corroborated. QA
artifacts of this session: `debug/m51/qa2/` (captures, oracles, EG-3 subset, EG-8).

QA discipline: non-destructive (DEC-0049) — the adversarial mutation used `cp` backup/restore
(never `git checkout`/`restore`/`stash`), verified byte-identical by SHA256 + `git status` clean
before any result was recorded; single `build/` tree throughout (DEC-0041).

---

## 1. Regression Scope

- **Change surface (QA code review of `git show 278af86`):** the ONLY behavior-affecting
  addition is the consumer-side sprite-pacing call pair — new `V9958Vdp::commit_sprite_rows(target)`
  (advance-only-when-active, render-only, clamped `check_until_visible_only(target-1)`) invoked
  from the M44 command-row sink `on_commit_up_to()` BEFORE `sync_to_line()`, plus the identical
  guard on the two `render_frame()` memoization syncs. Everything else in the diff is env-gated
  M51 trace plumbing (`SONY_MSX_M51_SPRITE_TRACE`/`SONY_MSX_M51_TRACE_BAND`, default-off).
- **MUST-NOT-TOUCH list verified respected:** `sprite_engine.*` absent from the commit entirely
  (`begin_frame` clear-at-open retained — shape (i) only); no DEC-0031 collision-path edits; no
  M48 timing constants; no per-line formula / mode-2 mask edits; the 5 deleted lines are the
  stale "no command-row sink drives the sprite watermark" comments the fix obsoleted.
- **Blast radius (HIGH):** all sprite rendering, all 13 screen modes, DEC-0031 collision path,
  M44/M48/M49 render-sync seams, determinism → NORMAL human-release gate, no auto-close.
- **Grounding audit:** fix contract cites tier-2 openMSX consumer-side sync
  (`references/openmsx-21.0/src/video/PixelRenderer.cc:580-584`, `SpriteChecker.hh:242-247`) as
  EFFECT only, tier-3 fMSX fused per-scanline selection (`references/fmsx-60/source/fMSX/Common.h:99-155`)
  as triangulation, tier-1 fact-sheet §6 (per-scanline fetch model, ≤8 sprites/line) as the hardware
  behavior modeled — all citations verified to be comments, not code (EG-9 below). No tier conflict;
  precedence order respected.

## 2. Regression Matrix Status

### 2.1 Integrity (dual-session / foreign-writer hazard)

| Check | QA result (this session) |
|---|---|
| `git status` | clean at `278af86`; branch ahead of origin by exactly 1 (local-only commit) |
| `git show 278af86 --stat` | exactly 3 files, +228/−5 — the DEC-0078 expected-edit locus, nothing wider (EG-11) |
| Landed source hash | `v9958_vdp.cpp` SHA256 `075e8c3e8643d608…` — matches the sha-stamped deliverable recorded at landing |
| tests/ tamper audit | pre-existing test files carry ONE bulk mtime (00:16 — repo-wide event also covering tracked `tools/` files, consistent with the bisect tag checkouts); only `tests/CMakeLists.txt` (02:44, verified: exactly the 2 new target registrations) and the 2 new test files (03:17) are later. Content grep: NO pre-existing test references M51/`commit_sprite_rows`. All 242 pre-M51 oracles pass unmodified in QA's own runs |
| Rogue-session quarantine | `debug/m51/quarantine-rogue-session/` (incl. `canonical-landing-quarantine/`) and `debug/m51/bisect/README-QUARANTINE.md` present; contaminated legs (`bisect/v1.1.4` non-redo) superseded by `-redo` legs |
| Prior sign-off | forward-dated (03:38 mtime vs 04:05 header) — superseded by this document; its measurable claims were independently corroborated where re-checked |

### 2.2 Evidence gates — QA re-measured vs claimed

| Gate | QA method (this session) | QA result | Claimed | Match |
|---|---|---|---|---|
| EG-1 build | `tools/bootstrap-build.ps1 -RunTests` + full rebuild after the mutation cycle | both exes, zero errors/warnings surfaced | zero errors | YES |
| EG-2 fast ctest | run TWICE (bootstrap; final post-restore attestation `-LE m24_slow_full_sweep`) | **244/244** both runs (242 pre-M51 + 2 new); sprite/M22/M32/M44/M48/M49 oracle-wall membership confirmed in the passing log | 244/244 | YES |
| EG-3 13-mode A/B | stored `debug/m51/m41-ab/m51-eg3-report.md` read (13/13 rows 0.000% MATCH) + **QA live re-run of 3 modes** (`m2_g2`, `m6_g5` 512-wide `-doublesize`, `sp2_double`) vs WSL openMSX 19.1 → `debug/m51/qa2/eg3/` | **3/3 re-run 0.000% MATCH**; 13/13 stored report internally consistent | 13/13 0.000% | YES |
| EG-4 collision (DEC-0031) | via EG-2 + source read of the new S#0 assertions | `devices_sprite_engine_collision_relatch_unit_test` + boot-logo + M22 oracles green unmodified; unit case 6 (`peek_status_register(0)` byte-equal across split-open+pacing) and integration case 3 (S#0 == no-command reference) are genuine, non-tautological assertions | byte-identical | YES |
| EG-5 / AC-F1 post-fix | **fresh canonical-binary captures** (`tools/m51-capture-frames.ps1` + SAT census + oracle) → `debug/m51/qa2/` | **Firebird f1800-59: present 60/60, 0 transitions, verdict PASS** (SAT in 60/60, max 4/line); **Aleste 2 f3560-99: present 40/40, 0 transitions, PASS** (SAT in 40/40, max 4/line) | 60/60 & 40/40, 0 transitions | YES |
| EG-5 pre-fix baselines | re-computed from raw stored CSVs | Firebird f1800-59: 27/60 absent, 53 transitions; Firebird f2000-39: 8 absent, 16 transitions; Aleste f3560-99: 40/40 absent | same | YES |
| openMSX reference rate | re-computed from raw CSVs | Firebird omx f1800-99: 100/100 present, **0 transitions** → post-fix rate (0) == reference rate (0), AC-F1 satisfied in BOTH directions (R1) | 0 | YES |
| Authentic-blink guard | per-frame count comparison + omx leg | Aleste idle f2900-39: pre-fix vs post-fix per-frame signature counts **byte-equal** (95↔53 blink preserved); openMSX leg shows the IDENTICAL alternation pattern → authentic, correctly NOT "fixed" | byte-equal | YES |
| EG-6 determinism | capture-tool dual runs + cross-evidence hashing | f1800/f1859/f3560/f3599 dual-run byte-identical; QA frames **byte-identical to stored post-fix evidence** (3 sampled Firebird frames SHA-equal) and to the report's hashes (`7a89d434…` f3560 reproduced by the final relinked exe; `91545ba7…` f1800) | byte-identical | YES |
| EG-7 assets | re-ran both tools | validate-assets **PASS** (7 BIOS + fmpac.rom); checksum re-run differs from stored file ONLY in the "Generated at" line — asset checksums unchanged | PASS | YES |
| EG-8 trace hygiene | QA dual run, Firebird f1800 | trace-on (54.7 MB trace written) vs trace-off frame dump **byte-identical** (`91545ba7…` both) | byte-identical | YES |
| EG-9 ban attestation | `grep src/` SpriteChecker/VDPAccessSlots/PixelRenderer | comment-level grounding citations ONLY; no code/table transcription | comments only | YES |
| EG-10 ZEXALL | `git diff v1.2.0..HEAD -- src/core src/devices/cpu` | **EMPTY** → ZEXALL correctly withheld per the standing slow-test cadence | empty | YES |
| EG-11 scope | commit stat + tools audit | 3 src files only; `tools/m51-ab-run.ps1`/`m51-m38-run.ps1` diffed vs originals → ONLY the parameterized `-Headless` path (CR-ending noise aside); `tools/m51-eg3-run.ps1` read in full (clean) | confined | YES |

### 2.3 Non-tautology — adversarial revert EXECUTED live by QA

`commit_sprite_rows()` stubbed to an immediate `return` (cp-backup first), targets rebuilt:

- `devices_v9958_command_row_sprite_pacing_unit_test` → **FAILED, 6 cases** (watermark stuck at 20,
  line 105 empty, wrap-clamp cases — the exact fix-contract asserts);
- `machine_hbf1xv_m51_command_sprite_pacing_integration_test` → **FAILED, 2 cases**, with the
  failure diff naming the mechanism itself: rows 101-108 first-diff at x=100, `cmd=0x0 ref=0x7fff`
  — the sprite band sealed sprite-less, i.e. the pre-M51 defect reproduced on demand.

Restore from backup → SHA256 byte-identical (`075e8c3e…`), `git status`/`git diff` clean, targets
rebuilt, both tests green, then the FULL fast suite re-run: **244/244**. This also proves the
incremental build tracks these exact sources (the §10 stale-object hazard does not persist).
The mechanism-reproduction case (`M51Mechanism_LazyOpenClear_Line105PastWatermark_Empty`) and the
integration test's non-vacuity guard (`Reference_SpriteBandRow_DiffersFromBackgroundRow`) make the
oracles falsifiable in both directions.

### 2.4 Root-cause / bisect / guard audit (from raw artifacts)

- **Bisect (redo legs, re-computed from `debug/m51/bisect/*-redo/oracle.csv`):** v1.1.4 32/32
  present · v1.1.5 32/32 present · v1.1.6 **14/32 absent, 28 transitions** → first-bad **v1.1.6
  (M49)**, M48/"adjustable VDP timing" eliminated; attribution (a)/H1. Contaminated non-redo leg
  properly quarantined with a provenance README.
- **Smoking-gun traces verified verbatim:** `debug/m51/traces/aleste2-f3560.trace` frame F3559:
  `CMDROW dy=96 r23=121 display_line=231 raster=67` → `CMDCOMMIT range=[69,231) sprite_wm=69` →
  28/28 `BAND-ROW line=148..175 visible=0 past_sprite_wm=1` (2117 such empty-composite events in
  the file; NO `CMDSPRITE` events → confirms pre-fix provenance of the trace binary).
  `debug/m51/traces/firebird-f1801.trace`: 221 wholesale-seal events (empty_rows ≥ 100) incl. the
  claimed `CMDROW dest=217 acc_wm=6 sprite_wm=6 empty_rows=206 raster=4`.
- **§1.6 authentic-multiplex guard — all three prongs re-verified:** (1) SAT census max 4
  sprites/line on both titles (mode-2 hardware limit 8, tier-1 fact-sheet §6) — under-limit;
  (2) openMSX same-window rate 0 (Firebird) / identical blink pattern (Aleste idle);
  (3) whole-band emptiness shape (the BAND-ROW trace). The per-line cap is untouched by the diff,
  so genuine >8/line multiplex flicker is preserved by construction.

## 3. Failures and Risk Ranking

No blocking failures. Findings and residuals:

| # | Severity | Finding / residual | Disposition |
|---|---|---|---|
| 1 | Medium | **Laydock 2 = probe-only repro** (title/credits + menu probes under `debug/m51/laydock2-probe/`; gameplay not scripted). Package R7 explicitly makes it the SHOULD/"if workable" leg; the mechanism is title-independent (proven on both MUST titles + the machine-level integration oracle) and Laydock 2 is a scroll-blit title of the same class. | ACCEPTED under R7 as-written; **owner live spot-check of Laydock 2 is a release condition** (§5). |
| 2 | Low | **M49 +2-line quantization ceiling stands** (cycle-timed intra-line effects stay line-quantized; the openMSX per-slot position table remains banned/unbuildable-without-fabrication). M51 adds pacing INSIDE the existing seams; no timing constant changed → regression risk vs pre-M51: none beyond the pre-existing accepted boundary (M44 Phase 2b / DEC-0069-OUTCOME). | Accepted documented boundary; unchanged. |
| 3 | Low | **1-frame-stale fallback**: rows sink-committed while the split is not yet open render the previous vsync recompute (lag at worst, never absence) — the documented §2.1 contract and exactly the pre-M49 behavior. Task 2 traces show every failing CMDCOMMIT ran split-active, so pacing never needs to lazy-open (advance-only-when-active is evidence-grounded, and the unit test's case 3 locks the no-op contract). | Accepted; document-only. |
| 4 | Low | **v1.1.7 phosphor persistence**: shipped as a softener for what M51 proves was THIS defect (the "genuine 8-sprite flicker" attribution is corrected by evidence). Its default is already **0 = OFF** (CLI `std::nullopt` → 0; shipped `sony_msx_hbf1xv.xml` `percent="0"`), so no default change is needed. Recommendation (no code): keep default off; retain the feature + `peak` mode as a legitimate presentation option for genuinely multiplexed scenes; note the corrected attribution in the next ledger/README touch. | Recommendation only. |
| 5 | Low | **Dual-developer-session incident** (report §8/§10): commit `278af86` was created despite the leave-uncommitted instruction; coordinator quarantined the rogue variant, landed the sha-stamped verified set exactly, and ratified the commit as a data-safety measure. QA evidence-integrity assessment: NO residual impact — deliverable hashes match the landed files, contaminated bisect legs were quarantined and redone, and this session's canonical captures are byte-identical to the stored post-fix evidence. Protocol note for the coordinator: the prior QA sign-off's forward-dated header (04:05 vs 03:38 mtime) is a documentation-honesty defect of that artifact; superseded here. Standing advice re-affirmed: close any other live automation session before further work. | Protocol note; no code action. |
| 6 | Low | Authentic flicker remains wherever real hardware shows it (>8/line multiplex; the Aleste idle blink) — openMSX-corroborated, correct per tier-1 spec. | Not a defect. |
| 7 | Low | A/B harness runs against WSL openMSX **19.1** while the reference source tree is 21.0 — the established practice of every prior A/B milestone (M38/M41). | Accepted, noted. |
| 8 | Info | `debug/m51/ab/m41-ab-report.md` row-overwrite hygiene (rewrite-not-append) and the Aleste 2 f3560-window openMSX leg RNG divergence remain honestly-documented evidence caveats; neither affects the verdict (Aleste classification rests on SAT prongs 1+3; cart_g2 was re-proven live). | No action. |
| 9 | Info | New M51 tests live in gitignored `tests/` (repo-wide convention) — local-only, same as every prior milestone. | Standing convention. |

## 4. Required Fixes

None blocking. No code changes requested. Non-blocking follow-ups: the §5 release conditions;
finding #4's documentation note on the corrected v1.1.7 attribution.

## 5. Sign-off Decision

**PASS.**

- AC-T0/T1/T2 verified from raw artifacts (repro CSVs, redo-bisect CSVs, verbatim traces, H1-H5
  dispositions); AC-F1 re-measured by QA from the canonical binary (both MUST titles 100% present,
  0 transitions, == the openMSX reference rate, both directions); AC-F2 held (oracle wall 244/244
  unmodified, collision S#0 byte-identity asserted directly, idle-window pre==post byte-equal);
  AC-F3 proven live (adversarial revert executed: 6+2 case failures, restored byte-identical).
- No existing oracle weakened; no gate skipped; every number in §2 was re-measured or re-computed
  from raw evidence in this session.
- **Release conditions (protocol, not code deficiencies — NORMAL human-release gate, NO auto-close):**
  1. **Owner live spot-check of Aleste 2, Firebird, AND Laydock 2** (Laydock 2 discharges residual
     #1), launched WITHOUT the phosphor softener so residuals cannot be masked: persistence
     defaults to **0 (OFF)** — omit `--persistence` (or pass `--persistence 0`), and if a
     `sony_msx_hbf1xv.xml` is auto-loaded confirm its `<persistence percent="0">` is unedited.
     Suggested: `build/Debug/sony_msx_sdl3.exe --bios-dir bios/ --slot1 <rom> --disk-writable`.
  2. Push/tag (target v1.2.1) only after the owner gate — `278af86` is currently local-only.

— MSX QA Agent (independent re-verification), 2026-07-15
