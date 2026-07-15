# M34 QA Sign-off — PSG/SCC Box-Average Integration (Defect A) + R#1 BL Render Gate (Defect B)

**QA agent, independent verification. Baseline v1.0.34 → tag target v1.0.35. Working tree uncommitted.**
Every number below was reproduced/re-derived by QA, not taken from the implementation report.
(Persisted by the coordinator from RESP-M34-003; QA authored the content, returning it in-message
per the turn directive.)

## 1. Regression Scope

Two universal fixes: (A) PSG/SCC per-sample production switched from point-sampling to exact
integer box-average integration (`take_integrated_sample()` in `PsgYm2149`/`SccWavetable` +
`src/devices/audio/dwell_rounding.h`); (B) a BL (R#1 bit6) display-enable gate at the top of
`VdpFrameRenderer::render_line()` (`src/devices/video/vdp_frame_renderer.cpp:423`). Affected
subsystems: `src/devices/audio/`, `src/devices/video/`, `src/frontend/` (pump + mixer SCC term),
`src/main.cpp` (frame-dump-demo BL=1 fixture); 16-surface audio oracle re-baseline + 13-fixture
BL=1 re-grounding; committed frame/WAV evidence. Deliberately out of scope and verified untouched:
`src/devices/cpu/`, `src/core/`, `src/machine/`, AudioPacer, YM2413 FM path.

## 2. Regression Matrix Status

| Claim | Independent QA result | Verdict |
|---|---|---|
| Headless build 183/183 | 183/183, 0 failed (44.80s; post-mutation re-run 50.07s) | PASS |
| SDL3-ON build 192/192 | 192/192, 0 failed (50.31s); all 6 new M34 tests present | PASS |
| Rounding = round-half-away-from-zero, constants fixed points | Re-derived `(2s+sign(s)·W)/(2W)`; `round(L·W,W)==L` ∀L∈[-600,600] for W=81,16; ties away-from-zero | PASS |
| §2.4 sinc table | Re-derived: p=0/1 |H|=0.1252 (−18.1dB), p=2 0.1869, p=4 0.4601; alias 20,715 Hz | PASS |
| Ultrasonic p=0..4 bounds | Measured 2400/2400/2800/2400/7200 (L=R) vs 2500/2500/5100/2500/7400; control p=112 max 24800 / min 0 | PASS |
| Ultrasonic pre-fix discrimination | Mutation A (point-sampling): all five = 12,400 AC → 5 cases FAIL | PASS |
| Midband fidelity | p=254 ac_rms 6158.72==predictor; p=112 6105.57==predictor; pre-fix p=112 +1.55% FAILs | PASS |
| Aleste regression post-fix | L burst_blocks=0; R burst_blocks=0 (maxflips 3839=0.937 ZCR, RMS 2619); two-run identity | PASS |
| §2.5 re-baseline (anti-tautology) | Re-derived rows hand-authored & discriminating; zero-source oracles = exactly 0 (meaning kept); motion-is-a-bug rows byte-frozen & passing | PASS |
| Audio A/B silence parity | WAV genuine non-zero (max 6974, 76% nonzero, 13.0s — NOT vacuous); 139 blocks, 0 bursts, max RMS 3143 | PASS |
| BL gate grounding (openMSX + fMSX) | Read both; they AGREE (PixelRenderer.cc:608-611/:580-584, VDP.cc:437-441/:1080-1082; fMSX MSX.h:216 + Common.h ×12) | PASS |
| Pure-backdrop unit rows (5 modes + sprites) | bl_unit_test PASS; mutation B → 6 cases FAIL | PASS |
| L+1 latch (real seam, both directions) | m34_bl_latch_integration PASS; mutation B → FAIL | PASS |
| Accumulator BL=0 equivalence | Gate in shared render_line; content rows re-grounded BL=1 byte-preserving | PASS |
| S4 outcome (a): MG genuinely blanks | Probe: room transition f4173 line11→f4191 line189 (r1 0x62→0x22); openMSX ref f4175→f4201 | PASS |
| fmON/fmOFF supersession, FM peak | peak=3780 exactly, 105126 differing values; SHAs 79d28df0/75681cce match | PASS |
| m27 WAV/dump regenerated | SHAs 34d5a0b6 / f2ca85bc match; both changed vs v1.0.34 | PASS |
| asset-checksums.txt fresh | Regenerated content identical (timestamp-only) | PASS |

## Escalation verdicts

**Escalation 1 (rows 14-15) — CONFIRMED.** Aleste f3000 captured with the gate LIVE: rows 14-15 are
uniform pure backdrop (0,33,0); rows 13 and 16 carry real content. With the gate removed, f2900
recapture is byte-identical to the committed PNG (all 212 rows) — the reconstructed recipe is
faithful, so rows 14-15 diverge only because of the gate. Aleste genuinely drives BL=0 at display
line 13. The 915-transition count corroborated structurally.

**Escalation 2 (f3000 rows 99-117) — CONFIRMED.** Gate PRESENT: f3000 recapture differs at rows
14-15 AND 99-117. Gate REMOVED (audio-only M34): differs at rows 99-117 ONLY (19 rows). The
99-117 divergence exists independent of any M34 render change = pre-existing recipe/sprite-evolution
residue. M34-independent confirmed.

**Both coordinator conditions satisfied → provisional acceptance STANDS (not void). Closure NOT
blocked.** The four committed m32-aleste-play PNGs are byte-identical to v1.0.34 (not silently
rebaselined). Regeneration-with-supersession authorized (M32 AC-6 pattern).

## Adversarial mutation outcomes (all restored, hash-verified)

- Mutation A (integrated→point sample): ultrasonic FAILS (5/5) + midband p=112 FAILS. Restored OK.
- Mutation B (BL gate → if(false)): frame_renderer_bl FAILS (6) + bl_latch_integration FAILS;
  accumulator equivalence correctly unaffected (shared render_line). Restored OK.
- Discriminator (gate removed, Aleste recapture): drove escalation 2. Restored OK.
- Final full headless suite after restorations: 183/183. All mutated files sha256-verified to
  baseline. Final src/ diff vs v1.0.34 = 325 insertions / 35 deletions (no QA residue).

## 3. Failures and Risk Ranking

No blocker/major failures. No code fix required.
- Low/hygiene: `.claude/settings.json` out-of-cycle hunk — exclude from closure commit (precedent).
- Low/hygiene: the four m32-aleste-play PNGs divergent-but-correct — regenerate with supersession.

## Residual-risk register

1. **20.7 kHz post-fix residual** (R-M34-1): right channel ZCR 0.937 at RMS ~400-500 (~1.5% FS),
   sub-threshold, never coincident with high RMS. Box-filter remainder; owned by ledger E4;
   cascaded second box pre-authorized (RESP-M34-001 Q1) only on a human audible-residual report.
   Human ear / live play is the final acceptance signal.
2. **p=2-4 partial suppression** (R-M34-2): measured phase-locked peaks 2,800 (p=2) / 7,200 (p=4)
   PCM — honestly asserted as bounds, not false silence. Disclosed limitation, owned by E4.
3. **D8/D9 remainders**: mid-LINE BL precision (D8), BL=1-mid-frame sprite-table staleness (D9);
   pre-existing, no evidence-set title exercises them.
4. No live frame-rate measurement this cycle (headless evidence) — flag for the human's SDL3 session.

## 4. Required Fixes

None (zero code). Closure-hygiene only (coordinator-owned):
1. Regenerate `debug/frames/m32-aleste-play-f{2900,3000,3200,3600}.png` with supersession notes
   from a now-committed script (M32 AC-6 pattern); originals remain at tag v1.0.34.
2. Exclude the `.claude/settings.json` out-of-cycle hunk.
3. Apply DEC-0044 `git add -f` for all new tests/tools/docs/debug files (report §11 inventory).

## 5. Sign-off Decision

**CONDITIONAL PASS.** All M34 acceptance criteria independently verified: both defects fixed
universally with hardware-honest, reference-grounded behavior; both dual-config builds reproduced
(183/183, 192/192); every audio oracle re-derived/re-measured; the §2.5 anti-tautology discipline
holds; adversarial mutations kill the right tests and restore cleanly; the audio A/B is a genuine
non-zero capture; the BL gate matches both references at line granularity; both escalations
independently CONFIRMED (provisional acceptance stands — closure unblocked); hard constraints clean
(cpu/core + audio-path + machine diffs EMPTY, ZEXALL correctly withheld, license-isolated, nothing
game-keyed, evidence supersessions accurate incl. FM peak 3780, checksums fresh). The three
conditions are closure-hygiene actions with zero code impact, consistent with the M32 lineage.
