# M34 Audio A/B — Aleste 2 Ultrasonic-Silence Property vs openMSX (DEC-0043 Defect A)

- Milestone: M34 (docs/m34-planner-package.md §2.7); harness `tools/openmsx-m34-aleste-audio-ab.ps1`
  + analyzer `tools/m34-audio-ab-analyze.py`.
- Executed: 2026-07-09, real openMSX 19.1 on WSL (`/usr/bin/openmsx`, machine `Sony_HB-F1XV`),
  power-cycle-fresh session.
- **Disposition: PARITY (silence property).** This is a STRUCTURAL A/B — the compared property is
  the §2.6.4 high-pitch-burst metric's verdict, never sample-level bytes (the two pipelines have
  different resamplers and gains by design).

## What was compared

Aleste 2's title → weapon-select → game-start transition parks tone period 0 on all three PSG
channels with tones enabled and volumes gated 7–15 (DEC-0043 triage; re-confirmed live this
cycle: our probe read `R0..R5=0, R7=0xB8, R8=12, R9=11..12, R10=0` at frames 2200/2250). On a
real YM2149 that is a ~112 kHz square — inaudible, analog-smoothed to its mean. The M34 question:
does each emulator render this idiom SILENT in the audible band?

Metric (§2.6.4, identical implementation on both sides): 4,096-sample blocks; integer mean
removed; burst block iff ZCR ≥ 0.70 (flips ≥ 2867/4095) AND AC_RMS ≥ 2,500 (int16 scale).

## Side A (ours, post-M34 box-average integration)

`hbf1xv_m34_aleste_ultrasonic_regression_system_test` (the committed ctest oracle; recorded m32
smoke recipe, KonamiSCC explicit, SPACE holds at frames 600/1500/2100, window frames 2150–2350):

```
LEFT  summary: blocks=35 burst_blocks=0 max_zcr_flips=906  max_ac_rms=5065
RIGHT summary: blocks=35 burst_blocks=0 max_zcr_flips=3839 max_ac_rms=2619
All Machine_Hbf1xvM34AlesteUltrasonic_System cases passed
```

Zero burst blocks. The 20.7 kHz residual survives only in ZCR character (flips 3839 = 0.937) at
RMS ~400–500 (the §2.4-predicted suppressed residual; the high-RMS blocks are ordinary music at
ZCR < 0.25 — never coincident with high ZCR).

**Recorded discrimination baseline (pre-fix, measured once during S3 bring-up on the
point-sampling build — the metric fires, proven not assumed):**

```
LEFT  summary: blocks=35 burst_blocks=0  max_zcr_flips=3839 max_ac_rms=5073
RIGHT summary: blocks=35 burst_blocks=17 max_zcr_flips=3839 max_ac_rms=5874
  (17 BURST blocks, ZCR up to 0.937 at AC_RMS 4,186..5,874, frames ~2189-2262)
```

(The burst manifests on the RIGHT side in this trajectory: R10=0 mutes channel C so right = A
alone carries the full alias, while left = A+B is a phase-opposed pair that mostly cancels —
the committed test meters BOTH sides, a recorded strengthening of the package's left-only
clause.)

## Side B (real openMSX 19.1, WSL, headless `record start -audioonly`)

A-M34-3 VERIFIED: openMSX records its audio mix headlessly to WAV with no host audio device
(mono, 16-bit, 44.1 kHz). SPACE injected via `keymatrixdown`/`keymatrixup` (row 8 bit 0) at
emulated-time 10 s/25 s/35 s (~frames 600/1500/2100); recording window 33–46 s spans the
transition. (Bring-up finding, recorded honestly: the first attempt used a wrong two-word
`keymatrix down` command name — the Tcl errored, no presses landed, and the capture was
all-zero loader silence; the metric verdict on such a capture is vacuous. Fixed to the
one-word commands and re-run; the capture below carries real game audio.)

```
wav: rate=44100 channels=1 frames=572416 (13.0 s)
channel 0: blocks=139 burst_blocks=0 max_zcr_flips=945 (zcr=0.231) max_ac_rms=3143
verdict: SILENT (zero burst blocks)
```

Real music present (max AC_RMS 3,143) at musical ZCR (max 0.231); NO high-ZCR content at any
amplitude — the reference's native-rate generation + band-limited resampling
(references/openmsx-21.0/src/sound/AY8910.cc:38-39,:482; ResampledSoundDevice.hh:23,29,46-48;
BlipBuffer.hh:1-28 — behaviour reference only, never copied) erases the ultrasonic idiom
entirely, where our single box average suppresses it to an inaudible-class ~20.7 kHz residual
(the disclosed §2.4 difference; backlog row E4 names the full band-limited depth).

## Verdict

Both sides render the Aleste-2 silence idiom with ZERO burst blocks: the reference corroborates
that real-lineage emulation makes this transition silent, and the M34 fix brings this project to
the same disposition. **PARITY (silence property).** Artifacts: `build/m34-ab/side_a_regression.log`,
`build/m34-ab/openmsx_aleste_transition.wav`, `build/m34-ab/side_b_metric.log` (local,
regenerable via the harness).
