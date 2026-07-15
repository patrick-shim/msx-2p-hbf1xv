# M31 Planner Package — YM2413 FM-Synthesis DSP Depth (backlog E1) — RELEASE CANDIDATE

- Milestone ID: M31
- Title: YM2413 (OPLL) FM-synthesis DSP depth — the formulaically-derivable subset (backlog E1),
  plus the Release-Candidate gate for the DEC-0035 autonomous run
- Tag target: v1.0.32
- Spec Owner: MSX Planner Agent (this package)
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Date: 2026-07-09
- Authority: DEC-0035 (three-milestone autonomous run; M31 = E1 scoped subset as RELEASE
  CANDIDATE; ZEXALL/ZEXDOC sweep at M31's QA gate with a durable log), DEC-0037 (M30 closed,
  "Next and final in the run: M31"), `agent-protocol/state/deferred-backlog.md` row E1
  (DEFERRED → M31, carrying `docs/m28-planner-package.md` §2.3(a)'s verbatim scoping caveat).
- Baseline at kickoff: M30 closed (DEC-0037, tag v1.0.31). Headless fast subset 163/163;
  SDL3-ON fast subset 172/172. ZEXALL last ran clean at v1.0.28 (146/146 full).

---

## 1. Scope and Assumptions

### 1.1 Objective

Give the already-shipped, QA-signed YM2413 register contract (M17,
`src/devices/audio/ym2413_opll.{h,cpp}`) a real, deterministic, formula-grounded FM waveform
synthesis engine, mix it as the THIRD audio source through M29's
`frontend::MachineAudioMixer` (PSG + SCC + now FM), and close the DEC-0035 run with the full
Release-Candidate QA gate. Every synthesis element in scope is grounded in
`references/fact-sheets/Yamaha YM2413 FM Chip.md` (independent grounding: Yamaha Application
Manual + andete hardware measurements + community-standard tables, per the sheet's own §2/§9
citations) — never in transcribed reference-emulator table values.

### 1.2 Ratified scope boundary (M28 §2.3(a) → DEC-0035 — build on it, do not relitigate)

IN (the formulaically-derivable subset):

1. Log-sin (256-entry, 12-bit) and exp (256-entry, 10-bit) operator tables COMPUTED BY FORMULA
   at construction (fact-sheet §4 structural description; the classic Yamaha logarithmic
   operator). openMSX corroborates that these tables are closed-form-computable — its own
   NukeYKT core computes them via `constexpr` log2/sin/exp2 lambdas at
   `references/openmsx-21.0/src/sound/YM2413NukeYKT.cc:56-71` (verified by direct read this
   cycle). We RE-DERIVE the formulas from the fact-sheet's structure; we never transcribe
   values (identical numeric OUTPUT from an independently-grounded closed form is expected and
   acceptable — the M28 §2.3(a) finding says exactly this).
2. The 15+3 ROM instrument patch table — ALREADY SHIPPED in M17 from the fact-sheet §4
   community-standard table (`Ym2413Opll::rom_patch()` / `rhythm_*_patch()`). REUSED, not
   re-derived, not re-litigated.
3. Envelope generator decay/release per the fact-sheet §5's OWN formula and mechanism:
   128 levels over 48 dB (0.375 dB/step); effective 6-bit rate = 4·R + Rks; rates 0-3 = no
   change; 60-63 = 2 levels/sample; otherwise `eg_shift = 13 - (rate/4)`, `eg_select = rate&3`
   with the four 8-entry select tables printed in §5 ({0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1},
   {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}); ONE global counter shared by all 18 operators
   (deliberately preserving §5's audible "first decay segment after key-on is typically
   shorter" consequence); overall timing anchored by §5's exact closed form
   `cycles = (rate<60) ? (1<<(14-(rate/4)))·s[rate&3] : 63`, s = {127,102,85,73}, ×(72/3579545 s).
4. Phase generators per §8: 9-bit F-Num, 3-bit BLOCK, MUL per the §3 table (0→½, 1→1, …,
   E→15, F→15), phase increment `ΔP = F-Num · 2^BLOCK · MUL(/2)` into a 19-bit accumulator,
   top 10 bits index the sine (top 2 bits mirror the 256-entry first-quadrant table);
   f_Hz = F-Num · 2^BLOCK · MUL_eff · 49716 / 2^19 (corroborated independently by fMSX — see
   §2.8 disagreement D-M31-1 for its 50000-vs-49716 constant).
5. Modulator→carrier FM (single fixed algorithm, §4), modulator self-feedback from the
   AVERAGE OF THE LAST TWO modulator samples (§5, die-confirmed 2-deep delay), FB[2:0] as an
   exponential shift spanning modulation index 0, π/16 … 4π (§3) — property: each FB step
   DOUBLES the modulation index (unit-testable without any golden table).
6. Key-on/off, SUS (release rate forced to 5 at key-off when SUS=1, §3), EG-TYP
   sustained/percussive (§3), KSR with Rks derived as (BLOCK·2 + F-Num[8]) for KSR=1 (the only
   natural 0-15 construction from §3's "block/F-num offset… full offset 0-15"; >>2 when KSR=0
   per "KSR=0 gives 0-3") — labeled A-M31-3 below.
7. AM/VIB LFOs — IN as DISCLOSED, FORMULA-CONSTRAINED APPROXIMATIONS (decision per the §1.2
   check the deferral instructed): §5 gives shape, depth and rate for both. AM: triangle,
   14 levels, 4.8 dB depth — implement as a 0..13-step triangle of 0.375 dB EG-steps
   (13 × 0.375 = 4.875 ≈ 4.8 dB, internally consistent with §5's own EG step) at a documented
   counter rate within ±5% of the measured ~3.7 Hz. VIB: 8-step, zero-mean, triangle-shaped
   pitch offset, peak ≈ ±14 cents — ±14 cents ≈ ±fnum/128, i.e. peak offset ±(F-Num >> 7)
   (a derivation from §5's own depth figure and "indexed by top 3 bits of F-Num"), at a
   documented rate within ±5% of ~6.1 Hz. Exact hardware LFO counter widths and the exact 8×8
   PM table VALUES are NOT independently available → named in the E3 remainder (§6.3).
8. Rhythm-mode synthesis to the depth the fact-sheet's own §6 supports: the slot commitment
   (ch6-8), BD = normal 2-op FM (full synthesis), TOM = 1-op (full synthesis), SD/HH/T-CY =
   disclosed noise/square approximations (§6 names "1-bit noise + square, phase-generator
   override" for SD and only "special synthesis" for HH/T-CY — no derivable formula exists in
   the sheet); HH+SD share ch7 frequency, TOM+T-CY share ch8 frequency (§6); $0E bit-keying and
   $36-$38 nibble volumes via the existing M17 accessors; the DOUBLE-OUTPUT quirk as a ×2
   rhythm contribution (§6 quirk 1 / §7: "output twice… effectively +6 dB after LPF").
   The noise source is a documented, standard maximal-length LFSR (developer's documented
   choice of width/taps) — exact OPLL LFSR taps are not in the fact-sheet → E3 remainder.
9. Output-level handling grounded in §4/§7: total attenuation composed in exp-index units
   (1 unit = 3/128 dB; §4's own `expTable[logsin[phase] + 128·volume + 16·envelope]`
   composition, extended per §3's unit-consistent TL = 32 units/step and KSL = 64/128/256
   units/octave), and the andete-measured per-volume peak-amplitude series in §7
   ({255,180,127,90,63,45,31,22,15,11,7,5,3,2,1,1}) usable as an independent TEST ORACLE for
   the 3 dB/step volume law (each step ≈ ×1/√2). Digital summing of channels (disclosed
   divergence from the adder-less TDM DAC, §7) with a documented mixer scale factor.

NAMED REMAINDER (cannot be built — license wall; recorded at closure as new row E3, §6.3):
the ATTACK-curve EG data. Verified by direct read this cycle:
`references/openmsx-21.0/src/sound/YM2413NukeYKT.cc:106-114` pulls `attack[14][4][64]`,
`releaseIndex`, `releaseData` from the pre-generated GPL
`references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii`; the fact-sheet §5 explicitly says
"Attack times are separately tabulated" without reproducing them. No independent source exists
in this repository. M31 therefore ships the DISCLOSED ATTACK POLICY of §2.4.

### 1.3 Assumptions (each with a verification action)

- **A-M31-1**: `bios/f1xvmus.rom` (slot 3-3 FM-MUSIC ROM) carries the internal-MSX-MUSIC
  "APRLOPLL" ID at ROM offset 0x18-0x1F (maps to CPU 0x4018, fact-sheet §2), so MSX-MUSIC
  titles will detect FM on this machine. Planner evidence: a binary grep for `APRLOPLL|OPLL`
  MATCHES in that file. Verification: developer confirms the 8 bytes at file offset 0x18 via
  PowerShell before relying on real-title FM evidence.
- **A-M31-2**: Aleste 2 (`roms/aleste.rom`, DB-identified, boots since M29/M30) is an
  MSX-MUSIC-capable title and will write YM2413 key-ons when internal MSX-MUSIC is detected.
  UNVERIFIED belief about third-party software. Verification: S6's FM-activity probe (count
  $10-$38 data-port writes + key-on edges during a scripted boot/attract run). The RC criterion
  (§4, item 7) has an honest fallback if NO available title exercises FM.
- **A-M31-3**: Rks = (BLOCK·2 + F-Num[8]) when KSR=1, >>2 when KSR=0 — the only natural 0-15
  construction satisfying §3's stated ranges. Verification: semantics cross-checked against
  both behavior references' KSR handling (READ for semantics only; no table transcription);
  disagreement, if any, recorded per DEC-0030.
- **A-M31-4**: phase accumulator is 19-bit with the top 10 bits indexing the sine
  (§8 says "~18-19-bit"; fMSX's frequency constant independently implies /2^19 — §2.8
  D-M31-1). Documented width choice; pitch oracle test (§3, S2) empirically pins it.
- **A-M31-5**: key-on resets the operator phase accumulator and enters attack from the current
  attenuation. The fact-sheet does not state phase-reset behavior either way; this is a
  documented modeling choice (the real chip's damp-then-attack key-on sequence is NukeYKT-era
  knowledge without independent grounding here → E3 remainder). Deterministic either way.
- **A-M31-6**: `roms/metalgear2_scc.rom` (added 2026-07-09, coordinator-registered,
  softwaredb SHA1 78560a5c2e6de02896a56c5662b1aaccaf9d386f = "Metal Gear 2 - Solid Snake",
  KonamiSCC) is SCC-only (no FM-PAC usage) — it serves the RC's real-SCC-audio evidence
  (closing M29's disclosed A-M29-4 residual), NOT the FM evidence. Verification: the S6
  FM-activity probe run on MG2 should show zero key-ons (a free negative control).

### 1.4 Non-goals (explicit)

- NO external Panasonic FM-PAC cartridge device (bank register/8 KB SRAM/7FF4-7FF7 overlay) —
  this machine's device is `MSXMusic`-style, settled in M17 (DEC-0012); unchanged.
- NO cycle-exact NukeYKT parity claim of any kind; no audio-sample A/B vs openMSX (§2.7).
- NO band-limited resampling (§2.5's ZOH/decimation policy is the disclosed simplification).
- NO TDM per-slot micro-timing (we compute per native sample, not per 4-master-cycle slot).
- NO change to the E2 write-timing gate default (stays OFF) or semantics.
- NO state-dump/debug-CLI extension for FM internals this cycle (M25 watch-item 15 remains a
  future candidate).
- NO touch to `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`,
  `src/frontend/psg_*` (psg_ym2149 lives in devices; the frontend psg_audio_pump.* /
  psg_audio_dump.* files are equally out of bounds), `src/devices/audio/scc_wavetable.*`,
  and the four existing YM2413 test files (§3.4 regression contract).

---

## 2. Spec Summary

### 2.1 Grounding table (every element → its independent source)

| Element | Grounding | Class |
|---|---|---|
| logsin/exp tables | fact-sheet §4 (structure: 256-entry 12-bit log-sin, 256-entry 10-bit exp; `expTable[logsin + 128·vol + 16·env]` composition); formulas re-derived: `logsin[i] = round(-log2(sin((i+0.5)·π/512))·256)`, `exp[i] = round((2^(i/256))·1024)`-family with the classic 8-bit-fraction + shift lookup; corroborated closed-form-computable by YM2413NukeYKT.cc:56-71 | EXACT (formula) |
| Patch table 15+3 | fact-sheet §4 table (shipped M17, `Ym2413Opll::rom_patch`) | EXACT (already precedented) |
| EG decay/release | fact-sheet §5 formula + select tables + global counter | EXACT (formula) |
| EG attack | fact-sheet §5 qualitative ("rises exponentially"; 0-3 = ∞; 60-63 ≈ 0) + §2.4 policy | DISCLOSED APPROXIMATION |
| Phase gen / MUL | fact-sheet §8 + §3 MUL table; A-M31-4 width | EXACT (formula; width documented) |
| Feedback | fact-sheet §5 (2-sample average) + §3 (exponential index ladder) | EXACT mechanism; amplitude mapping documented |
| KSR/Rks | fact-sheet §3 ranges; A-M31-3 construction | DERIVED (labeled) |
| KSL | fact-sheet §3 dB/oct rates; log2-of-pitch formula with documented threshold | DISCLOSED APPROXIMATION (breakpoints → E3) |
| AM LFO | fact-sheet §5 (triangle, 14 levels, 4.8 dB, ~3.7 Hz) | EXACT shape/depth; rate counter documented |
| VIB LFO | fact-sheet §5 (~6.1 Hz, ≈14 cent, top-3 F-Num bits) | DISCLOSED APPROXIMATION (exact table → E3) |
| Rhythm slots/keys/volumes | fact-sheet §6 + M17 accessors | EXACT |
| BD/TOM synthesis | fact-sheet §6 (BD normal 2-op; TOM "essentially 1-op") | EXACT structure |
| SD/HH/T-CY | fact-sheet §6 (no derivable formula) | DISCLOSED APPROXIMATION (noise/square; exact phase-override → E3) |
| Noise LFSR | standard maximal-length LFSR, documented choice | DISCLOSED APPROXIMATION (exact taps → E3) |
| Double-output quirk | fact-sheet §6/§7 (×2 ≈ +6 dB) | EXACT (as a gain law) |
| Native rate 49716 Hz | fact-sheet §7 (3.579545 MHz / 72) | EXACT |
| Volume law oracle | fact-sheet §7 measured per-volume peaks | EXACT (independent measurement, used as TEST oracle) |
| Write-path/register file | M17 contract + M28 E2 gate — UNCHANGED | shipped |

### 2.2 Architecture and placement

- New files `src/devices/audio/ym2413_synth.{h,cpp}`: the synthesis engine (18 operator
  states, EG, phase, LFOs, rhythm, native-sample generation). Pure, deterministic,
  self-contained; no clock attachment; advanced only by explicit
  `advance_cycles(delta)` calls (the exact `SccWavetable` precedent,
  `src/devices/audio/scc_wavetable.h:110-113`).
- `src/devices/audio/ym2413_opll.{h,cpp}` extended ADDITIVELY: owns a `Ym2413Synth`,
  forwards accepted (post-E2-gate, post-mask) register writes to it, exposes
  `advance_cycles(std::uint64_t)` and `[[nodiscard]] std::int32_t fm_sample() const`.
  The M17 register contract (two-port protocol, decode accessors, reset, open-bus reads,
  debug readback) is UNCHANGED; `reset()` additionally clears synth state. The synth may
  genuinely REUSE the M17 decode helpers (`patch()`, `f_number()`, `block()`, `key_on()`,
  rhythm accessors) as its register view — no duplicated decode.
- `src/frontend/machine_audio_mixer.{h,cpp}` extended ADDITIVELY: a new overload
  `mix_interleaved_stereo(psg, sccs, Ym2413Opll* fm, sample_count)`; the existing 3-arg
  overload delegates with `fm = nullptr` and stays byte-behavior identical (M29's unit tests
  untouched). Mixing law extends the documented M29 pattern:
  `pcm = clamp_int16(psg_raw·kPsgAmplitudeScale + scc_sum·kSccAmplitudeScale + fm·kFmAmplitudeScale)`.
  FM is mono → both channels, like SCC. `kFmAmplitudeScale` is a documented presentation
  policy (same disclosed class as 400/12): with per-channel peak ≈ ±255 (§7 oracle) and a
  realistic loud mix of ~6 melody + rhythm ×2, the planner suggests a starting value of 4-6
  with the int16 clamp REQUIRED and unit-tested at a constructed saturation input; §7's own
  "blend ratio varies by machine" note is the honest cover for this being a policy, not a
  measured constant.
- `src/frontend/sdl3_audio_presenter.*` / `Sdl3App`: pass `&machine.ym2413()`
  (`src/machine/hbf1xv_machine.h:209-210`) into the mixer path. `AudioPacer` and
  `PsgAudioPump` remain byte-for-byte untouched (DEC-0033/M29 discipline; the pacer decides
  HOW MANY samples, the mixer produces them).
- Headless evidence path: the S6 system test and/or an additive headless mode writes raw
  mixed PCM; `tools/audio-dump-to-wav.py` converts (existing M27 tool); committed WAV(s)
  under `debug/sounds/`.

### 2.3 Determinism contract

No wall clock anywhere; all state advanced by emulated cycles; `reset()` restores a
documented power-on state; two identical runs produce byte-identical sample streams
(dedicated test). The global EG counter is part of documented state.

### 2.4 THE ATTACK POLICY (decision + justification — prominently disclosed)

**Decision: option (a) — a documented exponential-approach approximation.** At each EG
advance event granted by the SAME fact-sheet-grounded global-counter/eg_shift/eg_select
mechanism used for decay/release, an attacking operator's attenuation moves toward 0
exponentially: `att -= (att >> k) + 1` per granted step (developer finalizes and documents
k; the +1 floor guarantees termination), with effective-rate 0-3 → no change (∞) and 60-63 →
instant 0, both per §5's own statements.

Justification: (i) the fact-sheet itself gives the qualitative SHAPE ("Attack rises
exponentially", §5) plus both endpoint behaviors — the approximation is grounded, only the
exact per-rate step data is not; (ii) reusing the §5 global-counter mechanism keeps KSR/Rks
and the "first segment shorter" phase-dependence quirk uniform across all EG phases;
(iii) option (b) (instant attack) would be audibly harsher and would misrepresent
slow-attack ROM patches (e.g. Violin/Flute carrier ARs of 7/6 in the §4 table audibly ramp);
(iv) the approximation is strictly NOT claimed cycle-exact — disclosed in a mandatory header
comment block in `ym2413_synth.h` naming `YM2413NukeYktTables.ii` as the inaccessible
source, in the implementation report, and on the E3 backlog row (§6.3). QA verifies the
disclosure exists in all three places.

### 2.5 THE RESAMPLING POLICY (native 49716 Hz → 44100 Hz pipeline)

**Decision: native-rate generation + zero-order-hold decimation, all in machine-cycle
domain.** `Ym2413Opll::advance_cycles(delta)` accumulates master cycles and executes one
native FM sample tick (all 18 operators) per 72 cycles (§7: 3.579545 MHz / 72 = 49716 Hz),
holding the latest native sample; `fm_sample()` returns the held value. The mixer advances
all sources by the same `cycles_per_sample = 81` per output sample (M29 pattern).

Consequences (documented honestly, the DEC-0033 precedent):
- **Pitch is EXACT in machine time** — FM samples are generated every 72 machine cycles, so
  FM frequencies carry NO device-specific offset (unlike the PSG's disclosed -3.6-cent
  81-vs-81.17-cycle rounding, which is a PSG-internal stepping convention — NOT "fixed" here,
  DEC-0033 untouched). The only global rate mapping is the pipeline-wide cycles→host-sample
  convention every source already shares.
- **Deterministic 9:8 decimation**: 8 output samples × 81 cycles = 648 = 9 × 72 — exactly
  nine native ticks per eight output samples, an exact repeating pattern; ZOH effectively
  drops one of every nine native samples. Disclosed simplification: no band-limited
  resampling; ZOH imaging/fold-down artifacts accepted and documented (same disclosed class
  as M26's amplitude policy). E3 records "proper resampling" as an optional quality item ONLY
  if ever demanded — it is not a hardware-fidelity gap of the same kind as the tables.

### 2.6 Register→synthesis event flow

Accepted data writes (post-gate, post-latch-mask — the M17/M28 path, unchanged) additionally
notify the synth: key-on/off edge detection per channel from $20-$28 bit4 and per drum from
$0E bits 0-4 (rhythm mode per $0E bit5, melody channels 6-8 suppressed while rhythm is on,
§3/§6); everything else (patch, F-Num, volume, SUS…) is read live from the register file via
the M17 decode accessors at synthesis time. Instrument changes on a keyed channel take
effect immediately (register-file-as-truth; documented).

### 2.7 openMSX A/B disposition (decided in advance, M29 §2.5 / E2-M28 precedent)

- **Audio-sample A/B: N/A BY DESIGN.** openMSX runs the cycle-exact GPL NukeYKT core with the
  very attack/rate tables this project may not possess; a live sample diff would show
  DIVERGENCE BY DESIGN, not a defect. Recorded in `docs/m31-parity-trace-diff.md` with this
  reasoning verbatim; does not gate closure.
- **CPU-visible surface: no new A/B required.** The YM2413 is write-only with no readback or
  status (fact-sheet §8; `io_read` = 0xFF, A-M17-5) — synthesis adds ZERO CPU-visible state.
  The M17 A/B (register-file + architectural parity, both PASS,
  `docs/m17-parity-trace-diff.md`) remains the standing evidence for the whole CPU-visible
  contract, which this milestone must not change (§3.4). If the developer touches ANY
  CPU-visible path (they should not need to), a bounded architectural A/B re-run becomes
  mandatory.
- The mechanism-vs-livediff pattern: correctness is proven by fact-sheet-formula oracles
  (§3), exactly as E2 (M28) proved its gate against cited constants with A/B honestly N/A.

### 2.8 fMSX cross-reference findings (DEC-0030 obligations — recorded now, in-code at S2)

`references/fmsx-60/source/EMULib/YM2413.c` was READ this cycle. Headline finding: **fMSX
contains NO operator-level OPLL DSP at all** — it maps OPLL channels to abstract
`Sound()`/`Drum()` MIDI-style calls (`Patches2413[]`/`Drums2413[]` instrument shims,
YM2413.c:22-54; its 19×16 `Synth2413[]` parameter blob is `#if 0`-disabled, :59-101). Its
value as a second reference here is register/frequency semantics only. Disagreements to
record in-code and arbitrate per DEC-0030:

- **D-M31-1 (frequency constant)**: fMSX computes `Freq = (3125·Frq·(1<<Oct))>>15` Hz
  (YM2413.c:193-195, 212-214) = F-Num·2^BLOCK·50000/2^19 — i.e. it approximates the sample
  rate as exactly 50000 Hz. Fact-sheet §7/§8 and openMSX both give 3.579545 MHz/72 =
  49716 Hz. **Arbitrated to the fact-sheet (49716)**; fMSX's constant is a rounding
  simplification (~10 cents sharp). Bonus: fMSX independently corroborates the /2^19
  accumulator scaling (A-M31-4).
- **D-M31-2 (address-latch masking)**: fMSX masks at LATCH time (`Latch = V&0x3F`,
  YM2413.c:134-137); openMSX/this project (M17, A-M17-3) latch UNMASKED and mask at use.
  Observationally equivalent for all sequences; already settled in M17 — no change; recorded.
- **D-M31-3 (rhythm-off key-bit clearing)**: fMSX forces $0E bits 0-4 to 0 whenever bit5=0
  (YM2413.c:170) — a register-file mutation neither the fact-sheet nor openMSX supports.
  **Arbitrated to fact-sheet/openMSX**: registers store what is written; drum key bits simply
  have no effect while rhythm mode is off.

---

## 3. Milestones (slice plan, build order)

### S1 — Operator tables + fixed-point spec
Formula-computed logsin/exp tables at construction; a written internal fixed-point spec
(attenuation in 3/128 dB exp-index units; 19-bit phase; documented operator output width).
**Tests (license-safe oracle discipline — applies to every slice)**: expected values
recomputed INDEPENDENTLY inside the test via double-precision math + rounding (never a
golden array copied from anywhere); endpoint/monotonicity/symmetry properties; exp∘logsin
round-trip sanity (reconstructed sine within quantization bounds of sin()).

### S2 — Phase generation + single-operator evaluation + FM coupling
MUL table (§3, ×2-integer form to carry the ½), 19-bit accumulator, 10-bit sine index with
quadrant mirroring, DC/DM half-sine rectification, TL/VOL/KSL/AM attenuation composition
(§4 law), modulator→carrier phase modulation, 2-deep feedback average + FB exponential
ladder. In-code recording of D-M31-1..3. **Tests**: pitch oracle — measured period of a
generated carrier-only tone vs f = F-Num·2^BLOCK·MUL_eff·49716/2^19 across representative
(F-Num, BLOCK, MUL) triples incl. MUL=0 (½); volume law vs the §7 measured-peaks oracle
(ratio ≈ 1/√2 per step within quantization tolerance); FB doubling property; half-sine
rectification (negative half → silence floor).

### S3 — Envelope generator
Global 18-operator counter; decay/release EXACT per §5 (formula + select tables + 0-3/60-63
specials); SL boundary (3 dB/step from top); EG-TYP; SUS→RR=5; KSR/Rks (A-M31-3); the §2.4
attack approximation with its mandatory disclosure comment block. **Tests**: decay/release
duration oracles computed from §5's closed form (exact expected native-sample counts, not
tolerances); first-segment-shorter quirk (same key-on at two different global-counter phases
→ different first-segment lengths, deterministic both); attack monotonicity + termination +
rate-ordering (higher AR strictly not slower) + AR 60-63 instant + AR 0-3 never.

### S4 — Channels, rhythm mode, LFOs
9 melody channels; rhythm mode per §1.2 item 8 (BD 2-op, TOM 1-op, SD/HH/T-CY disclosed
approximations, shared ch7/ch8 frequencies, $0E keying, $36-$38 volumes, ×2 double-output);
AM/VIB per §1.2 item 7. **Tests**: rhythm ×2 gain law; drum keying independence; melody
ch6-8 silenced in rhythm mode; Yamaha-recommended rhythm setup ($16=20h/$17=50h/$18=C0h,
$26/$27/$28 per §6) produces non-silent, deterministic output for each drum; AM depth ≈
4.875 dB peak-to-trough on a held tone; VIB peak offset = documented ±(F-Num>>7) law.

### S5 — Device/mixer/frontend integration
`Ym2413Opll::advance_cycles`/`fm_sample` (72-cycle native tick + ZOH per §2.5); mixer
overload + `kFmAmplitudeScale` + clamp; presenter wiring; headless PCM evidence path.
**Tests**: 9:8 decimation pattern (648-cycle repeat) unit-proven; **the zero-YM2413 hard
byte-identity oracle** — with `fm == nullptr` AND separately with the device attached but
never keyed (silent output path must contribute exactly 0), mixer output is byte-identical
to the v1.0.31 3-arg arithmetic for ANY input sequence (the M29 zero-SCC oracle pattern,
reference side = the literal pre-M31 arithmetic); clamp saturation test; two-run stream
determinism through the machine.

### S6 — System evidence + THE RC GATE + ledger
Register-driven FM system test (real CPU `OUT (#7C)/(#7D)` over the M11 bus: key-on → pitch
measured against the S2 oracle at machine level, key-off → release-bounded decay to
silence, rhythm burst; two-run byte-identity). FM-activity probe on real titles (A-M31-1/2/6
verifications). The full RC matrix of §4. Ledger edits (§6). Implementation report +
`docs/m31-parity-trace-diff.md` + committed WAV/PNG evidence.

Dependency map: S1→S2→S3→S4 strictly ordered inside `src/devices/audio/`;
S5 touches `src/devices/audio/ym2413_opll.*` + `src/frontend/machine_audio_mixer.*` +
presenter; S6 touches `tests/system/`, `docs/`, `debug/`, ledger. `src/machine/` expected
untouched (accessor already exists); `src/core|cpu|peripherals` untouched by constraint.

---

## 4. Acceptance Criteria (RC gate — ALL must hold; never fabricate, capture everything)

1. **Synthesis correctness (formula-oracle-proven)**: every S1-S5 test class above green;
   all expected values derived from fact-sheet formulas or recomputed independently in-test;
   NO golden table transcribed from `references/` anywhere in src/ or tests/.
2. **License-sensitive-scope compliance (absolute)**: no numeric-table VALUES from
   `YM2413NukeYktTables.ii`, `YM2413NukeYKT.cc`, fMSX, or any reference appear in the tree
   (the fact-sheet's own printed tables/constants and the already-shipped M17 patch table are
   the only permitted literals, each cited). QA performs an explicit source-form review: the
   only literal arrays in the new code are the §5 select tables, the §3 MUL table, the §7
   measured-peaks TEST oracle, and documented-choice constants — no attack/release step
   tables, no 8×8 PM values, no LFSR tap sets claimed as hardware-exact.
3. **Attack/approximation disclosures present** in all three mandated places (§2.4), plus
   equivalents for VIB/KSL/SD/HH/T-CY/LFSR/ZOH (§2.1's "DISCLOSED APPROXIMATION" rows).
4. **Regression contract**: the four existing YM2413 test files
   (`tests/unit/devices/audio_ym2413_opll_unit_test.cpp`,
   `tests/unit/devices/audio_ym2413_write_timing_unit_test.cpp`,
   `tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp`,
   `tests/integration/machine/hbf1xv_m28_ym2413_write_timing_integration_test.cpp`) and the
   M29 mixer unit test remain byte-for-byte UNMODIFIED and green; `git diff v1.0.31` EMPTY
   for `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`,
   `src/frontend/psg_audio_pump.*`, `src/frontend/psg_audio_dump.*`,
   `src/devices/audio/psg_ym2149.*`, `src/devices/audio/scc_wavetable.*`, and all 12 named
   CPU-timing-oracle test files. E2 gate still defaults OFF.
5. **Zero-YM2413 byte-identity hard oracle** (S5) green — the prior audio pipeline is
   provably unperturbed when FM is absent/silent.
6. **RC full-suite evidence**: (a) headless config: FULL UNFILTERED `ctest` (INCLUDING
   `hbf1xv_m24_zexall_system_test` — the DEC-0035 RC-checkpoint gate; the
   `feedback_slow_test_cadence.md` fast-cadence rule explicitly does NOT apply at this gate)
   — ZEXALL+ZEXDOC must report the invariant 67/67 groups, 0 error markers, with the
   **durable log copied to `docs/m31-rc-zexall-log.txt` and committed** (build/ logs get
   overwritten — a permanent path is mandatory); (b) SDL3-ON config: full fast suite green
   under dummy drivers; (c) exact pass counts recorded in the implementation report
   (baselines: 163 headless / 172 SDL3-ON, plus this milestone's new tests).
7. **RC smoke matrix (each item = a captured artifact, PNG under `debug/frames/m31-rc-*` or
   WAV under `debug/sounds/m31-*`, produced via the real frame loop
   (`step_cpu_instruction()` + `on_vsync_boundary()`), scripted input where needed)**:
   - (a) Real BIOS cold boot to the MSX-BASIC prompt with the MSX2+ boot-logo phase intact
     (DEC-0031 regression check).
   - (b) `roms/metalgear.rom` auto-identifies via DB ("Metal Gear", Konami, no type flag),
     boots, and reaches gameplay/attract under scripted input.
   - (c) `roms/aleste.rom` auto-identifies (KonamiSCC via DB, no type flag) and boots
     (M30's own end-to-end re-confirmed on the RC build).
   - (d) **NEW (coordinator, 2026-07-09)**: `roms/metalgear2_scc.rom` auto-identifies via DB
     SHA1 78560a5c2e6de02896a56c5662b1aaccaf9d386f ("Metal Gear 2 - Solid Snake", KonamiSCC,
     no type flag) and boots to its title under the real frame loop; during title music the
     SCC genuinely produces samples through `MachineAudioMixer` — evidence: a deterministic
     headless capture in which the SCC contribution is demonstrably non-zero (e.g. mixed
     stream differs byte-wise from a PSG-only mix of the same run, or an SCC sample()/
     register-activity assertion). **This closes M29's disclosed A-M29-4 residual (real SCC
     title evidence) — record that on/next to the G1 row.** Note MG2 is SCC-only (A-M31-6):
     it is NOT the FM evidence.
   - (e) MSX-DOS disk boot: unattended cold boot with `disks/msxdos22.dsk` reaches the Disk
     BASIC prompt (the C5/DEC-0034 closure evidence re-confirmed on the RC build).
   - (f) **FM audible in a real title**: run the FM-activity probe on `roms/aleste.rom`
     (A-M31-1/A-M31-2). If key-ons are observed → capture a WAV segment demonstrating a
     non-silent FM contribution (FM-included mix differs from FM-muted mix of the identical
     deterministic run). If NO available title exercises FM (honest possible outcome) → the
     fallback evidence is (i) a committed synthetic FM demo WAV (deterministic register
     program, e.g. a §4-patch melody + §6 rhythm bar) + (ii) the S6 register-driven system
     test — explicitly disclosed as synthetic in the report. Either outcome satisfies this
     criterion; fabricating neither does.
8. **RC health re-check** (M28 audit quick items): no new untracked TODO/FIXME (grep diff vs
   v1.0.31); both executables launch (`--help`/minimal run, exit 0); `tools/validate-assets.ps1`
   green; `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed (now
   including `roms/metalgear2_scc.rom`); both configs build clean.
9. **A/B disposition artifact**: `docs/m31-parity-trace-diff.md` records §2.7's dispositions
   (audio-sample N/A BY DESIGN with reasoning; CPU-visible surface unchanged with the git-diff
   proof; conditional bounded A/B only if a CPU-visible path was touched).
10. **Ledger/protocol**: §6's backlog edits applied same-cycle; channels appended (ISO-8601);
    `state/current-phase.md`/`state/milestones.md` refreshed; implementation report
    `docs/m31-implementation-report.md` is the QA handoff artifact.

Exit criteria: QA sign-off per the DEC-0035 pattern (Conditional Pass → fix-re-confirm-then-
proceed; only genuine blockers stop the run), then coordinator release decision, tag v1.0.32.

---

## 5. Risks (with verification actions)

- **R-M31-1 (transcription temptation — HIGHEST GOVERNANCE RISK)**: the developer will read
  NukeYKT/fMSX for semantics while implementing next to a file
  (`YM2413NukeYktTables.ii`) that contains exactly the missing data. Mitigation: acceptance
  criterion 2's source-form review; the developer NEVER opens `YM2413NukeYktTables.ii` (the
  M30 SHA1.c discipline — record the non-opening in the report); QA re-verifies.
- **R-M31-2 (fixed-point under-specification)**: §8's "~18-19-bit" hedge and unstated
  operator-output widths force documented choices; wrong choices → audible pitch/level bugs.
  Mitigation: A-M31-4 + the S2 pitch/volume oracles pin the observable behavior to fact-sheet
  formulas regardless of internal width; every width choice gets a written rationale.
- **R-M31-3 (real-title FM assumption fails)**: Aleste 2 may not exercise FM (A-M31-2) or
  detection may fail (A-M31-1). Mitigation: probe FIRST (cheap), fall back to the disclosed
  synthetic-demo path already accepted by criterion 7(f); if detection fails on a title KNOWN
  to support MSX-MUSIC, treat as a real defect hunt (slot/ID-string path) before falling back.
- **R-M31-4 (mixer clipping/balance)**: a third source raises saturation risk and the FM:PSG
  balance is a policy (§7's "varies by machine"). Mitigation: clamp unit test, documented
  scale, WAV listening evidence; zero-FM oracle guarantees the quiet path.
- **R-M31-5 (RC gate skipped by habit)**: the standing fast-cadence rule has skipped ZEXALL
  since v1.0.28; M31 is the explicit RC exception (DEC-0035). Mitigation: criterion 6 names
  the full unfiltered run + durable committed log as a hard gate; QA must independently see
  the log.
- **R-M31-6 (performance)**: 49716 native ticks/s × 18 operators is trivial for modern hosts,
  but the SDL3 real-time loop should be sanity-checked once with FM active (no starvation);
  LOW.
- **R-M31-7 (approximation over-claiming)**: reports/comments accidentally calling attack/
  VIB/rhythm-approx "accurate". Mitigation: criterion 3's disclosure checklist; QA reads the
  header block verbatim.
- **R-M31-8 (scope creep toward E3)**: mid-implementation temptation to "quickly add" damp
  phase, exact PM tables, TDM serialization. Mitigation: §1.4 non-goals + E3 row exists to
  receive them; universal-fixes-only discipline continues.

---

## 6. Backlog re-affirmation (full-ledger pass, per DEC-0005) + E1 closure shape

### 6.1 Full-row re-affirmation (35 rows at kickoff)

- **DONE rows — re-affirmed unchanged**: B1-B9; C2, C3, C4, C5 (DEC-0034), C6, C7, C8, C9;
  D1, D2, D3, D5, D6, D7; E2; G1 (M29), G2 (M30). M31 adds to G1's row a factual
  cross-reference note at closure: A-M29-4's "no real SCC game ROM" residual is RESOLVED by
  `roms/metalgear2_scc.rom` + RC criterion 7(d) evidence (no status change — G1 stays DONE).
- **C1/D4**: UNBUILDABLE-WITHOUT-FABRICATION ruling (M28) stands untouched; still OPEN /
  IN-PROGRESS (M23 partial), no owner. M31 touches nothing in that territory.
- **C10 → M32-era, F1 → M33-era, F2 → M34-era**: numeric-owner shift notes stand; final
  numbers at their own kickoffs (DEC-0035). Unchanged this cycle.
- **G3, G4, G5**: OPEN, unchanged. (G5/SCC-I remains unscheduled; MG2 is a plain KonamiSCC
  title and does not require SCC-I.)
- **E1**: closes THIS milestone — see §6.2.

### 6.2 E1 closure shape — RECOMMENDATION: **DONE (M31) with a named-remainder split-out (new row E3)**

Precedent-matched to C7→F1/F2 and G1→G5 (split-out, not partial close), NOT to C5's
stay-IN-PROGRESS shape, because: (1) DEC-0035 ratified the formulaically-derivable subset AS
M31's deliverable, so delivering it completes the ratified milestone scope in full; (2) every
element of E1's own row text (18-slot-equivalent 9-channel pipeline @ native 49716 Hz,
logsin+exp operator, 128-level EG with the global-counter mechanism, 2-deep feedback
averaging, AM/VIB LFOs, rhythm + double-output quirk, output-level nonlinearity) ships in a
grounded form — exact where a formula exists, disclosed-approximate where only qualitative
grounding exists; (3) the remainder is a C1/D4-CLASS SOURCING BLOCKER (needs a NEW FACT — an
independent attack-table source), not "more work" — leaving E1 IN-PROGRESS would imply an
achievable completion path that does not currently exist, which is exactly the situation the
C1/D4 pattern (open, unowned, explicitly unscheduled) was designed to represent honestly.

### 6.3 New row E3 (draft text — developer records verbatim-equivalent at closure, Section D)

> **E3 | YM2413 cycle-exact synthesis residuals (license/sourcing-blocked remainder of E1)**
> — (1) ATTACK-curve EG data (exact per-rate attack steps; sole in-repo source is the GPL
> `references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii`, per YM2413NukeYKT.cc:106-114 —
> M31 ships a disclosed exponential-approach approximation instead); (2) the EG rate-52-59
> anomaly's 16-entry select tables (fact-sheet §5/§9 names the anomaly, values not
> reproduced); (3) exact VIB 8×8 PM table values + exact LFO counter widths (§5 gives
> rate/depth/shape only); (4) exact rhythm noise-LFSR width/taps and the HH/SD/T-CY
> phase-generator override formulas (§6 names them without formulas); (5) exact KSL base-curve
> breakpoints; (6) key-on damp-phase behavior; (7) TDM per-slot DAC serialization + per-code
> DAC nonlinearity beyond §7's per-volume peaks (511 codes, missing -256, NMOS crossover);
> (8, optional-quality, not fidelity-blocked) band-limited 49716→44100 resampling replacing
> M31's disclosed ZOH. | **OPEN** | M31 (named remainder split-out) | **No owner — items 1-7
> need an independent source (Yamaha datasheet excerpt, real-hardware measurement set, or a
> new fact-sheet section) before any milestone may responsibly schedule them; the C1/D4
> standard applies** | YM2413 fact-sheet §5/§6/§7/§9; YM2413NukeYKT.cc:106-114;
> `feedback_license_sensitive_scope.md`.

---

## 7. Developer Handoff (dispatch summary)

Implement S1→S6 in order. Hard rules: (1) formulas only — never open
`YM2413NukeYktTables.ii`, never transcribe table values from openMSX or fMSX (source-form
review is acceptance criterion 2); (2) the no-touch list of §1.4/criterion 4 is absolute;
(3) every approximation carries its §2.4/§2.1-mandated disclosure; (4) the RC gate of §4 runs
IN FULL, including the unfiltered ZEXALL sweep with the durable committed log
(`docs/m31-rc-zexall-log.txt`) and the 5-title/disk smoke matrix with committed artifacts;
(5) ledger edits per §6 (E1 → DONE (M31), new row E3, G1 cross-reference note) same-cycle;
(6) `docs/m31-implementation-report.md` is the QA handoff; record exact test counts, the
FM-activity probe outcomes for aleste/metalgear/metalgear2_scc (A-M31-1/2/6), and the
non-opening attestation for the .ii file. Deviations that change scope go through
`agent-protocol/channels/decisions.md` first.
