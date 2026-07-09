# M34 Planner Package — Playtest Defect Pair: PSG/SCC Point-Sampling Aliasing (A) + R#1 BL Display-Enable Gate (B)

- Milestone ID: M34 (DEC-0043 charter, 2026-07-09; human-reported v1.0.33/v1.0.34 playtest defects, both coordinator-triaged to root cause with evidence)
- Baseline: v1.0.34 (M33 closed, DEC-0042). Fast subsets green: headless 177/177, SDL3-ON 186/186. Tag target: **v1.0.35**.
- Author: msx-planner, 2026-07-09.
- Standing constraints (hard):
  - **Universal fixes only** (`feedback_universal_fixes_only.md`): both fixes model chip/renderer physics generically. No Aleste-keyed or Metal-Gear-keyed logic anywhere; the titles are only the *evidence vehicles*.
  - **ZEXALL/ZEXDOC is NOT triggered** (`feedback_slow_test_cadence.md`): no file under `src/devices/cpu/` or `src/core/` is in scope. `src/devices/audio/` and `src/devices/video/` edits do **not** trip the carve-out. **Tripwire**: if any slice discovers it must touch `src/devices/cpu/` or `src/core/`, STOP and escalate to the coordinator before writing a line; if such an edit ever lands, the immediate-sweep rule fires.
  - **Single-build policy** (DEC-0042): ALL builds/tests through the ONE canonical `build/` tree via `tools/bootstrap-build.ps1` (fast subset default). `-Slow` is **NOT authorized** this milestone. Never create per-agent build directories.
  - **License isolation**: openMSX (GPL) and fMSX (non-commercial freeware) are read-only behavior references — cited below with concrete paths/lines actually read; never copied into `src/`.
  - **Oracle discipline** (DEC-0043 risk note 1): Defect A deliberately changes output PCM bytes. Every affected audio oracle is enumerated in §2.5 with an explicit disposition (unchanged / re-derived / re-baselined-with-new-grounding). Nothing is silently rebaselined; hand oracles are re-derived from dwell math *before* running the code (anti-tautology).

---

## 1. Scope and Assumptions

### 1.1 Included scope

- **Defect A (audio, universal)**: replace point-sampling with exact integer box-average integration of the chip output level across each sample window, for **PSG (YM2149)** and **SCC** production paths. FM (YM2413) is untouched (already generates at native 49,716 Hz).
- **Defect B (video, universal)**: gate the background renderer's per-line output on R#1 bit 6 (BL, display enable) — a BL=0 line renders pure backdrop, sprites included (they are off when BL=0).
- In-milestone verification probes for both defects (Aleste audio regression capture; Metal Gear BL transition instrumentation + openMSX A/B arbitration).
- Full oracle re-baseline per §2.5; new hard oracles per §2.6/§3.4; evidence re-capture with supersession notes; ledger/docs/QA handoff.

### 1.2 Excluded scope (named non-goals)

1. **True band-limited resampling** (polyphase sinc / BLEP / BlipBuffer-class synthesis): explicitly out of scope. The box filter is the charter-recommended deterministic lightweight equivalent; its honest limitations (§2.4) become an E-series ledger row at closure.
2. **FM (YM2413) resampling changes**: the 9:8 ZOH decimation (M31 §2.5 disclosed simplification) stays as-is.
3. **AudioPacer / DEC-0033 pacing accounting**: byte-untouched. The pacer decides HOW MANY samples; this milestone changes only how each sample's VALUE is produced.
4. **Per-line live sprite fetch** (D9) and **sub-line accuracy** (D8): the BL gate is line-granular via the existing M32 seam; mid-*line* BL effects and BL=1-mid-frame sprite-table liveness are D8/D9-class remainders (cross-notes only).
5. **GRAPHIC5 two-color checkered border**: the single-`border_color()` simplification (disclosed since M21, `vdp_frame_renderer.h:54-59`) carries over to BL=0 lines unchanged.
6. **kCyclesPerSample=81** (the disclosed −3.6-cent integer simplification, `machine_audio_mixer.h:146-149`): not "fixed" here; the box window is defined over whatever `cycles_per_sample` the caller supplies.

### 1.3 Assumptions (each with verification action)

- **A-M34-1**: The recorded Aleste smoke recipe (M32 shape: KonamiSCC explicit, SPACE holds at frames 600/1500/2100/2700, real frame loop — `docs/m32-implementation-report.md` AC-9 section) deterministically reproduces the frames 2190–2299 ultrasonic burst DEC-0043 measured. *Verify in S3 by re-measuring the pre-fix baseline with the committed metric before the fix lands.*
- **A-M34-2**: Every committed frame-evidence PNG was captured with BL=1 (R#1 bit6 set). *Verify in S5 — never assume: the byte-identity sweep is the gate, and a per-recipe R#1 probe is the explanation. Any divergence ⇒ escalate (M32 AC-6 pattern), never rebaseline.*
- **A-M34-3**: openMSX on WSL can record its audio mix to a WAV file headlessly (`record sound`), independent of a host audio device. *Verify in S3; documented fallback in §2.7.*
- **A-M34-4**: The `hbf1xv_m31_metalgear2_scc_integration_test` SCC-activity detection survives box filtering (activity thresholds, not exact-PCM assertions). *Audit in S2 (§2.5 row 10); re-derive with recorded justification if wrong.*
- **A-M34-5**: `sample()` / `amp_out()` (point-sample chip APIs) can be kept byte-identical for all existing chip-level unit tests while the *production path* switches to integrated samples. *Structurally guaranteed by the §2.3 contract (additive APIs); confirmed by the untouched-test rows of §2.5.*

---

## 2. Defect A — PSG/SCC point-sampling aliasing

### 2.1 Root cause (coordinator-evidenced, DEC-0043) and the exact numbers

- Production samples are produced by `PsgAudioPump::pump_one_sample()` (`src/frontend/psg_audio_pump.cpp:7-10`): `advance_cycles(81)` then ONE instantaneous `sample()`. `MachineAudioMixer::mix_interleaved_stereo()` does the same for SCC (`src/frontend/machine_audio_mixer.cpp:29-35`).
- Aleste 2's silence idiom: tone period 0 on all three channels, tones enabled, volumes modulated 7–15. In our PSG, `Tone::set_period(0)` → `period = max(1,0) = 1` (`psg_ym2149.cpp:98-103`) → output toggles every generator step (16 cycles, `psg_ym2149.h:54`) → a square of full period **32 cycles = 111,861 Hz** (matches DEC-0043's "~112 kHz").
- Effective sample rate: f_clk/81 = 3,579,545/81 ≈ **44,191.9 Hz** (Nyquist ≈ 22,096 Hz). Point-sampling folds 111,861 Hz: 111,861 − 2×44,192 = 23,477 Hz → reflects to **44,192 − 23,477 = 20,715 Hz** — exactly the measured "~20.6 kHz-class" tone at RMS up to 17,533. Real hardware output at 112 kHz is inaudible and analog-smoothed to its mean level; the game is correct, our sampler is the defect.

### 2.2 Reference model characterization (read, not copied)

- **openMSX**: every sound chip generates at its own native rate and is band-limit-resampled centrally. `AY8910.cc:38-39` — `NATIVE_FREQ = (3579545/2)/8` ≈ 223,722 Hz native generation; `AY8910.cc:482` constructs `ResampledSoundDevice(..., NATIVE_FREQ_INT, ...)`. `ResampledSoundDevice.hh:23,29,46-48` — a `ResampleAlgo` (`HQ` polyphase or `BLIP`) converts native-rate input to host rate. `BlipBuffer.hh:1-28` — Blip_Buffer 0.4.0-derived band-limited step synthesis, 10 phase bits, float accumulation. Both algorithms are float-based true band-limiters.
- **Ours (deliberate, disclosed divergence in mechanism, not in goal)**: exact integer box-average over the 81-cycle window — the zero-order model of the analog reconstruction. Deterministic, integer-only, no float in the sample loop (DEC-0043 risk note 3). Its frequency response is a **sinc**, honestly documented in §2.4: it is *not* a brickwall; stopband suppression is partial and the audible band has mild sinc rolloff. This is the disclosed simplification; true band-limited depth is the named E-series follow-on (§8).
- **fMSX**: not architecturally comparable for this defect (event/frequency-driven host synthesis rather than native-rate sample generation); not used as grounding here.

### 2.3 Integration-point decision (THE architecture call) and contract

**Decision: the integration lives INSIDE each chip's `advance_cycles()` (chip-internal dwell accumulation), with a new paired take-API; the pump/mixer only orchestrate.** Rationale (least architectural violence):

- Exact box integration requires knowing *when the output level changes inside the window*. For the PSG that grain is the 16-cycle generator step plus the residual phase (`cycle_residual_`, `psg_ym2149.cpp:293-306`); for the SCC it is **per-channel** `(period+1)`-cycle position steps (`scc_wavetable.cpp:221-244`). The pump cannot see either without gutting chip encapsulation; the chips already walk exactly these boundaries in their advance loops.
- The pump-side alternative (sub-advancing 1 cycle × 81 and averaging point samples) is O(81×generators) per sample and still needs chip phase knowledge to be exact. Rejected.
- The chip contract keeps the existing point `sample()`/`amp_out()` APIs **byte-untouched** (all M15/M29 chip-level unit tests survive unchanged, §2.5 rows 11–13).

Contract (developer implements; names indicative):

1. **`PsgYm2149`** — during `advance_cycles(delta)`, accumulate per-channel `Σ level_ch(t) × dwell_cycles` into three `std::uint64_t` integrals, where `level_ch = channel_audible(ch) ? resolved_amplitude(ch) : 0` is piecewise-constant between generator steps (segment walk: head partial-step from `cycle_residual_`, whole 16-cycle steps, tail partial). New API `StereoSample take_integrated_sample(std::uint64_t window_cycles)`: `left = round_div(intA + intB, W)`, `right = round_div(intA + intC, W)` (stereo law unchanged: fact-sheet §2, A=center/B=left/C=right), then reset integrals. `sample()` (point) remains untouched.
2. **`SccWavetable`** — same pattern: per-channel dwell walk inside `advance_cycles()` (a stopped channel, period ≤ 8, holds `out_` constant → contributes `out × delta` exactly; enabled-gating per `enable_` as in `sample()`, `scc_wavetable.cpp:246-256`), one mono `std::int64_t` integral, `std::int32_t take_integrated_sample(std::uint64_t window_cycles)`. `sample()`/`amp_out()` untouched.
3. **Boundary convention (MUST be documented in the headers)**: a generator/position step completing at cycle *t* changes the level effective *after* cycle *t* — the completing cycle's dwell belongs to the pre-step level. Consequence: with `cycles_per_sample = 16` and a period-1 PSG tone, the integrated sequence is `{0, 31, 0, 31, …}` where the point-sampler produced `{31, 0, 31, 0, …}` (the M26 pump oracle re-derivation, §2.5 row 1).
4. **Rounding convention**: one shared, unit-tested integer helper — recommended round-half-away-from-zero (`(2·s + sign(s)·W) / (2·W)` shape or equivalent); the developer records the final exact form in the header and the S1 report. Whatever the choice: **constants are fixed points** — for any window over which the summed level is a constant L, the integrated sample is exactly L (this property is unit-proven and is why the §2.5 "unchanged" rows are safe).
5. **Zero-window guard**: `window_cycles == 0` returns `{0, 0}` (preserves the M26 pump idle case).
6. **Consumers**: `PsgAudioPump::pump_one_sample()` = advance + `take_integrated_sample(W)` (signature unchanged); `MachineAudioMixer` SCC term likewise; FM branch byte-untouched; `psg_audio_dump` inherits the pump change (W=1 degenerates to per-cycle levels — exact). `Sdl3AudioPresenter`/`AudioPacer` untouched.
7. **Precondition**: the caller advances exactly `window_cycles` between takes (the pump guarantees this by construction); document it.

### 2.4 The box filter's honest frequency response (the disclosed-simplification block)

Window T = 81/3,579,545 s = 22.628 µs; |H(f)| = |sin(πfT)/(πfT)|. PSG tone period register p (effective P = max(1,p)): half-period H = 16P cycles, f = 3,579,545/(32P).

Exact worst-case AC bound of the box average (levels 0/A, H ≤ W = 81): `|avg − A/2| ≤ A·B(P)/W`, with `B(P) = H/2` when `H ≤ W/2`, else `B(P) = H − W/2`.

| p | f (Hz) | alias @44,192 Hz (Hz) | sinc attenuation | exact dwell bound B(P) cycles | per-channel peak bound (A=31, ×400 PCM) | 2-channel/side bound + rounding | disposition |
|---|---|---|---|---|---|---|---|
| 0/1 | 111,861 | 20,715 | 0.125 (−18.1 dB) | 7.5 (exact enum; ≤8 closed form) | ≤1,148 | **≤2,500** | Aleste idiom: suppressed ~18 dB AND parked ultrasonic-adjacent |
| 2 | 55,930 | 11,738 | 0.187 (−14.6 dB) | 16 | ≤2,449 | **≤5,100** | PARTIAL — audible-band alias survives at ≤ ~19% |
| 3 | 37,287 | 6,905 | 0.178 (−15.0 dB) | 7.5 | ≤1,148 | **≤2,500** | partial (bound small: 96-cycle period ≈ window) |
| 4 | 27,965 | 16,227 | 0.460 (−6.7 dB) | 23.5 | ≤3,597 | **≤7,400** | PARTIAL — weakest suppression of the family |
| 5 | 22,372 | 21,820 | 0.629 | 39.5 | ≤6,046 | ≤12,300 | alias itself ultrasonic-adjacent (>21.8 kHz) |
| ≥6 | ≤18,643 | — (legitimate audible content) | ≥0.73 (−2.7 dB @18.6 kHz) | — | — | — | passband: sinc rolloff only |

Passband droop (documentation figures): −0.0004 dB @440 Hz, −0.007 dB @1 kHz, −0.18 dB @5 kHz, −0.74 dB @10 kHz, −1.72 dB @15 kHz.

**Honest statement the package requires verbatim-in-spirit in the code header**: the box filter fully resolves the actual defect (the p=0/1 ~112 kHz silence idiom: ≥18 dB down AND the residual sits at ~20.7 kHz), but periods 2–4 receive only partial suppression (audible-band aliases at up to ~19%/46% of channel amplitude by the table above), and the audible band has sinc rolloff. It is a disclosed simplification vs openMSX's true band-limited resampling; the E-series ledger row (§8) names the full fix. The charter's "period 1-4 family produces near-zero AC" is *achieved as measured bounds for p=0/1/3 and only partially for p=2/4* — recorded here rather than silently claimed (open question Q1, §9).

Aleste post-fix prediction (for S3): measured burst RMS 17,533 × 0.125 ≈ **~2,190 RMS residual at 20,715 Hz** (plus an inaudible DC/mean term that the real chip's analog stage also produces). The volume-gating ("bbee bbee") becomes sub-audible mean-level modulation — the hardware-faithful outcome.

### 2.5 Oracle re-baseline enumeration (file-by-file; the central discipline table)

Dispositions: **U** = unchanged (byte-for-byte test file); **R** = re-derived (hand oracle recomputed from dwell math, shown in comments); **G** = re-grounded (test survives mechanically but its grounding comment must be updated); **A** = audit (verify claimed disposition at implementation, record result).

| # | File | What it asserts today | Disposition | Grounding for the disposition |
|---|---|---|---|---|
| 1 | `tests/unit/frontend/psg_audio_pump_unit_test.cpp` (M26) | Hand-computed point-sample sequences `{31,0,…}` (W=16), `{0,31,31,0,…}` (W=8); idle-silent W=0 | **R** (cases 1–2: the §2.3.3 boundary convention flips/reshapes the sequences — re-derive by dwell arithmetic, comment the math); **U in meaning** (case 3: idle stays exactly silent) | Constants/idle are fixed points; period-1 toggles land on window boundaries → windows average the pre-toggle level |
| 2 | `tests/unit/frontend/psg_audio_dump_unit_test.cpp` (M27) | W=1 hand oracle: `expected_raw[15]=31,[16]=31` (lines 111-113); pcm16 scale mapping (case 1) | **R** (case 2: W=1 windows carry the *during-cycle* level → the toggle completing at cycle 16 shifts the first audible sample by one index per §2.3.3); **U** (case 1: `psg_raw_sum_to_pcm16` untouched) | W=1 box = per-cycle level; scale function not in scope |
| 3 | `tests/unit/frontend/machine_audio_mixer_unit_test.cpp` (M29) | Case 1: zero-SCC byte-identity vs a pump-driven reference twin; cases 2/4: constant 720; case 3: constant clamp 32,767 / 31,940; case 5: determinism | **G** (case 1: both sides recompute through the same new pump — the oracle's MEANING survives: zero SCC sources contribute exactly 0 for ANY input; the "pre-M29 presenter arithmetic" comment gets a dated re-grounding note); **U** (cases 2–5: all constant-signal fixed points — verify, don't assume) | §2.3.4 fixed-point property; silent/null source still contributes exactly 0 (DEC-0043 requirement) |
| 4 | `tests/unit/frontend/machine_audio_mixer_fm_unit_test.cpp` (M31) | Zero-FM byte-identity (nullptr AND silent-attached), constant-max clamp both rails, exact 9:8 decimation, determinism | **G** (byte-identity arms recompute through the new pump; meaning survives: nullptr/silent FM contributes exactly 0); **U** (clamp/decimation/determinism: constants + FM-native path untouched); `reference_pre_m31()` helper comment re-grounded | Same as row 3; FM branch byte-untouched |
| 5 | `tests/unit/frontend/machine_audio_mixer_loudness_ratio_unit_test.cpp` (M32) | `psg_peak == 31×400` exactly (1.12 kHz square), FM/PSG peak ratio ±15%, determinism | **U expected** (windows fully inside a 1,600-cycle half-period integrate to exactly 31; FM untouched) — **A**: run and confirm; if the ±15% gate or the exact-peak assert moves, STOP: that indicates an integration bug, not a rebaseline | §2.4 passband: −0.007 dB @1 kHz; exact-dwell constancy inside half-periods |
| 6 | `tests/unit/frontend/audio_pacer_unit_test.cpp` (DEC-0033) | Sample-count accounting only | **U** (file byte-untouched; pacer out of scope) | §1.2.3 |
| 7 | `tests/integration/frontend/sdl3_audio_presenter_integration_test.cpp` (M26/M33) | Host-queue/lifecycle invariants, no PCM values | **U** | Asserts queue state, not sample values |
| 8 | `tests/system/hbf1xv_m29_scc_system_test.cpp` | PCM "720 everywhere" (constant SCC + silent PSG), two-run identity | **U expected** (both terms constant → fixed points) — **A**: confirm at implementation | §2.3.4 |
| 9 | `tests/system/hbf1xv_m31_fm_synth_system_test.cpp` | Mixed-PCM session (silent PSG + FM): span/period measurement via sign changes, exact-0 final quarter, byte-identical two-session | **U expected** (PSG term constant 0; FM native path untouched → PCM byte-identical to today) — **A**: confirm | Silent PSG integrates to exactly 0; FM branch untouched |
| 10 | `tests/integration/machine/hbf1xv_m31_metalgear2_scc_integration_test.cpp` | SCC activity through the mixer from a real boot | **A** (A-M34-4): thresholds expected to survive; if it embeds exact PCM values or an exact first-activity index, **R** with recorded justification | Real-boot PSG is non-constant → mixed bytes change where PSG varies |
| 11 | `tests/unit/devices/audio_psg_ym2149_unit_test.cpp` (M15) | Chip-level generator/register/point-`sample()` semantics | **U** (point API untouched) + **additive** new integration-API cases (§2.6) | §2.3 contract keeps `sample()` byte-identical |
| 12 | `tests/unit/devices/audio_scc_wavetable_unit_test.cpp` (M29) | Chip-level De Schrijver/deform/latch semantics incl. `sample()`/`amp_out()` | **U** + **additive** integration-API cases | Same |
| 13 | `tests/unit/devices/cartridge/cartridge_konami_scc_rom_unit_test.cpp` | Mapper semantics (one `sample()` use) | **U** | Point API untouched |
| 14 | `tests/unit/devices/audio_ym2413_*.cpp` (×6, M17/M28/M31) | FM synth/opll semantics | **U** (FM out of scope) | §1.2.2 |
| 15 | Committed evidence `debug/sounds/m32-fm-aleste-fmON/fmOFF.wav` (M32) | fmON/fmOFF audible-divergence evidence pair | **Re-captured** on the recorded M31/M32 procedure at the new pipeline, supersession note in the report (M32 evidence-supersession precedent; old bytes remain at tag v1.0.34) | PCM bytes legitimately change; the FM-divergence property must be re-shown on the new bytes |
| 16 | Committed evidence `debug/sounds/m27-example-tone.wav` (M27) | Example of the audio-dump tool's output | **Coordinator arbitration** (Q2, §9): recommended re-generate with supersession note (tool output changes); alternative: freeze as historical M27 artifact | Dump path inherits the pump change |

Final sweep obligation (S2 acceptance): a repo grep for `pump_one_sample|pump_samples|mix_interleaved_stereo|\.sample\(|amp_out\(|fm_sample\(` across `tests/` and `src/` reconciled against this table — any consumer not listed here is a table defect to fix in the report, not to ignore.

### 2.6 New hard oracles (Defect A)

1. **Ultrasonic-silence oracle** — NEW `tests/unit/frontend/machine_audio_mixer_ultrasonic_silence_unit_test.cpp`: PSG programmed with all three tones enabled at full volume (R8/R9/R10=15), tone period p ∈ {0, 1, 2, 3, 4}; mix ≥ 4,096 samples; assert per-period **peak |AC|** (mean-removed, integer mean) ≤ the §2.4 bound column (2,500 / 2,500 / 5,100 / 2,500 / 7,400 PCM) — every one discriminates hard against point-sampling (which peaks at 24,800). Control arm: period 112 (~999 Hz) must NOT be suppressed (peak == 31×2×400 exactly on both sides for in-half-period windows). SCC arm: stopped channel (period ≤ 8) integrates to its exact held constant; a fast-stepping channel (period 9, 8 steps/window) matches a hand-computed dwell average.
2. **Audible-fidelity oracle** — NEW `tests/unit/frontend/machine_audio_mixer_midband_fidelity_unit_test.cpp`: PSG squares at p=254 (440.1 Hz) and p=112 (998.8 Hz), full volume: (a) peak == 12,400 exactly (windows fully inside half-periods are constant → fixed point); (b) samples with intermediate values are confined to transition-straddling windows: ≤ 2 per tone half-period + 1; (c) block AC-RMS within ±1% of the ideal-square prediction. The sinc math (§2.4: −0.0004 dB / −0.007 dB) is cited in comments as the frequency-domain justification; the assertions themselves are exact integer time-domain properties.
3. **Determinism** — two-run byte identity inside oracles 1, 2, and 4 (mixer-level), same pattern as every existing suite.
4. **Aleste transition regression** — NEW `tests/system/hbf1xv_m34_aleste_ultrasonic_regression_system_test.cpp`: the recorded m32 smoke recipe (KonamiSCC explicit, SPACE holds at 600/1500/2100/2700, real frame loop — `docs/m32-implementation-report.md` AC-9), audio produced deterministically over frames **2150–2350** (window-restricted production is permitted for runtime — record the choice; two-run identity required either way).
   **High-pitch-burst metric (ZCR-based, defined here)**: partition left-channel PCM into 4,096-sample blocks; per block compute integer mean m, x̃=x−m; `ZCR = #{i≥1 : (x̃[i−1]<0) ≠ (x̃[i]<0)} / 4095` (zero counts as non-negative); `AC_RMS = floor(sqrt(Σx̃²/4096))`. A block is a **burst block** iff `ZCR ≥ 0.70 AND AC_RMS ≥ 2,500` (the bound derives from §2.4 row p=0/1: peak ≤2,500 ⇒ RMS < 2,500; a 20.7 kHz tone has ZCR ≈ 2f/fs ≈ 0.94, music < ~0.4). Oracle: **zero burst blocks post-fix**. Discrimination proof: the developer runs the identical metric against the pre-fix build ONCE during S3 bring-up and records the measured baseline (DEC-0043 measured RMS up to 17,533 ⇒ burst blocks present) in the implementation report — the metric must be shown to fire pre-fix, not assumed.
5. **Chip-level integration cases (additive)** in rows 11–12 of §2.5: dwell hand-oracles (period-1 at W=81: per-window dwell 33..48 → exact sample sequence), constant fixed-point property, envelope-mid-window segmenting, the rounding helper's exact behavior at ±ties, W=0 guard.

### 2.7 Evidence & A/B plan (Defect A)

- **A/B vs openMSX (behavior-affecting ⇒ required)**: NEW `tools/openmsx-m34-aleste-audio-ab.ps1` + a small Python analyzer in `tools/`. Side B: real openMSX on WSL runs Aleste 2 with a scripted, deterministic input reaching the title→weapon-select transition, `record sound` capturing the mix to WAV (A-M34-3). Side A: our post-fix capture over the same transition. Comparator: the §2.6.4 metric applied to both — **the reference must show zero burst blocks in the transition window** (proving real-lineage emulation renders this idiom silent) and our post-fix capture must match that disposition. This is a *structural* A/B (silence-property parity), not sample-level — recorded as such in `docs/m34-audio-ab.md`.
  Fallback if `record sound` proves impractical in WSL (verify first; the m32 Tcl-injection mechanism `tools/openmsx-m32-split-ab.ps1` is the scripting precedent): record the honest limitation and rest the disposition on (a) the already-verified register-stream parity class, (b) the §2.1 hardware physics (fact-sheet YM2149 + the real 112 kHz degenerate), and (c) openMSX's native-rate+resampler architecture (§2.2 citations) — never fabricate an audio capture.
- **WAV evidence**: re-capture `debug/sounds/m32-fm-aleste-fmON/fmOFF.wav` on the recorded procedure at the new pipeline; supersession notes; re-verify the FM-divergence property (peak/mean figures re-measured, not carried forward). m27 example WAV per Q2.
- The DEC-0043 `--aleste-beep-probe` was temporary and already reverted; any new probe follows the same revert-after-capture precedent.

---

## 3. Defect B — R#1 BL (display-enable) ignored by the background renderer

### 3.1 Root cause and grounding (all files read this cycle)

- `src/devices/video/vdp_frame_renderer.cpp` contains **zero** reads of `control_register(1)` (verified by grep this cycle: the only `0x40` literals are sprite CC bits and palette masks at :17/:24/:352/:373). Content renders regardless of BL.
- `src/devices/video/sprite_engine.cpp:90-94` honors the same bit (`display_enabled = control_regs[1] & 0x40`) — the internal asymmetry proving the gap.
- **openMSX (primary)**: `PixelRenderer.cc:608-611` — when `!displayEnabled` the ENTIRE line (0..TICKS_PER_LINE) is drawn `DRAW_BORDER`; `:580-584` — sprite checking only under `displayEnabled`; `:159-163` `updateDisplayEnabled` syncs-before-change. `VDP.cc:435-442` `execSetBlank` derives `displayEnabled` from `controlRegs[1] & 0x40`; `VDP.cc:1080-1082` — an R#1 bit6 change schedules `syncAtNextLine(syncSetBlank)` (`VDP.cc:1260-1269`): the change takes effect at the **next line** — corroborating our M32 L+1 latch exactly, at line granularity.
- **fMSX (independent second reference)**: `MSX.h:216` `#define ScreenON (VDP[1]&0x40)`; every per-line refresh in `Common.h` (:463, :497, :533, :569, :601, :643, :690, :725, :768, :815, :850, :884) and `Wide.h` (:79, :113, :147) begins `if(!ScreenON) ClearLine(P, <background color>)` — the line filled with the backdrop/background color.
- **The two references AGREE** (BL=0 ⇒ active area shows backdrop; effect from the next line) — no disagreement to arbitrate for the gate itself.
- History: present since M20 snapshot rendering; M32's per-line renderer exposed it (DEC-0043).

### 3.2 Fix contract

**One gate, at the top of `VdpFrameRenderer::render_line()`** (`vdp_frame_renderer.cpp:397-413` — the single per-line workhorse shared by the legacy snapshot `render_frame()` at :415-427 AND `VdpScanlineAccumulator::sync_to_line()/compose()/finalize()` at `vdp_scanline_accumulator.cpp:57, :113, :120, :143`):

```
if ((vdp_->control_register(1) & 0x40) == 0):  fill out with border_color(); return
```

- **Fill color = `border_color()`** (the mode-aware backdrop, `vdp_frame_renderer.cpp:224-…`, grounded in VDP.hh:211-226 getBackgroundColor) — matching PixelRenderer.cc:608-611 DRAW_BORDER and fMSX's ClearLine(background). **NOT `render_blank()`** (:539-…): that is the *undefined-mode palette-15 fallback* (CharacterConverter.cc:368-373), a different semantic. The charter's "check how backdrop_color()/render_blank relate and reuse" resolves to: reuse `border_color()`.
- Early return skips content dispatch AND `composite_sprites()` AND the R#25 MSK step — the composed BL=0 line is **pure backdrop** (sprites off, per PixelRenderer.cc:580-584; consistent with sprite_engine.cpp:90-94).
- **L+1 latch comes free**: the existing M32 write-hook (`hbf1xv_machine.cpp:34-54`) commits `[watermark, L+1)` from PRE-write state before ANY VDP io_write mutates registers; `render_line` reads R#1 live → a BL write at display line L takes effect exactly from L+1, matching VDP.cc:1080-1082/1260-1269 at line granularity. Mid-*line* precision is the pre-existing D8 remainder (cross-note).
- Legacy snapshot semantics stay self-consistent: a whole-frame snapshot with BL=0 renders every line backdrop (what real hardware shows for a full BL=0 frame).
- Disclosed carry-overs: GRAPHIC5 single border color (§1.2.5); BL=1-mid-frame sprite-table liveness (D9 class, §1.2.4).
- **No new state, no new clock consumer, no seam changes** — this is a read of an existing live register inside an existing per-line function.

### 3.3 In-milestone verification (REQUIRED before the fix is judged done — S4 precedes S5 judgment)

1. **Metal Gear probe (side A)**: extend the `tools/capture-metalgear-evidence.ps1` lineage — the developer designs a deterministic extended input script (`HBF1XV-INPUT-SCRIPT v1`, cycle-stamped key holds) that walks Snake through a door after the recorded f1100 gameplay state. A temporary probe (m31/m32 probe precedent: temporary CLI flag, REVERTED after capture) logs every R#1 write as `(frame, display line, old→new bit6)` around the room change, plus frame dumps bracketing the transition. Artifact: `debug/logs/m34-mg-bl-probe.txt` + frames; probe reverted (`git diff` proof in the report).
2. **openMSX A/B (side B)**: NEW `tools/openmsx-m34-mg-bl-ab.ps1` — the SAME scripted transition on real openMSX/WSL (Tcl `keymatrix` injection per the verified m32 mechanism; A-M32-3 precedent), logging R#1 writes via the openMSX debug interface and capturing screenshots inside the blank window. Structural comparison: does the reference show a backdrop wipe during the BL=0 window (expected), and does its window match ours in frame extent?
3. **Three-outcome honest disposition (pre-agreed, no overfit to the report)**:
   - (a) Game blanks; reference shows backdrop wipe → fix corroborated end-to-end; record PARITY-class evidence.
   - (b) Game blanks only part of the window / reference also displays a progressive-fill phase → scope the *symptom attribution* to exactly what the reference corroborates; the render gate itself still ships (it is reference-proven independent of MG, §3.1); record the partial disposition.
   - (c) Game never sets BL=0 in the transition → the MG symptom is NOT Defect B's symptom; the gate STILL ships as a proven universal correctness gap (the sprite/background asymmetry is real), and the un-resolved MG symptom is escalated to the coordinator as a new triage item — never silently claimed fixed.

### 3.4 Hard regression oracles (Defect B)

1. **Committed frame evidence byte-identity**: every committed PNG in the M31/M32 evidence matrix stays byte-identical (they are believed all-BL=1 — **verified, not assumed**, per A-M34-2: re-run the recorded recipes post-fix; the byte-identity sweep is the gate and a per-recipe R#1 probe is the explanation). Any divergence ⇒ escalate with evidence (M32 AC-5/AC-6 pattern); rebaselining is forbidden.
2. **Static-frame equivalence extended**: `tests/unit/devices/video_vdp_scanline_accumulator_unit_test.cpp` matrix gains BL=0 configurations — accumulator vs legacy snapshot stays byte-identical (holds by construction since the gate lives in the shared `render_line`; proven, not assumed).
3. **Renderer unit oracle (additive)**: for a representative mode set (TEXT2, GRAPHIC4, GRAPHIC5, GRAPHIC7, YJK), BL=0 ⇒ every pixel == `border_color()`; with sprites configured and visible under BL=1, the BL=0 arm asserts pure backdrop (no sprite pixels). BL=1 arms byte-match today's output.
4. **L+1 latch integration test**: NEW `tests/integration/machine/hbf1xv_m34_bl_latch_integration_test.cpp` — drive R#1 bit6 through the REAL seam (machine bus `io_write` to #99, not `debug_io_write`) while the raster sits at display line L: rendered frame shows content rows `[0..L]` and backdrop rows `[L+1..]` exactly; the re-enable direction (bit6 set at line M) shows backdrop `[0..M]`, content `[M+1..]`; a `debug_io_write` arm confirms the documented seam exclusion (legacy projection). Grounding cited in-test: VDP.cc:1080-1082/:1260-1269, PixelRenderer.cc:608-611, fMSX MSX.h:216 + Common.h:463.
5. **Determinism**: two-run identity inside the new integration test.

---

## 4. Dependency map

| Layer | Touched | Files |
|---|---|---|
| `src/core/` | **NO** (tripwire) | — |
| `src/devices/cpu/` | **NO** (tripwire) | — |
| `src/devices/audio/` | YES (A) | `psg_ym2149.{h,cpp}`, `scc_wavetable.{h,cpp}` — additive integration accumulators/APIs; point APIs byte-kept |
| `src/devices/video/` | YES (B) | `vdp_frame_renderer.{h,cpp}` — the BL gate in `render_line()`; accumulator/seam files expected UNTOUCHED |
| `src/frontend/` | YES (A) | `psg_audio_pump.cpp` (+header comment), `machine_audio_mixer.{h,cpp}` (SCC term), `psg_audio_dump.*` comments; `audio_pacer.*`, `sdl3_audio_presenter.*` byte-untouched (verify by path-scoped git diff) |
| `src/machine/` | expected NO | write-hook seam already delivers L+1; any exception is a scope escalation |
| `src/peripherals/` | NO | — |
| `tests/` | YES | per §2.5/§2.6/§3.4 + `tests/CMakeLists.txt` registrations |
| `tools/`, `docs/`, `debug/` | YES | 2 new A/B scripts + analyzer, probe artifacts, evidence re-captures, reports |

Slice dependencies: S1→S2→S3 (audio chain); S4→S5 (B: probe/arbitration before fix judgment); S3/S5→S6. S4 may run in parallel with S1–S3.

---

## 5. Slices

### S1 — Audio survey confirmation + integration-point contract (no behavior change)
Scope: confirm the §2.3 contract against the live tree; write the chip-header contract blocks (boundary convention §2.3.3, rounding §2.3.4, the §2.4 honest-response table); implement the shared rounding helper + its unit cases; land the additive chip APIs with chip-level unit cases (§2.6.5) while ALL production consumers still point-sample (suites stay green unchanged).
Acceptance criteria:
- AC-S1.1: contract blocks in both chip headers include the §2.4 table and the disclosed-simplification statement (sinc rolloff + imperfect stopband, E-series pointer).
- AC-S1.2: fixed-point property (constant ⇒ integrated == point) unit-proven for PSG and SCC.
- AC-S1.3: fast subsets green both configs, all pre-existing tests byte-untouched at this point; zero cpu/core diffs (path-scoped git diff recorded).

### S2 — Band-limited production switch + oracle re-baseline
Scope: switch `PsgAudioPump::pump_one_sample()` and the mixer SCC term to integrated samples; execute the §2.5 disposition table row by row (R rows re-derived with dwell math in comments BEFORE running; G rows re-grounded; A rows audited with recorded results); the §2.5 final consumer-grep sweep.
Acceptance criteria:
- AC-S2.1: every §2.5 row's disposition executed and individually recorded in the implementation report (table mirrored with outcomes).
- AC-S2.2: zero-source oracles (M29 zero-SCC, M31 zero-FM nullptr + silent-attached) GREEN with meaning intact: a silent/absent source contributes exactly 0.
- AC-S2.3: rows 5/8/9 (loudness ratio, m29 system, m31 FM system) pass UNMODIFIED, or any motion is treated as an integration bug (stop, fix, never rebaseline these).
- AC-S2.4: fast subsets green both configs; `audio_pacer.*`/`sdl3_audio_presenter.*`/`ym2413_*` path-scoped diffs EMPTY.

### S3 — New audio oracles + Aleste regression capture + audio A/B + evidence
Scope: §2.6 oracles 1–4 (ultrasonic-silence family, mid-band fidelity, determinism, the Aleste system regression with the defined ZCR metric incl. the recorded pre-fix discrimination baseline); `tools/openmsx-m34-aleste-audio-ab.ps1` + analyzer + `docs/m34-audio-ab.md`; WAV evidence re-captures with supersession notes (§2.5 rows 15–16 per Q2 ruling).
Acceptance criteria:
- AC-S3.1: ultrasonic-silence oracle green with the §2.4 bound table asserted verbatim (p=0..4 + the 1 kHz control arm).
- AC-S3.2: mid-band fidelity oracle green (peak exactly 12,400; transition-sample confinement; RMS ±1%).
- AC-S3.3: Aleste regression: pre-fix metric baseline MEASURED and recorded (burst blocks present); post-fix ZERO burst blocks over frames 2150–2350; two-run byte identity.
- AC-S3.4: audio A/B disposition recorded honestly (parity of the silence property, or the documented fallback with the A-M34-3 finding).
- AC-S3.5: fmON/fmOFF pair re-captured with supersession notes; FM-divergence property re-measured on the new bytes.

### S4 — BL verification probe (Metal Gear script + openMSX A/B arbitration)
Scope: §3.3 items 1–2; the three-outcome disposition recorded BEFORE S5 is judged; probe reverted.
Acceptance criteria:
- AC-S4.1: deterministic extended MG input script committed under `tools/` (recorded-recipe discipline); R#1 transition log captured on our side.
- AC-S4.2: openMSX same-script A/B executed; reference-side R#1 log + blank-window captures (or an honestly recorded blocker).
- AC-S4.3: outcome (a)/(b)/(c) explicitly declared with evidence; for (b)/(c) the symptom attribution is re-scoped/escalated per §3.3.3 — never overfit.
- AC-S4.4: probe reverted (path-scoped git diff proof).

### S5 — BL render fix + oracles
Scope: the §3.2 one-gate fix; §3.4 oracles 2–5; the §3.4.1 committed-evidence byte-identity sweep with the A-M34-2 BL=1 verification.
Acceptance criteria:
- AC-S5.1: BL=0 line == pure `border_color()` in every tested mode, sprites asserted absent; BL=1 arms byte-identical to pre-fix output.
- AC-S5.2: L+1 latch integration test green in both directions via the real seam; `debug_io_write` exclusion arm green.
- AC-S5.3: accumulator static-frame equivalence green INCLUDING the new BL=0 configs.
- AC-S5.4: committed-PNG matrix byte-identical (recipes re-run); any divergence escalated with evidence, not rebaselined.
- AC-S5.5: fast subsets green both configs.

### S6 — Gates, ledger, docs, QA handoff
Scope: evidence gates (`tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `tools/bootstrap-build.ps1 -RunTests` both configs — fast subset ONLY, `-Slow` not authorized); ledger updates (§8); `docs/m34-implementation-report.md`; state-file updates; QA dispatch.
Acceptance criteria:
- AC-S6.1: all gates run and captured verbatim (never fabricated); headless + SDL3-ON fast subsets green (expected 177+new / 186+new).
- AC-S6.2: zero `src/devices/cpu/` + `src/core/` diffs proven by path-scoped git diff; ZEXALL recorded as correctly NOT run.
- AC-S6.3: ledger rows/cross-notes from §8 landed; supersession notes present for every re-captured artifact; universal-fix audit (grep for title/SHA conditionals in changed paths) recorded.
- AC-S6.4: implementation report contains the §2.5 executed table, both A/B dispositions, the S4 outcome declaration, and the honest §2.4 limitation statement.

---

## 6. Test-matrix additions (complete)

| # | Test (target name indicative) | Type | Config | Oracle | Slice |
|---|---|---|---|---|---|
| 1 | `audio_psg_ym2149_unit_test` (extended) | unit | both | dwell hand-oracles, fixed-point property, envelope mid-window, W=0 guard | S1 |
| 2 | `audio_scc_wavetable_unit_test` (extended) | unit | both | per-channel dwell average, stopped-channel hold, enable gating in integral | S1 |
| 3 | shared rounding helper cases (in #1 or a small dedicated unit) | unit | both | exact behavior incl. ± ties | S1 |
| 4 | `frontend_psg_audio_pump_unit_test` (re-derived) | unit | both | re-derived window-average sequences + idle case | S2 |
| 5 | `frontend_psg_audio_dump_unit_test` (re-derived) | unit | both | W=1 per-cycle-level oracle | S2 |
| 6 | `frontend_machine_audio_mixer_unit_test` (re-grounded) | unit | both | zero-SCC meaning + constant cases + determinism | S2 |
| 7 | `frontend_machine_audio_mixer_fm_unit_test` (re-grounded) | unit | both | zero-FM (nullptr + silent) meaning + clamp + 9:8 | S2 |
| 8 | `frontend_machine_audio_mixer_ultrasonic_silence_unit_test` (NEW) | unit | both | §2.4 bound table p=0..4 + 1 kHz control + SCC arm + determinism | S3 |
| 9 | `frontend_machine_audio_mixer_midband_fidelity_unit_test` (NEW) | unit | both | exact peak 12,400 + transition confinement + RMS ±1% + determinism | S3 |
| 10 | `hbf1xv_m34_aleste_ultrasonic_regression_system_test` (NEW) | system | both | zero burst blocks (§2.6.4 metric) frames 2150–2350 + two-run identity | S3 |
| 11 | `video_vdp_scanline_accumulator_unit_test` (extended) | unit | both | static-frame equivalence with BL=0 configs | S5 |
| 12 | renderer BL unit arm (extend M21 render unit or NEW `video_vdp_frame_renderer_bl_unit_test`) | unit | both | BL=0 pure backdrop per mode + sprites-absent + BL=1 byte-match | S5 |
| 13 | `hbf1xv_m34_bl_latch_integration_test` (NEW) | integration | both | L+1 latch both directions via real seam + debug-seam exclusion + determinism | S5 |
| 14 | Committed-evidence byte-identity sweep | manual gate (S6 report) | — | PNG matrix byte-identical; escalate-never-rebaseline | S5/S6 |

All ctest additions are fast (seconds-class); #10 may restrict audio production to the frame window to stay well under a minute (recorded choice). Asset-dependent tests follow the existing `set_asset_root(SONY_MSX_BIOS_DIR)`/ROM-path pattern (DEC-0016 watch-item 3; m31 metalgear2 precedent).

## 7. Risks

| # | Risk | Sev | Mitigation / disposition |
|---|---|---|---|
| R-M34-1 | Post-fix Aleste residual (~2,190 RMS at 20.7 kHz) still faintly audible to the human | Med | Honest §2.4 prediction recorded; pre-named escalation = cascaded second box pass (sinc², still integer/deterministic, −3.4 dB @15 kHz droop) — NOT built speculatively; coordinator arbitrates only if the human reports it |
| R-M34-2 | p=2–4 partial suppression (audible aliases at −15…−7 dB) if some title parks tones there | Med | Disclosed limitation (§2.4) + E-series ledger row (§8); oracle asserts the honest bounds, not a false silence claim; Q1 |
| R-M34-3 | Oracle re-baseline breadth (16 enumerated surfaces) | High (discipline) | §2.5 table with per-row disposition + executed-table mirror in the report + final consumer-grep sweep (AC-S2.1) |
| R-M34-4 | Row 9/10 tests secretly embed exact PCM (audit rows) | Low | A-dispositions audited with recorded outcomes; any motion in "U expected" rows = integration bug, stop |
| R-M34-5 | MG A/B outcome (b)/(c): game doesn't blank as reported | Med | Three-outcome disposition pre-agreed (§3.3.3); gate ships on reference grounding regardless; symptom escalated honestly |
| R-M34-6 | A committed PNG was captured at BL=0 (A-M34-2 false) | Low | Byte-identity sweep + escalate-with-evidence (AC-6 pattern); never rebaseline |
| R-M34-7 | openMSX WSL `record sound` impractical | Low | §2.7 documented fallback; finding recorded, never fabricated |
| R-M34-8 | Integration accumulators cost per sample | Low | ~3 mul-adds per 16-cycle PSG step + per-channel SCC walks; suites' wall-times compared pre/post in the report |
| R-M34-9 | Boundary-convention re-derivations done by running the code first (tautology) | Med | R-rows require hand dwell math in comments authored before execution; QA can mutate to check discriminancy |
| R-M34-10 | Scope creep into machine seam / D8 sub-line depth | Low | §4 expects `src/machine/` untouched; any exception = decision-channel entry first |

## 8. Ledger / closure notes (rows to open or annotate at closure)

1. **NEW E-series row (coordinator assigns ID; suggested "E4 — true band-limited audio resampling depth")**: supersede the M34 box filter with genuine band-limited synthesis/resampling (BLEP/polyphase-class; openMSX ResampledSoundDevice/BlipBuffer is the behavior reference, never the code source). Covers: p=2–4 partial stopband (§2.4), sinc passband droop, and (cross-note) the FM 9:8 ZOH decimation. Trigger: any playtest report of residual whistles/dullness.
2. **Cross-note on D8** (sub-line accuracy): BL latch is line-granular (L+1); mid-line BL transitions are D8 depth.
3. **Cross-note on D9** (per-line live sprite fetch): BL=1-mid-frame sprite re-enable uses the frame-boundary visibility table until D9 lands.
4. If S4 lands outcome (c): NEW triage row for the unexplained MG transition symptom.
5. Evidence supersessions recorded (fmON/fmOFF pair; m27 WAV per Q2 ruling).

## 9. Open questions needing coordinator arbitration BEFORE developer dispatch

- **Q1 — Box-filter partial suppression at PSG periods 2–4** (§2.4): the charter's "period 1-4 family → near-zero" is *not* what the exact math yields for p=2/4 (aliases at 11.7/16.2 kHz at ≤19%/≤46% of channel amplitude). Recommended: accept the charter-recommended single box as-is, oracle the honest bounds, open the E-series row (§8.1). Alternative: authorize the cascaded double-box now (kills p=1 to −36 dB, p=2 to −29 dB; costs −3.4 dB @15 kHz droop + a second re-derivation pass on the R-rows). **Planner recommendation: single box + ledger row.**
- **Q2 — `debug/sounds/m27-example-tone.wav` disposition** (§2.5 row 16): regenerate-with-supersession (consistent with the M32 evidence precedent; the dump tool's output changes) vs freeze-as-historical-M27-artifact. **Planner recommendation: regenerate with a supersession note.** (The m32 fmON/fmOFF pair re-capture, row 15, is treated as required either way — it is the live FM-evidence lineage.)
- **Q3 — none for the BL gate**: both references agree (§3.1); the three-outcome S4 disposition is pre-agreed in this package and needs no advance ruling.

## 10. Exit criteria (milestone)

1. All S1–S6 acceptance criteria met; fast subsets green in BOTH configs from the ONE canonical `build/` via `tools/bootstrap-build.ps1` (`-Slow` not run).
2. §2.5 executed-disposition table + §2.6/§3.4 oracles green; zero-source byte-identity oracles intact in meaning; committed-PNG matrix byte-identical (or escalated per pattern).
3. Both A/B dispositions recorded (`docs/m34-audio-ab.md`, MG BL A/B in the implementation report or `docs/m34-bl-ab.md`); S4 outcome declared.
4. Evidence gates captured (assets, checksums, build, ctest); zero cpu/core diffs proven; universal-fix audit recorded.
5. `docs/m34-implementation-report.md` handed to QA; channels/state updated; QA sign-off is the closure gate (tag v1.0.35 only after it, per protocol).
