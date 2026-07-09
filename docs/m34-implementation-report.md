# M34 Implementation Report — PSG/SCC Box-Average Integration (Defect A) + R#1 BL Render Gate (Defect B)

- Milestone: M34 (DEC-0043 playtest defect pair; charter `docs/m34-planner-package.md`;
  coordinator ratifications Q1 = single box filter / Q2 = regenerate m27 WAV, RESP-M34-001).
- Baseline: v1.0.34 + commit daad6b8 (DEC-0044). Tag target: **v1.0.35**.
- Author: MSX Developer Agent, 2026-07-09. Working tree UNCOMMITTED for QA/coordinator
  (standing rule; note the DEC-0044 `git add -f` discipline for new files under
  tests/, tools/, docs/, debug/ — full inventory in §11).
- ZEXALL/ZEXDOC: correctly NOT run — `git diff v1.0.34 -- src/devices/cpu src/core` is EMPTY
  (§7); fast-subset cadence per `feedback_slow_test_cadence.md`.

---

## 1. What shipped (by slice)

### S1 — Chip-internal dwell integration (additive; zero behavior change)

- NEW `src/devices/audio/dwell_rounding.h`: THE shared rounding helper,
  `round_div_half_away_from_zero(sum, window)` = `(2*sum + sign(sum)*window) / (2*window)` in
  exact int64 (the §2.3.4 recorded form; fixed-point property `round(L*W, W) == L` unit-proven
  across [-600, 600] for windows 81 and 16).
- `src/devices/audio/psg_ym2149.{h,cpp}`: `advance_cycles()` rewritten as the §2.3.1 dwell walk
  (head partial step from `cycle_residual_`, whole 16-cycle generator steps, tail partial;
  generator-state evolution IDENTICAL to the old bulk advance — proven by the untouched chip
  suites and the full fast suite), accumulating per-channel `Σ level×dwell` into three uint64
  integrals with `level_ch = channel_audible(ch) ? resolved_amplitude(ch) : 0`. NEW
  `take_integrated_sample(window_cycles)` → `{round(intA+intB, W), round(intA+intC, W)}`
  (stereo law unchanged), resets integrals; W==0 → {0,0} (§2.3.5). The §2.3.3 boundary
  convention (a step completing at cycle t changes the level AFTER t) is documented in the
  header together with the FULL §2.4 sinc-response table and the honest partial-suppression
  statement (p=2/4 keep audible-band aliases; E-series pointer). Point `sample()` byte-kept.
- `src/devices/audio/scc_wavetable.{h,cpp}`: same pattern per channel at its own
  `(period+1)`-master-cycle step boundaries; a stopped channel (period ≤ 8) contributes
  `out × delta` exactly; enable gating matches `sample()` (disabled channel contributes 0,
  phase keeps running); one mono int64 integral; NEW `take_integrated_sample()`. Point
  `sample()`/`amp_out()` byte-kept; NYYRIKKI latch semantics unchanged (out_ refresh at
  position steps).
- Additive chip-level unit cases (dwell math authored in comments BEFORE execution, R-M34-9):
  PSG period-1 W=81 hand oracle (dwells 33/47/35/45/37/43 → samples **13,18,13,17,14,16**,
  cross-checked in-test against an independent per-cycle level model), constant fixed points
  ({48,40} at W=81 AND the tie-prone W=16), envelope-mid-window segmenting (31×32+30×32+29×17
  = 2445 → **30**, while the point endpoint is 29 — the integral provably differs from the
  endpoint), W=0 guard + discard semantics, take-never-perturbs-state twin; SCC stopped-channel
  −1 fixed point, period-9 ramp (integral 10×(0+7+15+22+30+37+45+52)+60 = 2140 → **26**, final
  pos 8), disabled-running-channel zero, constant-wave 60 fixed point, W=0 guard. NEW
  `tests/unit/devices/audio_dwell_rounding_unit_test.cpp` (exact ±ties, fixed-point property,
  W=1 identity).
- **S1 gate: 187/187 fast subset green (46.45 s), production still point-sampling — every
  pre-existing test byte-untouched at that point.**

### S2 — Production switch + §2.5 oracle re-baseline (executed table in §2)

- `src/frontend/psg_audio_pump.cpp`: `pump_one_sample()` = advance exactly W, then
  `take_integrated_sample(W)` (satisfies the take-API precondition by construction; W==0 M26
  idle case exactly silent via the chip guard). Signature unchanged.
- `src/frontend/machine_audio_mixer.cpp`: SCC term = advance + `take_integrated_sample(W)`.
  FM branch byte-untouched. `machine_audio_mixer.h` / `psg_audio_pump.h` / `psg_audio_dump.h`
  contract comments re-grounded (M34 notes; the M29/M31 zero-source oracles' meaning).
- Consumer-grep sweep (§2.5 final obligation) executed and reconciled: every
  `pump_one_sample|pump_samples|mix_interleaved_stereo|sample(|amp_out(|fm_sample(` consumer in
  src/ and tests/ maps to a §2.5 row or an M34-new file; `sdl3_audio_presenter.cpp` (the only
  other mixer consumer) is diff-EMPTY. No unlisted consumer found.
- **S2 gate: 190/190 fast subset green (56.99 s; +10.8 s of that is the new Aleste system
  test). R-M34-8 wall-time note: heaviest pre-existing audio test post-switch =
  `machine_hbf1xv_m31_metalgear2_scc_integration_test` at 11.55 s (frame-loop-dominated,
  normal M31-class range); no per-test blow-up observed.**

### S3 — New audio oracles + Aleste regression + audio A/B + evidence re-capture

- NEW `tests/unit/frontend/machine_audio_mixer_ultrasonic_silence_unit_test.cpp` — results §3.
- NEW `tests/unit/frontend/machine_audio_mixer_midband_fidelity_unit_test.cpp` — results §3.
- NEW `tests/system/hbf1xv_m34_aleste_ultrasonic_regression_system_test.cpp` — the recorded m32
  smoke recipe (KonamiSCC explicit; SPACE stamps (f−1)×59736, the committed metalgear-script
  stamp convention), window-restricted production frames 2150–2350 (recorded runtime choice),
  §2.6.4 ZCR burst metric, two-session byte identity. Results + the recorded PRE-fix baseline
  in §3.
- NEW `tools/openmsx-m34-aleste-audio-ab.ps1` + `tools/m34-audio-ab-analyze.py` +
  `docs/m34-audio-ab.md`: **disposition PARITY (silence property)** — real openMSX 19.1/WSL
  `record start -audioonly` capture of the same transition (A-M34-3 VERIFIED live; the §2.7
  fallback was NOT needed) shows ZERO burst blocks with real music present (max AC_RMS 3,143 at
  ZCR ≤ 0.231), while our committed regression oracle is green. Bring-up finding recorded
  honestly in the doc: the first side-B attempt used a wrong `keymatrix down` (two-word) Tcl
  command, silently producing an all-zero capture whose "silent" verdict was VACUOUS — caught,
  fixed to `keymatrixdown`/`keymatrixup`, re-run with genuine game audio.
- Evidence re-captures (recorded procedures, supersessions in §9):
  `debug/sounds/m32-fm-aleste-fmON/fmOFF.wav` and `debug/sounds/m27-example-tone.{dump,wav}`.

### S4 — Metal Gear BL verification probe + openMSX A/B (**outcome (a) — PARITY**)

Full narrative + evidence in §4. Headline: Metal Gear genuinely drives R#1 bit6 BL=0 windows
for every screen/room transition; the player-driven walk transition (UP through the top of the
beach-dock screen) blanks the display for **18 frames (frame 4173 line 11 → frame 4191
line 189)** on our side, and real openMSX shows the SAME window at frames **4175 → 4201** under
the same frame-indexed script — reference-corroborated end-to-end.

### S5 — The BL render gate + oracles

- `src/devices/video/vdp_frame_renderer.cpp`: ONE gate at the top of `render_line()` —
  `(R#1 & 0x40) == 0` ⇒ fill `width()` pixels with the mode-aware `border_color()` and return
  (content dispatch, `composite_sprites()`, AND the R#25 MSK step all skipped). NOT
  `render_blank()` (the undefined-mode palette-15 fallback — a different semantic). Grounding
  cited in-code: PixelRenderer.cc:608-611/:580-584, VDP.cc:435-442/:1080-1082/:1260-1269; fMSX
  MSX.h:216 + Common.h per-line `if(!ScreenON) ClearLine(background)`. No new state, no new
  clock consumer, no seam change — the L+1 latch comes free from the existing M32 write hook
  (`src/machine/` diff EMPTY, §7).
- NEW `tests/unit/devices/video_vdp_frame_renderer_bl_unit_test.cpp` (§3.4.3): TEXT2/G4/G5/G7/
  YJK BL=0 ⇒ every pixel == `border_color()` exactly; BL=1 same scene ⇒ genuine content;
  sprite arm (mode-2 sprite visible under BL=1, PURE backdrop under BL=0); determinism.
- NEW `tests/integration/machine/hbf1xv_m34_bl_latch_integration_test.cpp` (§3.4.4): through
  the REAL seam (`vdp().io_write(0x99, ...)`, the M32 hooked-write pattern) — BL 1→0 at display
  line 100 ⇒ content rows [0,100], backdrop rows [101,192); BL 0→1 at line 64 ⇒ backdrop
  [0,64], content [65,192); `debug_io_write` arm ⇒ NO split (documented seam exclusion);
  two-run identity. Legacy-renderer references (never-stepped twins), the M32 oracle pattern.
- `tests/unit/devices/video_vdp_scanline_accumulator_unit_test.cpp` matrix: every content row
  re-grounded to BL=1 (byte-preserving — pre-M34 the background renderer ignored bit6) + FOUR
  new BL=0 rows (G4/TEXT2/G7 backdrop + G4-with-sprites-configured); accumulator-vs-legacy
  equivalence green for all 19 configs.
- **BL=1 fixture re-grounding sweep** (the gate's honest fallout — every fixture that asserted
  CONTENT while modelling a BL=0 machine state, which real hardware never displays): 13 tests
  updated to set R#1 bit6 (each a hardware-honest fixture fix; asserted bytes unchanged):
  renderer unit fixtures ×10 (`text1`, `text2`, `graphic1`, `tile_modes`, `bitmap_modes`,
  `planar_interleave`, `yjk`, `effects`, `color0_backdrop`, `command_engine_nonbitmap`), the
  M21 machine-level pair (`hbf1xv_m21_vdp_render_{integration,system}_test`), and the M32
  per-line-latch integration scene. Plus the SAME class in `src/main.cpp`'s
  `--frame-dump-demo` synthetic scene (see §5 — with BL=1 it reproduces the committed
  `m26-example-boot` artifact byte-identically).
- Committed-PNG byte-identity sweep + A-M34-2: §5.

### S6 — Gates, ledger, docs

- Ledger (`agent-protocol/state/deferred-backlog.md`): NEW row **E4 — true band-limited audio
  resampling depth** (covers p=2–4 partial stopband, sinc droop, the 20.7 kHz residual, FM 9:8
  ZOH cross-note; pre-authorized cascaded-double-box escalation named per RESP-M34-001 Q1);
  D8/D9 rows gained the M34 BL cross-notes (line-granular BL latch → D8; BL=1-mid-frame sprite
  staleness → D9).
- Evidence gates: §6. This report + `docs/m34-audio-ab.md` are the QA handoff.

---

## 2. §2.5 oracle re-baseline — THE EXECUTED TABLE (row by row)

| # | Surface | Package disposition | EXECUTED result |
|---|---|---|---|
| 1 | `psg_audio_pump_unit_test.cpp` | R (cases 1–2), U-in-meaning (case 3) | **CONFIRMED — R executed.** Case 1 (W=16) re-derived by dwell math authored before running: `{0,31,0,31,0,31}` (the exact §2.3.3 flip of the old `{31,0,...}`). Case 2 (W=8): every 8-cycle window sits inside one 16-cycle level block → `{0,0,31,31,0,0}` (old `{0,31,31,0,0,31}` read post-toggle levels). Case 3 (W=0 idle) UNCHANGED, green. No surprise. |
| 2 | `psg_audio_dump_unit_test.cpp` | R (case 2), U (case 1) | **CONFIRMED — R executed.** W=1 carries the during-cycle level; the toggle completing AT cycle 16 belongs to the pre-step level → first audible sample at index 16 (old: 15 and 16). `expected_raw[15]` removed, `[16]=31` kept; `psg_raw_sum_to_pcm16` case byte-untouched. Green. No surprise. |
| 3 | `machine_audio_mixer_unit_test.cpp` | G (case 1), U (cases 2–5) | **CONFIRMED.** Case 1 re-grounded (dated M34 note; both sides now compute through the integrated pump — meaning intact: null SCC contributes exactly 0). Cases 2–5 (720 constant / 32,767 clamp / 31,940 / slot-2 / determinism) pass UNMODIFIED — verified, not assumed. No motion. |
| 4 | `machine_audio_mixer_fm_unit_test.cpp` | G (byte-identity arms), U (clamp/9:8/determinism) | **CONFIRMED.** `reference_pre_m31()` + header re-grounded; all cases pass with assertion bodies unmodified (constants are §2.3.4 fixed points; FM native path byte-untouched). No motion. |
| 5 | `machine_audio_mixer_loudness_ratio_unit_test.cpp` | U expected — A (motion = bug) | **AUDITED: passes UNMODIFIED.** psg_peak still 31×400 = 12,400 exactly (windows inside the 1,600-cycle half-periods are fixed points); fm untouched; ratio gate green. NO motion. |
| 6 | `audio_pacer_unit_test.cpp` | U | **CONFIRMED**: file untouched, `src/frontend/audio_pacer.*` diff EMPTY, green. |
| 7 | `sdl3_audio_presenter_integration_test.cpp` | U | **CONFIRMED**: untouched, green in the SDL3-ON suite; `sdl3_audio_presenter.*` diff EMPTY. |
| 8 | `hbf1xv_m29_scc_system_test.cpp` | U expected — A | **AUDITED: passes UNMODIFIED.** PSG raw 0 (cold-boot amplitudes 0 → integral exactly 0) + SCC held 60 through running-constant steps (fixed point) → 720 everywhere; two-run identity. NO motion. |
| 9 | `hbf1xv_m31_fm_synth_system_test.cpp` | U expected — A | **AUDITED: passes UNMODIFIED.** Silent PSG integrates to exactly 0; FM native path untouched → PCM byte-identical semantics preserved (pitch span, exact-silence tail, rhythm burst, session identity all green). NO motion. |
| 10 | `hbf1xv_m31_metalgear2_scc_integration_test.cpp` | A (A-M34-4) | **AUDITED: passes UNMODIFIED** — it asserts activity thresholds (`scc->sample() != 0` first-activity frame via the untouched point API, enable bits, zero FM key-ons) and two-session identity, never exact PCM values. A-M34-4 HOLDS. |
| 11 | `audio_psg_ym2149_unit_test.cpp` | U + additive | **CONFIRMED**: all pre-existing cases byte-untouched and green; M34 block appended (§1-S1). |
| 12 | `audio_scc_wavetable_unit_test.cpp` | U + additive | **CONFIRMED**: same. |
| 13 | `cartridge_konami_scc_rom_unit_test.cpp` | U | **CONFIRMED**: untouched, green (point `sample()` byte-kept). |
| 14 | `audio_ym2413_*` (×6) | U | **CONFIRMED**: untouched, green; `ym2413_synth.*`/`ym2413_opll.*` diffs EMPTY. |
| 15 | `debug/sounds/m32-fm-aleste-fmON/fmOFF.wav` | re-capture + supersession | **EXECUTED** (§9.1) — FM-divergence property re-measured on the new bytes: peak 3,780 (EXACTLY the M32 figure — FM path untouched), mean-nonzero 1,098.4 (M32: 1,094.3), 105,126 differing values, first divergence frame ~697 (M32: ~695-698). |
| 16 | `debug/sounds/m27-example-tone.{dump,wav}` | Q2: regenerate + supersession | **EXECUTED** (§9.2) — both byte-affected, both regenerated in place via the standing recipe. |

**Surprise log (the anti-tautology rule's stop-and-record duty): NONE of rows 1–16 surprised.**
The two genuine surprises of the milestone were OUTSIDE this table and are §3 (the pre-fix
Aleste baseline's left/right channel asymmetry) and §5 (BL=0 content in the aleste-play PNGs +
the `--frame-dump-demo` BL=0 scene) — each stopped, root-caused, and recorded, never silently
rebaselined.

## 3. New audio oracles — real measured numbers

**Ultrasonic-silence family** (`frontend_machine_audio_mixer_ultrasonic_silence_unit_test`,
4,096 samples/period, both stereo sides; hand-model predictions authored before execution and
matched exactly):

| p | measured peak AC (L/R) | §2.4 bound | pre-fix (point sampling) measured |
|---|---|---|---|
| 0 | 2,400 / 2,400 | ≤ 2,500 | 12,400 (FAIL — discrimination proven) |
| 1 | 2,400 / 2,400 | ≤ 2,500 | 12,400 (FAIL) |
| 2 | 2,800 / 2,800 | ≤ 5,100 | 12,400 (FAIL) |
| 3 | 2,400 / 2,400 | ≤ 2,500 | 12,400 (FAIL) |
| 4 | 7,200 / 7,200 | ≤ 7,400 | 12,400 (FAIL) |
| 112 (control) | max 24,800 / min 0 EXACTLY both sides | not suppressed | (also full swing) |

SCC arms green (stopped-channel −1 through the mixer = −12 every sample; period-9 dwell 26 at
chip level). Two-run byte identity green.

**Mid-band fidelity** (`frontend_machine_audio_mixer_midband_fidelity_unit_test`, 8,192
samples): p=254 (440 Hz): max 12,400 / min 0 exactly, 157 intermediate samples (bound 327),
AC-RMS 6,158.72 == predictor 6,158.72, and within ±1% of the plain A/2 = 6,200 (−0.67%). p=112
(999 Hz): max/min exact, 357 intermediate (bound 741), AC-RMS 6,105.57 == predictor 6,105.57.
Pre-fix discrimination: the p=112 predictor gate fails on point sampling (6,199.99 vs 6,105.57
= +1.55%, outside ±1%) and intermediate counts are 0. Two-run identity green.

**Aleste transition regression**
(`hbf1xv_m34_aleste_ultrasonic_regression_system_test`, frames 2150–2350, 35 blocks/side):

- **PRE-fix baseline (measured ONCE during S3 bring-up on the point-sampling build — the
  committed metric, the committed recipe):**
  `LEFT: burst_blocks=0 max_zcr_flips=3839 max_ac_rms=5073` /
  `RIGHT: burst_blocks=17 max_zcr_flips=3839 (ZCR 0.937) max_ac_rms=5874` — 17 BURST blocks at
  frames ~2189–2262. THE METRIC FIRES PRE-FIX, measured, not assumed (A-M34-1 verified with a
  refinement: the burst lives on the RIGHT side of this trajectory — the game holds R10=0 so
  right = channel A alone carries the alias, while left = A+B is a phase-opposed pair that
  self-cancels to an RMS-400-class residual. Register snapshot in-window: R0..R5=0, R7=0xB8,
  R8=12, R9=11..12, R10=0 — DEC-0043's exact idiom. The shipped test meters BOTH sides — a
  recorded strengthening of the package's left-channel clause, §8 deviation 1).
- **POST-fix:** `LEFT: burst_blocks=0 (max flips 906)` / `RIGHT: burst_blocks=0 (max flips
  3839 at RMS ~400–500-class; max_ac_rms 2,619 belongs to a low-ZCR music block)` — ZERO burst
  blocks both sides; two-session byte identity green. The residual 20.7 kHz tone sits at
  ~1.5% of full scale (500/32,767) — the §2.4-predicted inaudible-class residue (the
  pre-authorized cascaded second box remains UNBUILT; no audible-residual signal exists on the
  evidence).

**Audio A/B:** PARITY (silence property) — `docs/m34-audio-ab.md` (side B: 139 blocks, zero
bursts, real music at max RMS 3,143 / ZCR ≤ 0.231, openMSX 19.1 WSL `record -audioonly`).

## 4. S4 — Metal Gear BL probe + openMSX A/B: **outcome (a), PARITY**

- **Script design finding (recorded honestly):** the M31/M32-era "f1100 gameplay" state on the
  recorded unattended recipe is Metal Gear's ATTRACT DEMO (control run without input past
  frame 615: the trucks scene autoplays, Snake enters a truck at ~f1508 behind a 56-frame BL=0
  wipe, then the loop returns to the Konami splash). A real game requires SPACE pressed while
  the title genuinely waits. The final recorded script
  (`tools/metalgear-m34-bl-transition-input.script`, HBF1XV-INPUT-SCRIPT v1, 20 cycle-stamped
  events) therefore: SPACE holds [300,315)+[600,615) (recorded lineage) → UP at 1149 (aborts
  the demo → title) → SPACE [1249,1264) (starts a REAL game) → RETURN taps at
  2700/3000/3300/3600/3900 (the intro transceiver call advances on RETURN — determined by a
  recorded key sweep: SPACE/F4/LSHIFT do NOT advance it) → UP held [4100, 5600) (Snake walks
  north off the beach-dock screen).
- **Side A (ours)** — `debug/logs/m34-mg-bl-probe.txt` (every R#1 bit6 transition with frame +
  display line; scratchpad probe driver linked against the project library — ZERO src/ probe
  code, nothing to revert; `git diff -- src/main.cpp` contains only the permanent §5
  `--frame-dump-demo` BL fixture line). Measured BL=0 windows: boot/title cluster; game-load
  **f1393(line 8) → f1467(line 192)** (74 frames); transceiver-open **f2042 → f2049**;
  transceiver-close **f3603 → f3620**; **the player-driven room transition f4173(line 11) →
  f4191(line 189) — 18 frames** while walking north. Committed frame evidence:
  `debug/frames/m34-mg-bl-f4000.png` (beach dock, before), `m34-mg-bl-f4180.png` (INSIDE the
  window — pure backdrop post-fix), `m34-mg-bl-f4180-prefix-slowfill.png` (the SAME frame
  rendered by the pre-gate renderer: the half-rewritten-VRAM "slow fill" the human reported —
  the defect, visualized), `m34-mg-bl-f4400.png` (the next room: the tank yard).
- **Side B (real openMSX 19.1/WSL)** — `tools/openmsx-m34-mg-bl-ab.ps1` (frame-indexed
  `keymatrixdown/up` injection mirroring the script; R#1 polled per frame): boot cluster at
  f137/258/308/475/517/579-611 (ours: 126/248/298/466/509/571-599 — ~10-frame skew, different
  BIOS-image boot tolerances); short intro blank f966–970 (ours f897, sub-frame — invisible to
  per-frame polling, consistent); game-load blank **f1401 → f1482** (ours 1393 → 1467);
  transceiver open **f2058 → f2074** (ours 2042 → 2049); close **f3605 → f3629** (ours
  3603 → 3620); **walk transition f4175 → f4201** (ours 4173 → 4191 — same trajectory point
  within 2 frames). Screenshot captured INSIDE the reference blank window at f4181
  (`build/m34-ab/openmsx_mg_blank_window.png`) — it shows the next room already composed
  (openMSX's per-frame screenshot of a blanked display under throttle re-enable; the
  authoritative reference-side evidence is the R#1 log itself, whose window extents match
  ours; recorded caveat).
- **Declaration: OUTCOME (a)** — the game genuinely blanks via R#1 bit6 across room
  transitions, and the reference corroborates the same windows at the same trajectory points.
  The BL gate's symptom attribution to the human's Metal Gear "slow fill" report is fully
  corroborated (the pre-fix renderer displayed half-rewritten VRAM exactly inside these
  windows — visualized by the committed prefix-slowfill artifact). No re-scope, no new triage
  row needed.

## 5. Committed-evidence byte-identity sweep (§3.4.1) + A-M34-2

All recipes re-run on the FINAL tree (gate live) via the recorded `--debug-session` recipes /
`tools/capture-metalgear-evidence.ps1` lineage; PNGs re-encoded with the deterministic
`tools/frame-to-png.py` and byte-compared against the committed artifacts:

| Artifact | Result |
|---|---|
| `boot-logo-after-f0150-letters-sliding.png` (bordered f150) | **byte-identical** |
| `m31-rc-bios-f210.png` / `boot-logo-after-f0210-mainram64k.png` / `boot-logo-post-colorfix-f0210-mainram64k.png` | **byte-identical** (3/3) |
| `border-before-f0300-active-area-only.png` / `border-after-f0300-basic-skyblue-border.png` | **byte-identical** (2/2) |
| `m31-rc-dos-f1000.png` / `c5-verify-settled.png` | **byte-identical** (2/2) |
| `konami-splash-after-f0750-white-background.png` (unattended MG f750) | **byte-identical** |
| `m31-rc-metalgear-f1100.png` (tools/capture-metalgear-evidence.ps1, two-run-identity asserted by the tool) | **byte-identical** |
| `m31-rc-mg2-f1260.png` (unattended MG2) | **byte-identical** |
| `m31-rc-aleste-f900.png` (M31 SPACE 300/600 recipe) | **byte-identical** |
| `m26-example-boot.{frame,png}` (--frame-dump-demo) | **byte-identical AFTER the demo-fixture fix** (below) |
| `m32-aleste-play-f2600.png` (weapon select) | **byte-identical** |
| `m32-aleste-play-f{2900,3000,3200,3600}.png` (gameplay) | **DIVERGE — ESCALATED, not rebaselined** (below) |

Out-of-matrix exclusions (same scope as the M32 AC-5 precedent): `*openmsx-reference*` PNGs
(reference-side captures, not ours), `m32-divergence-*`/`m32-split-ab-*` (frozen A/B evidence),
`mg-sprite-invisibility-*` (pre-M21-era), `m29-aleste-f*` (v1.0.30-binary captures, explicitly
outside the matrix since M32), `boot-logo-before-f0135` / `konami-splash-before-f0750` (M32
precedent: no recovered exact recipe / historical pre-fix artifact).

**A-M34-2 verification outcome (instrumented, not assumed):** the sweep itself is the
instrument — with the gate LIVE, any BL=0 line in a committed frame would now render backdrop
and break byte-identity. It holds for every artifact EXCEPT the four Aleste gameplay frames:

1. **The four `m32-aleste-play` gameplay PNGs contain a genuine BL=0 band.** The dedicated
   Aleste R#1 probe (scratchpad, same driver class as S4) measured: from gameplay start
   (~f2849) the game drops BL=0 at display line 13 and re-enables at line 15 EVERY frame — the
   classic 2-line display-off band hiding the HUD/playfield scroll seam (915 R#1 transitions
   over 3,300 frames). Under the reference-corroborated gate those two lines (rows 14–15 via
   the L+1 latch) now legitimately render backdrop; the committed PNGs captured pre-M34
   BL-ignored content there. Exactly the package's predicted A-M34-2-false case ⇒ **escalated
   to the coordinator; committed artifacts left untouched** (regeneration-with-supersession is
   the coordinator's call, the M32 AC-6 pattern).
2. **The f3000/3200/3600 frames additionally carry a pre-existing, M34-independent residue**
   (sprite-band-sized regions, growing over time: 19/60/210 further rows). DISCRIMINATOR RUN
   (recorded): with the BL gate temporarily stashed and ONLY the M34 audio changes in place,
   the f3000 recapture vs the committed PNG differs in rows 99–117 ONLY — rows 14/15 match —
   proving the residue exists WITHOUT any M34 render change. f2900 differs in rows 14–15 ONLY
   (gate-only). Attribution: the committed m32-aleste-play PNGs were captured by M32's
   temporary `--m32-evidence` probe whose exact input-stamp mechanics were reverted without a
   committed script (the recorded recipe is prose-level: "SPACE holds at 600/1500/2100/2700");
   my stamp reconstruction ((f−1)×59736, the committed metalgear-script convention) reproduces
   f2600 and the f2900 background byte-exactly but not the gameplay sprite evolution. Same
   class as the M31 Metal Gear recipe gap (M32 report §5). **Escalated with the discriminator
   evidence; nothing rebaselined.** Recommendation: regenerate the four at closure from the
   NOW-COMMITTED script inside `hbf1xv_m34_aleste_ultrasonic_regression_system_test` (or a
   dedicated tools/ script), with supersession notes.
3. **`m26-example-boot` (§1-S5):** `--frame-dump-demo` built its synthetic scene with R#1=0 —
   a BL=0 fixture, the same class as the 13 test fixtures. One-line fix in `src/main.cpp`
   (`set_register(1, 0x40)` with an M34 comment): the demo now reproduces the committed
   `.frame` AND `.png` **byte-identically** (no supersession needed — the committed artifact
   was captured when BL was ignored, and with BL=1 the same bytes come out of the gated
   renderer).

## 6. Evidence gates (final tree; real captured outputs)

- `tools/validate-assets.ps1` → **"Asset validation result: True"** (7 BIOS files; aleste.rom,
  metalgear.rom, metalgear2_scc.rom).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` → refreshed; content identical,
  timestamp-only diff.
- `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md` refreshed (behavior-affecting
  gate); the historical R5-resolution note restored after regeneration (the DEC-0032/F1
  lesson — the script overwrites it); diff is timestamp-only.
- Headless config (`cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` on the ONE canonical
  tree): build zero errors; fast subset → **183/183, 51.33 s** (baseline 177 + 6 new).
- SDL3-ON superset (`tools/bootstrap-build.ps1 -RunTests`, restores the canonical ON
  configuration as the tree's final state): fast subset → **192/192, 52.12 s** (baseline 186 +
  6 new). `-Slow` correctly NOT run.
- New ctest additions (all headless-buildable): `devices_audio_dwell_rounding_unit_test`,
  `frontend_machine_audio_mixer_ultrasonic_silence_unit_test`,
  `frontend_machine_audio_mixer_midband_fidelity_unit_test`,
  `hbf1xv_m34_aleste_ultrasonic_regression_system_test` (asset-gated skip discipline),
  `devices_vdp_frame_renderer_bl_unit_test`, `machine_hbf1xv_m34_bl_latch_integration_test`.

## 7. Isolation / audit gates

- `git diff v1.0.34 -- src/devices/cpu src/core` → **EMPTY** (tripwire never fired; working-tree
  diff also empty). ZEXALL/ZEXDOC correctly NOT run.
- Path-scoped diffs **EMPTY** for: `audio_pacer.*`, `sdl3_audio_presenter.*`, `ym2413_synth.*`,
  `ym2413_opll.*`, `vdp_scanline_accumulator.*`, and the whole `src/machine/` tree (§4
  dependency map honored; the L+1 BL latch needed zero seam changes).
- Universal-fix audit: grep over every changed src/ path for title/SHA conditionals — the only
  hits are doc comments naming the evidence vehicles (psg_audio_pump.h's defect description;
  machine_audio_mixer.h's pre-existing M32 comment); ZERO game-keyed logic. The chip/render
  changes are pure hardware physics (dwell integration; the BL bit both references implement).
- License isolation: no code/tables from `references/openmsx-21.0/` or `references/fmsx-60/`;
  every behavior claim cites concrete paths (AY8910.cc:38-39/:482;
  ResampledSoundDevice.hh:23,29,46-48; BlipBuffer.hh:1-28; PixelRenderer.cc:580-584/:608-611;
  VDP.cc:435-442/:1080-1082/:1260-1269; fMSX MSX.h:216, Common.h:463...; Sony fact-sheets).
- Probe hygiene: all M34 probes were SCRATCHPAD drivers linked against the project static
  library — no temporary src/ probe code existed, so there is nothing to revert;
  `git diff -- src/main.cpp` contains ONLY the permanent, additive `--frame-dump-demo` BL
  fixture line (§5.3).

## 8. Deviations from the package (each recorded with reason)

1. **§2.6.4 burst metric meters BOTH stereo sides, not left only** — the S3 bring-up
   measurement showed the pre-fix burst manifests on the RIGHT channel in this trajectory
   (left = phase-opposed A+B self-cancels; right = A alone at R10=0). Left-only would have
   passed PRE-fix (burst_blocks=0 on the left) — a vacuous oracle. Both-sides is strictly
   stronger and makes the discrimination genuine (17 right-side burst blocks pre-fix).
2. **Window-restricted audio production in the Aleste regression** (package-authorized runtime
   choice, §2.6.4/§6-row-10): production frames 2150–2350 only; the chips' generator phase
   origin shifts relative to continuous production, changing no metric property of the
   periodic idiom (verified: the continuous-production probe shows the same burst signature;
   the pre-fix baseline used the committed window-restricted form).
3. **S4 script reality vs charter prose**: "walk Snake through a door after the recorded f1100
   gameplay state" — f1100 on the recorded recipe is the ATTRACT DEMO (measured, §4); the
   recorded extended script instead starts a REAL game and drives a genuine player-controlled
   screen transition. The charter's intent (a deterministic, script-recorded, player-driven
   room change with R#1 instrumentation) is delivered in full.
4. **BL=1 fixture re-grounding across 13 pre-existing tests + the frame-dump demo** (§1-S5,
   §5.3): not individually enumerated by the package (which specified the gate + new oracles);
   required because those fixtures modelled a state real hardware never displays. Each edit
   sets only R#1 bit6 (byte-preserving for every assertion; suite-proven).
5. **m34-audio A/B side-B first attempt was vacuous** (wrong Tcl command name produced an
   all-zero capture) — caught by inspecting the capture's max RMS, fixed, re-run; recorded in
   `docs/m34-audio-ab.md` rather than silently discarded.

## 9. Evidence supersession (Q2 + §2.5 rows 15–16; originals remain at tag v1.0.34)

1. **`debug/sounds/m32-fm-aleste-fmON.wav` / `-fmOFF.wav`** — re-captured on the recorded
   M31/M32 procedure (two IDENTICAL deterministic Aleste sessions, real BIOS + KonamiSCC
   explicit + SPACE holds [300,315)/[600,615), 900 frames × 735 mixed samples through the REAL
   three-source mixer; fmON = machine OPLL, fmOFF = fm nullptr; window = first differing stereo
   pair to end of session, written as HBF1XV-AUDIO-DUMP v1 → `tools/audio-dump-to-wav.py`).
   New bytes (the M34 integrated PSG/SCC terms legitimately change PCM):
   fmON `79d28df0…` (old `2be6991c…`), fmOFF `75681cce…` (old `921a4b80…`); 149,205 pairs
   (3.38 s). FM-divergence property RE-MEASURED on the new bytes: first divergence frame ~697,
   105,126 differing values, **peak 3,780 = exactly the M32 figure** (FM path untouched),
   mean-nonzero 1,098.4 (M32: 1,094.3). Local `.dump` companions regenerated alongside.
2. **`debug/sounds/m27-example-tone.{dump,wav}`** (Q2 ruling) — regenerated in place via the
   standing recipe (`sony_msx_headless --audio-dump-demo m27-example-tone.dump` →
   `tools/audio-dump-to-wav.py`): dump `f2ca85bc…` (old `d783ec97…`), wav `34d5a0b6…` (old
   `df15911e…`). Byte change = the W=81 box average's transition-window samples (the tone-edge
   windows now carry exact intermediate levels; the 44.1 kHz/1 s format is unchanged).

## 10. Known issues / open items for QA

1. **The four `m32-aleste-play` gameplay PNG divergences (§5)** await coordinator arbitration —
   BL band (rows 14–15, gate-correct) + pre-existing recipe residue (discriminator-proven
   M34-independent). Suggested closure: regenerate with the now-committed script + supersession
   notes (M32 AC-6 pattern).
2. **The 20.7 kHz post-fix residual** (~1.5% full scale in the Aleste capture) is the disclosed
   §2.4 box-filter remainder; the cascaded second box stays UNBUILT per Q1 (pre-authorized only
   on an audible-residual report). The human's ear on the regenerated
   `debug/sounds/m32-fm-aleste-fmON.wav` (and live play) remains the final acceptance signal.
3. **p=2–4 partial suppression** is honestly bounded (measured phase-locked peaks 2,800/7,200
   PCM), asserted as bounds in the ultrasonic oracle, and owned by backlog row **E4**.
4. **D9 interaction**: a BL=1 re-enable mid-frame composites sprites from the frame-boundary
   table until D9 lands (ledger cross-note added); no evidence-set title exercises it.
5. R-M34-8 (integration cost): fast-suite wall time 41.1 s → 45.8–57.0 s across the cycle
   (~10.8 s = the new Aleste system test itself); heaviest audio test 11.55 s. No interactive
   frame-rate measurement this cycle (headless-only evidence) — same flag as M32 for the
   human's next live session.
6. `build/m34-ab/*` artifacts (A/B logs, reference WAV/screenshot) are local and regenerable
   via the two committed harnesses.

## 11. Complete new/modified file inventory (for the coordinator's `git add -f` discipline)

**src/ (tracked normally):** `src/devices/audio/dwell_rounding.h` (NEW),
`src/devices/audio/psg_ym2149.{h,cpp}`, `src/devices/audio/scc_wavetable.{h,cpp}`,
`src/devices/video/vdp_frame_renderer.cpp`, `src/frontend/psg_audio_pump.{h,cpp}`,
`src/frontend/machine_audio_mixer.{h,cpp}`, `src/frontend/psg_audio_dump.h` (comments),
`src/main.cpp` (frame-dump-demo BL fixture).

**tests/ (gitignored tree — `git add -f`):** NEW
`tests/unit/devices/audio_dwell_rounding_unit_test.cpp`,
`tests/unit/frontend/machine_audio_mixer_ultrasonic_silence_unit_test.cpp`,
`tests/unit/frontend/machine_audio_mixer_midband_fidelity_unit_test.cpp`,
`tests/system/hbf1xv_m34_aleste_ultrasonic_regression_system_test.cpp`,
`tests/unit/devices/video_vdp_frame_renderer_bl_unit_test.cpp`,
`tests/integration/machine/hbf1xv_m34_bl_latch_integration_test.cpp`; MODIFIED
`tests/CMakeLists.txt`, `tests/unit/frontend/psg_audio_pump_unit_test.cpp`,
`tests/unit/frontend/psg_audio_dump_unit_test.cpp`,
`tests/unit/frontend/machine_audio_mixer_unit_test.cpp`,
`tests/unit/frontend/machine_audio_mixer_fm_unit_test.cpp`,
`tests/unit/devices/audio_psg_ym2149_unit_test.cpp`,
`tests/unit/devices/audio_scc_wavetable_unit_test.cpp`,
`tests/unit/devices/video_vdp_scanline_accumulator_unit_test.cpp`, and the 13-file BL=1
fixture sweep: `video_vdp_frame_renderer_{text1,text2,graphic1,tile_modes,bitmap_modes,yjk,
effects,color0_backdrop}_unit_test.cpp`, `video_vdp_planar_interleave_unit_test.cpp`,
`video_vdp_command_engine_nonbitmap_unit_test.cpp`,
`tests/integration/machine/hbf1xv_m21_vdp_render_integration_test.cpp`,
`tests/integration/machine/hbf1xv_m32_per_line_latch_integration_test.cpp`,
`tests/system/hbf1xv_m21_vdp_render_system_test.cpp`.

**tools/ (`git add -f`):** NEW `tools/openmsx-m34-aleste-audio-ab.ps1`,
`tools/openmsx-m34-mg-bl-ab.ps1`, `tools/m34-audio-ab-analyze.py`,
`tools/metalgear-m34-bl-transition-input.script`.

**docs/ (`git add -f`):** NEW `docs/m34-audio-ab.md`, `docs/m34-implementation-report.md`
(this file), `docs/m34-planner-package.md` (planner's, already present); MODIFIED
`docs/asset-checksums.txt`, `docs/openmsx-ab-smoke.md` (timestamp-only + restored note).

**debug/ (`git add -f` for the new evidence):** NEW `debug/logs/m34-mg-bl-probe.txt`,
`debug/frames/m34-mg-bl-f4000.png`, `debug/frames/m34-mg-bl-f4180.png`,
`debug/frames/m34-mg-bl-f4180-prefix-slowfill.png`, `debug/frames/m34-mg-bl-f4400.png`;
MODIFIED (supersessions, §9) `debug/sounds/m32-fm-aleste-fmON.wav`,
`debug/sounds/m32-fm-aleste-fmOFF.wav`, `debug/sounds/m27-example-tone.dump`,
`debug/sounds/m27-example-tone.wav`. (Local-only, untracked, regenerable: `m34s-*` sweep
frames, `m34-mg-bl-f*.frame` companions, `.dump` fm companions.)

**agent-protocol/ :** `agent-protocol/state/deferred-backlog.md` (E4 row + D8/D9 cross-notes).

## 12. QA handoff

All package acceptance criteria addressed: AC-S1.1–.3 (§1-S1; 187/187 with production
untouched), AC-S2.1–.4 (§2 executed table; zero-source oracles intact in meaning; rows 5/8/9
unmodified; path diffs empty), AC-S3.1–.5 (§3; pre-fix baseline measured; PARITY A/B;
supersessions §9), AC-S4.1–.4 (§4; outcome (a); zero-src probe), AC-S5.1–.5 (§1-S5, §5;
byte-identity sweep with two escalations, never rebaselined), AC-S6.1–.4 (§6, §7; 183/183
headless + 192/192 SDL3-ON; ledger §1-S6; honest §2.4 statement in `psg_ym2149.h` and §3/§10
here). Working tree uncommitted; the coordinator owns the commit, the §5 escalations, and QA
dispatch.
