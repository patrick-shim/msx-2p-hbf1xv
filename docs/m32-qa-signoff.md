# M32 QA Sign-off — Raster-Accurate Per-Line Rendering + Line-Interrupt Delivery (Defect A) + FM Mix Calibration k=21 (Defect B)

- Milestone: M32 (DEC-0039 RC-playtest defect pair; charter REQ-M32-003; ratifications D-1/D-2 in RESP-M32-001)
- Baseline: v1.0.32 production candidate. Tag target: v1.0.33.
- Review surface: UNCOMMITTED working tree vs `git diff v1.0.32` + untracked new files.
- QA: MSX QA Agent, 2026-07-09. Every result below was **independently reproduced** — fresh
  builds (`build-qa-m32/`, `build-qa-m32-sdl/`), a scratchpad QA reproduction driver compiled
  against the fresh static library, and a temporary v1.0.32 git-worktree baseline core (built,
  used, removed) for same-script A/B. No developer artifact was taken on faith.

## 1. Regression Scope

- **Defect A** (renderer architecture + new interrupt source): `VdpScanlineAccumulator` (new),
  `VdpRenderSyncListener` seam in `V9958Vdp::io_write()`, machine write-hook (L+1 latch),
  `render_frame()` re-route, finalize in `on_vsync_boundary()`, O(1) line-interrupt poll in
  `step_cpu_instruction()`. Affects EVERY rendered frame and (for IE1 software) CPU-visible
  interrupt behavior — the highest-risk change class since M23. Regression matrix: full fast
  suites both configs, committed-frame-evidence byte-identity, IE1-off trajectory identity,
  determinism guards, openMSX A/B.
- **Defect B** (mixing policy): one constant `kFmAmplitudeScale` 5→21 in
  `src/frontend/machine_audio_mixer.h` (header-only). Regression matrix: zero-YM2413
  byte-identity oracle, saturation clamps, loudness ratio, fmON/fmOFF evidence pair.

## 2. Verification Matrix (claim → independent result)

| # | Claim (report/package) | Independent result |
|---|---|---|
| 1 | Headless fast suite 177/177 | **REPRODUCED**: clean-configure/build `build-qa-m32` (exit 0), `ctest -LE m24_slow_full_sweep` → **100% passed, 0 failed out of 177**, 41.73 s |
| 2 | SDL3-ON fast suite 186/186 | **REPRODUCED**: clean `build-qa-m32-sdl` (SDL3 prefix `build/_sdl3_install`, dummy drivers) → **100% passed, 0 failed out of 186**, 45.62 s |
| 3 | AC-1: zero `src/devices/cpu` + `src/core` diffs; ZEXALL correctly not run | **CONFIRMED**: `git diff v1.0.32 --stat -- src/devices/cpu src/core` empty. Fast-subset cadence honored per `feedback_slow_test_cadence.md` |
| 4 | Audio chain + legacy renderer untouched (`audio_pacer.*`, `psg_audio_pump.*`, `ym2413_synth.*`, `ym2413_opll.*`, `machine_audio_mixer.cpp`, `vdp_frame_renderer.*`) | **CONFIRMED**: path-scoped `git diff v1.0.32 --stat` empty for all; `src/main.cpp` also empty (temp probe genuinely reverted); `tests/unit/frontend/machine_audio_mixer_unit_test.cpp` (M29/M31 zero-SCC/zero-FM oracle host) empty |
| 5 | Line-match relation `(R#19 − R#23) & 0xFF`, never-occurs clamp, passed-moment→next-window | **VERIFIED against sources**: `references/openmsx-21.0/src/video/VDP.cc:518-576` (relation at :527-529, clamp :554-559, future-only arming :571-574) and FH-at-right-border :913-926. fMSX `references/fmsx-60/source/fMSX/MSX.c:2091-2107` corroborates the same screen-space relation (nuance: fMSX's coincidence constant is 2, i.e. it fires ~2 lines past match entry — inside the disclosed precision band; openMSX is primary and the live A/B measured delta 0). Also noted: openMSX sets FH even with IE1 off (VDP.cc:909-926) — our IE1-gated FH is the pre-existing disclosed M14 narrowing, correctly unchanged and re-disclosed in package §2.5 |
| 6 | Sync-before-change + LINE accuracy grounding | **VERIFIED**: PixelRenderer.cc:253-304 (sync before every update), :510-517 (VRAM), :549-571 (LINE rounding). Implementation is architecturally independent (row store + watermark vs openMSX rasterizer/sync-points) — **no structural transcription, license isolation holds** |
| 7 | FM k=21 derivation | **RE-DERIVED**: `Sony_HB-F1XV.xml` PSG `<volume>21000</volume>` / MSX-MUSIC `<volume>9000</volume>` (read directly); AY8910.cc:64-93 volumeTab peak 1.0 + :977-980 amplification 1.0; YM2413Okazaki.cc:48 (DB2LIN_AMP_BITS=8) + :159-165 (peak 255) + :1051-1054 (1/256). round(400×31×(3/7)/256) = round(20.76) = **21**. **QA bonus cross-check**: the XML selects `<ym2413-core>NukeYKT</ym2413-core>` for this machine — `YM2413NukeYKT.cc:914-917` returns the SAME 1/256, so the per-channel reading holds for the actually-selected core too |
| 8 | Loudness ratio 0.4335 (+1.16% vs 3/7) | **REPRODUCED**: `fm_peak=5376` (=256×21 exactly), `psg_peak=12400` (=31×400 exactly), `ratio=0.433548` vs reference 0.428571 |
| 9 | fmON/fmOFF pair: peak 3,780 = 900×21/5; mean-nonzero 1,094; 105,366 diffs | **REPRODUCED numerically** from the committed WAVs: first divergence at sample 0 of the trimmed 148,470-pair window, **105,366** differing values, **peak 3,780**, **mean-nonzero 1,094.29**. Audible-divergence property preserved at 4.2× the M31 level |
| 10 | Split A/B PARITY, boundary delta 0 | **RE-RUN LIVE** (`tools/openmsx-m32-split-ab.ps1` against real openMSX 19.1 on WSL, QA build + QA output paths): side A boundary **82**, side B boundary **82**, delta 0; comparator self-check FLAGS the corruption (42 breaks); reference-side IE1-off arm `fh=0`, zero breaks, no split. **DISPOSITION: PARITY**, independently reproduced end to end |
| 11 | AC-5 committed-evidence byte-identity | **REPRODUCED from scratch** (own driver, fresh build, own recipes): bios f210 (all three committed variants: `m31-rc-bios-f210.png`, bordered `boot-logo-after-f0210`, borderless `boot-logo-post-colorfix-f0210`), f300 (both `border-before/after-f0300` variants — stronger than the report, which only bracketed them), f320/f400/f520/f700/f1100/f2000/f3000 (vs the M31-era local dumps); DOS f200-f1000 (5/5) + `m31-rc-dos-f1000.png` + `c5-verify-settled.png`; Aleste f500/f900 (incl. committed `m31-rc-aleste-f900.png`; SPACE@300-314/600-614 recipe re-derived and byte-confirmed); MG2 f700/f1260/f1400/f2200 (incl. committed `m31-rc-mg2-f1260.png`); Metal Gear f500 + f750 (`konami-splash-after-f0750` committed PNG byte-identical); `m26-example-boot` covered by suite. **ALL byte-identical except the four escalated divergences** |
| 12 | A-M32-2: no evidence scenario enables IE1 | **REPRODUCED with per-instruction sampling** (stronger than boundary sampling): bios 3000 frames, DOS 1000, Metal Gear 1400, MG2 2200, Aleste 900 — `ie1_ever=0` in every run |
| 13 | Boot-logo timeline unchanged | **CONFIRMED** inside both suites (boot-logo integration test green, unmodified) |
| 14 | Determinism | Split system test run twice → identical pass (its own two-machine byte-identity is internal); M27 replay-determinism + M10 golden inside both green suites |
| 15 | Evidence gates | `tools/validate-assets.ps1` → **True** (7 BIOS, 3 ROMs); `tools/checksum-assets.ps1` regenerated to scratchpad → content identical to `docs/asset-checksums.txt` (timestamp-only delta); `docs/openmsx-ab-smoke.md` diff is timestamp-only with the historical R5 note intact |
| 16 | Aleste smoke (AC-9) | **SIGHTED**: `m32-aleste-play-f2900.png` = HUD row intact + non-garbage scrolling forest playfield + player ship + "AREA 1"; f3600 = advanced terrain, score 1280, enemy sprites. The human's repro is resolved |
| 17 | Universal-fix audit | **CONFIRMED**: no game-keyed logic in any M32-changed path (grep for title/SHA conditionals; the only Konami mentions are the pre-existing M29 mapper-type dispatch, device-class not game-class) |
| 18 | Ledger | D8/D9/D10 rows present and accurate (D9 carries the measured characterization); C10→M33-era / F1→M34-era / F2→M35-era shifts recorded with DEC-0039 citations |

## 3. AC-6 Arbitration — per-divergence verdict (the sign-off's center of gravity)

QA independently root-caused all four. **All four CONFIRMED as intended Defect-A raster-truth
changes. The coordinator's provisional acceptance STANDS.** None is an IE1/CPU-trajectory shift
(verdict row 12 above: per-instruction IE1 survey reproduced `ie1_ever=0` in every scenario).

### 3.1 `boot-logo-after-f0150-letters-sliding.png` / bios-f150 (committed) — **CONFIRMED**

- **Reproduced**: my fresh-build unattended-BIOS f150 dump is byte-identical to the developer's
  `m32-divergence-bios-f150-m32.frame`; its bordered render differs from the committed PNG;
  80 diff rows exactly at rows 32-111 (the logo-letter band).
- **Mechanism directly measured** (not inferred): instrumented per-instruction R#26/R#27
  sampling shows the BIOS rewriting the horizontal-scroll pair EVERY DISPLAY LINE during the
  slide — frame 150 alternates (R#26=9, R#27≈7) ↔ (R#26=56, R#27=1) at successive display
  lines 30, 31, 32, ... (the DEC-0031 wobble, `docs/border-and-boot-logo-investigation.md`).
  The legacy snapshot flattened the frame to the end-of-frame scroll; raster-true rendering
  shows each line at the scroll in effect when the beam passed → interleaved two-position comb.
- **Direction corroborated by real openMSX**: the COMMITTED DEC-0031 reference
  `debug/frames/boot-logo-openmsx-reference-t2500ms.png` (same boot phase) shows the SAME
  per-line interleaved comb pattern on the logo letters. The M32 output moves TOWARD the
  openMSX reference; the old clean mid-slide logo was the flattening artifact.
  (openMSX LINE-accuracy model re-read: PixelRenderer.cc:253-264 scroll sync.)

### 3.2 `m31-rc-metalgear-f1100.png` (committed) — **CONFIRMED** (D9-class)

- The M31 capture recipe is genuinely unrecovered (disclosed by the developer; QA verified:
  the developer's same-script `m32-divergence-metalgear-f1100-v1032.png` does NOT byte-match
  the committed PNG, while the local `m31-rc-metalgear-f1100.frame` IS the committed PNG's
  exact source). Root cause therefore rests on same-script A/B pairs, which QA verified at TWO
  independent input trajectories:
  - Developer's pair (verified numerically): 17 diff rows at 89-105, x-extent 68-83 — one
    16-px-wide sprite band; f1400 pair: 14 rows at 95-108, x 131-142.
  - **QA's own second trajectory** (unattended script, own v1.0.32 worktree baseline vs own
    M32 build): f1100 IDENTICAL, f1400 an 8-row band at rows 168-175 — the same class appears
    at trajectory-dependent frames, always sprite-band-sized, never background/global.
- **Verdict**: divergence class confirmed (sprite-band-only, background byte-identical),
  mechanism proven quantitatively in 3.4 below. Regeneration at closure requires a NEW,
  RECORDED recipe + supersession note (the old bytes are unreproducible by anyone).

### 3.3 `metalgear-f1400` + 3.4 `mg2-f1700` (local artifacts) — **CONFIRMED** (D9-class, mechanism proven)

- MG2 f1700 fully anchored: QA's v1.0.32-worktree baseline capture is byte-identical to the
  developer's local `m31-rc-mg2-f1700.frame` (recipe = unattended, baseline faithful), and
  QA's fresh-build M32 capture is byte-identical to `m32-divergence-mg2-f1700-m32.png`.
  Diff: 12 rows at 76-95, x-extent 178-192 — one sprite band, exactly as reported.
- **One-frame sprite-lag mechanism proven pixel-exactly**: of the 28 differing pixels in the
  M32 f1700 frame, **28/28 (100%) equal the v1.0.32 baseline's PREVIOUS frame (f1699) content**
  at those positions, and 0/28 are static content (baseline f1700 ≠ f1699 at all 28). This is
  the package's own §1.4-item-2 predicted remainder measured exactly: rows committed mid-frame
  composite sprites from the previous boundary's visibility table (named backlog row D9);
  projected rows are exact.

## 4. Adversarial Test-Strength Checks (mutations; each restored and re-proven green, hashes verified)

| Mutation | Expected | Result |
|---|---|---|
| Latch at L instead of L+1 (`sync_to_line(line)`) | latch test fails | **KILLED**: `HookedWriteAtLine100_RowsThrough100_KeepPreWriteScroll` fails — exactly the discriminating case |
| Line-interrupt fire disabled (`on_line_match()` call removed) | split system test fails; IE1-off arm still passes | **KILLED**: 4 split cases fail (`SplitRun_HandlerObservedS1FH…`, `…SplitBoundaryExists`, `…BoundaryWithinPrecisionBand`, `…Offset128`), the `AdversarialIe1Off_*` arm still passes; line-int integration test fails 7 cases |
| `kFmAmplitudeScale` 21→5 | loudness-ratio test fails | **KILLED**: ratio 0.103226 vs gate [0.364, 0.493] — fails; plus both FM-alone rail-clamp cases fail |

After restoration (SHA-256 verified against pre-mutation hashes) all five M32 tests pass; the
split system test was additionally run twice back-to-back for determinism.

## 5. Deviation Judgments

1. **Finalize AFTER `vdp_.on_vsync()`** (package said before) — **SOUND, required**.
   `on_vsync()` recomputes sprite visibility from the frame's own end-of-frame VRAM and
   advances R#13 blink; the legacy pipeline that produced every committed frame rendered
   post-recompute, and real hardware fetches attributes live during the frame (vblank-written
   attributes for frame F are visible during frame F). QA's empirical proof: the full AC-5
   matrix reproduces byte-identically under this ordering, and the §3.4 one-frame-lag
   measurement shows staleness confined to mid-frame-COMMITTED rows (the declared D9
   remainder) — under the package's literal ordering it would afflict ALL projected rows of
   every moving-sprite frame, violating AC-4/AC-5.
2. **Even/Odd fields keep legacy snapshot semantics** — **SOUND, low risk**. Verified no
   production caller: `sdl3_app.cpp:239` and `main.cpp:357` render Progressive only;
   `Field::Odd` appears in production only inside the untouched legacy renderer.
3. **Split boundary band [S, S+4) instead of ±2 prose** — **SOUND**: measured 82 on both
   sides (A/B delta 0, better than the gate).
4. **debug_io_write exclusion tested at integration level** — correct layer (machine seam).
5. **`--m32-evidence` probe reverted** — `git diff v1.0.32 -- src/main.cpp` empty.
6. **`cold_boot()` `last_vsync_cycle_ = 0`** (latent-since-M23 fix, first load-bearing now) —
   reviewed; correct (scheduler restarts at 0; all vsync-relative derivations must share the
   origin); covered by both green suites.

## 6. Failures and Risk Ranking (findings)

No blocker or major defects found. Findings, ranked:

1. **[Low — commit hygiene, protected file] `.claude/settings.json` is in the diff surface**
   (one-character model-name typo fix `"sonnect"`→`"sonnet"`). Not M32 scope; agents must not
   modify this file. Almost certainly a human edit, but attribution is unrecorded. Must be
   attributed (or excluded/separately committed) at closure — not folded silently into M32.
2. **[Low — provenance] `references/music_in_basic.md`** (untracked, MSX-MUSIC BASIC usage
   notes) has no recorded provenance in any channel. Attribute (human-provided?) or exclude
   from the closure commit.
3. **[Info] Metal Gear committed-PNG regeneration is recipe-less**: the committed
   `m31-rc-metalgear-f1100.png` bytes are unreproducible on ANY tree (M31 recipe lost). The
   closure regeneration must use a NEW recorded input script and a supersession note; adopt
   the developer's own process note (record exact input scripts with every future capture).
4. **[Info] fMSX citation precision**: MSX.c:2094 fires at coincidence value 2 (≈2 lines past
   match entry), not 0 — "algebraically identical relation" is right for the match-line
   arithmetic, with a latency constant nuance inside the disclosed precision band. openMSX
   (primary) + the delta-0 A/B are dispositive. No action.
5. **[Info] `agent-protocol/state/deferred-backlog.md` acquired a UTF-8 BOM** (PowerShell
   write). Cosmetic encoding churn.
6. **[Info] `debug/sounds/m32-fm-aleste-*.dump`** exist locally but only the `.wav` pair has
   gitignore exceptions; the report cites `{dump,wav}`. Consistent with local-only dumps; no
   action required.

## 7. Required Fixes (conditions — all closure-commit hygiene, none are code defects)

1. Coordinator records attribution for the `.claude/settings.json` edit (decisions/closure
   note) or keeps it out of the M32 closure commit (finding 1).
2. Coordinator records provenance for `references/music_in_basic.md` or excludes it from the
   closure commit (finding 2).
3. The AC-6 regenerations (contingent on this sign-off, per REQ-M32-003): regenerate
   `boot-logo-after-f0150-letters-sliding.png` and `m31-rc-metalgear-f1100.png` with
   supersession notes; the Metal Gear regeneration MUST record its input script (finding 3).

## 8. Residual-Risk Register

| Risk | Severity | Disposition |
|---|---|---|
| D9: one-frame sprite lag in mid-frame-committed rows (measured: 8-17 rows, sprite-band-sized) | Low | Named backlog row D9 with measured characterization; per-line live sprite fetch is the refinement |
| D8: sub-line accuracy (±1-line poll granularity; mid-line effects) | Low | Named row D8; A/B measured delta 0 at line granularity |
| D10: mid-frame geometry-change adaptation policy | Low | Deterministic + crash-safe; no title in evidence set exercises it |
| k=21 perceptual acceptance (R-M32-7) | Low | Reference-honest derivation + numeric evidence; the human's ear on `m32-fm-aleste-fmON.wav` is the final signal |
| Interactive frame-rate under the per-step poll + write hook (R-M32-3) | Low | O(1) design verified in code; headless suites show no measurable slowdown (41.7 s / 45.6 s, normal range); no live measurement this cycle — flag for the human's next session |
| Evidence-recipe fragility (unrecorded input scripts pre-M32) | Process | Required fix 3 adopts script-recording going forward |
| IE1-gated FH narrowing (pre-existing M14, openMSX sets FH regardless) | Info | Unchanged this cycle, re-disclosed in package §2.5; a title polling FH with IE1 off would be the trigger to revisit |

## 9. Sign-off Decision

**CONDITIONAL PASS** — conditions are the three §7 closure-hygiene actions only (attribution ×2,
supersession-noted regeneration ×1). Zero code changes required.

- Both defect fixes are verified genuine, universal (never game-keyed), reference-grounded,
  and license-clean.
- Full fresh-build regression reproduced: headless **177/177**, SDL3-ON **186/186**.
- The AC-6 provisional acceptance is **confirmed for all four divergences** — each
  independently root-caused (one with a 100%-pixel-exact one-frame-lag proof, one with the
  openMSX reference PNG corroborating the tearing direction); the intended raster-truth
  behavior change stands and the affected evidence may be regenerated per REQ-M32-003.
- All new oracles proven discriminating by mutation (3/3 killed, restored, re-proven green).
- openMSX A/B split parity independently re-run live: delta 0.
- Hard constraints intact: zero cpu/core diffs (ZEXALL correctly not run), audio chain and
  legacy renderer byte-untouched, probe reverted.

Upon completion of the §7 items, QA recommends closure and tag **v1.0.33**.
